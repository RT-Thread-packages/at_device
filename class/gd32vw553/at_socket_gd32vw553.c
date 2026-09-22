/*
 * Copyright (c) 2006-2026, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-09-22     CYFS         add GD32VW553 TCP socket support
 */

#include <string.h>

#include <at_device_gd32vw553.h>

#define LOG_TAG                         "at.skt.gd32vw553"
#include <at_log.h>

#if defined(AT_DEVICE_USING_GD32VW553) && defined(AT_USING_SOCKET)

#define GD32VW553_SEND_MAX_SIZE         2920
#define GD32VW553_RAW_RX_SIZE           512
#define GD32VW553_RAW_PROMPT_TIMEOUT    5000
#define GD32VW553_RAW_HANDOFF_WAIT_MS   20
#define GD32VW553_RAW_RX_POLL_MS        20
#define GD32VW553_RAW_EXIT_SILENCE_MS   2000
#define GD32VW553_RAW_EXIT_WAIT_MS      2000
#define GD32VW553_RAW_CMD_TIMEOUT_MS    3000
#define GD32VW553_RAW_CMD_BUF_SIZE      64
#define GD32VW553_CONNECT_TIMEOUT_MS    30000

struct gd32vw553_raw_context
{
    struct at_device *device;
    rt_thread_t thread;
    struct rt_semaphore rx_notice;
    struct rt_mutex rx_lock;
    struct rt_mutex tx_lock;
    rt_err_t (*at_rx_indicate)(rt_device_t dev, rt_size_t size);
    at_status_t saved_status;
    rt_bool_t active;
    rt_bool_t connected;
    rt_bool_t closing;
};

static struct gd32vw553_raw_context raw_context;

static at_evt_cb_t at_evt_cb_set[] =
{
    [AT_SOCKET_EVT_RECV] = RT_NULL,
    [AT_SOCKET_EVT_CLOSED] = RT_NULL,
#ifdef AT_USING_SOCKET_SERVER
    [AT_SOCKET_EVT_CONNECTED] = RT_NULL,
#endif
};

static void gd32vw553_urc_close(struct at_client *client, const char *data, rt_size_t size);
static rt_bool_t gd32vw553_parse_connect_ok(at_response_t resp);

static const struct at_urc urc_table[] =
{
    {"", ",CLOSED\r\n", gd32vw553_urc_close},
    {"", "CLOSED\r\n",  gd32vw553_urc_close},
};

static void gd32vw553_copy_value(char *dst, size_t dst_size, const char *src)
{
    size_t length;

    if (dst == RT_NULL || dst_size == 0 || src == RT_NULL)
    {
        return;
    }

    length = rt_strlen(src);
    if (length >= dst_size)
    {
        length = dst_size - 1;
    }
    rt_memcpy(dst, src, length);
    dst[length] = '\0';
}

static rt_err_t gd32vw553_raw_rx_indicate(rt_device_t dev, rt_size_t size)
{
    RT_UNUSED(dev);

    if (size > 0)
    {
        rt_sem_release(&raw_context.rx_notice);
    }
    return RT_EOK;
}

static void gd32vw553_raw_rx_entry(void *parameter)
{
    struct gd32vw553_raw_context *context =
        (struct gd32vw553_raw_context *)parameter;
    char data[GD32VW553_RAW_RX_SIZE];

    while (1)
    {
        rt_int32_t timeout = context->active ?
            rt_tick_from_millisecond(GD32VW553_RAW_RX_POLL_MS) :
            RT_WAITING_FOREVER;

        rt_sem_take(&context->rx_notice, timeout);
        rt_mutex_take(&context->rx_lock, RT_WAITING_FOREVER);

        while (context->active)
        {
            struct at_socket *socket;
            char *recv_buf;
            rt_size_t recv_size;

            recv_size = rt_device_read(context->device->client->device, 0,
                                       data, sizeof(data));
            if (recv_size == 0)
            {
                break;
            }

            recv_buf = (char *)rt_malloc(recv_size);
            if (recv_buf == RT_NULL)
            {
                LOG_E("no memory for receive buffer(%d).", recv_size);
                continue;
            }

            rt_memcpy(recv_buf, data, recv_size);
            socket = &(context->device->sockets[0]);
            if (at_evt_cb_set[AT_SOCKET_EVT_RECV] != RT_NULL)
            {
                at_evt_cb_set[AT_SOCKET_EVT_RECV](socket, AT_SOCKET_EVT_RECV,
                                                  recv_buf, recv_size);
            }
            else
            {
                rt_free(recv_buf);
            }
        }

        rt_mutex_release(&context->rx_lock);
    }
}

static int gd32vw553_raw_start(struct at_device *device)
{
    struct at_client *client = device->client;
    rt_tick_t start_tick;
    rt_base_t level;
    size_t error_index = 0;
    char flush_buf[16];
    char ch;
    int result = -RT_ETIMEOUT;

    rt_mutex_take(&raw_context.rx_lock, RT_WAITING_FOREVER);
    if (raw_context.active || raw_context.device != RT_NULL)
    {
        rt_mutex_release(&raw_context.rx_lock);
        return -RT_EBUSY;
    }

    raw_context.device = device;
    raw_context.saved_status = client->status;
    raw_context.at_rx_indicate = client->device->rx_indicate;
    client->status = AT_STATUS_CLI;

    level = rt_hw_interrupt_disable();
    rt_device_set_rx_indicate(client->device, gd32vw553_raw_rx_indicate);
    rt_hw_interrupt_enable(level);

    /* Let the AT parser finish any read that was signaled before the handoff. */
    rt_thread_mdelay(GD32VW553_RAW_HANDOFF_WAIT_MS);
    while (rt_device_read(client->device, 0, flush_buf, sizeof(flush_buf)) > 0)
    {
    }

    if (at_client_obj_send(client, "AT+CIPSEND\r\n", 12) != 12)
    {
        result = -RT_ERROR;
        goto __failed;
    }

    start_tick = rt_tick_get();
    while (rt_tick_get() - start_tick <
           rt_tick_from_millisecond(GD32VW553_RAW_PROMPT_TIMEOUT))
    {
        if (rt_device_read(client->device, 0, &ch, 1) != 1)
        {
            rt_thread_mdelay(1);
            continue;
        }

        if (ch == '>')
        {
            result = RT_EOK;
            break;
        }

        if (ch == "ERROR"[error_index])
        {
            if (++error_index == 5)
            {
                result = -RT_ERROR;
                break;
            }
        }
        else
        {
            error_index = (ch == 'E') ? 1 : 0;
        }
    }

    if (result != RT_EOK)
    {
        goto __failed;
    }

    rt_thread_mdelay(20);
    while (rt_device_read(client->device, 0, flush_buf, sizeof(flush_buf)) > 0)
    {
    }

    level = rt_hw_interrupt_disable();
    raw_context.active = RT_TRUE;
    rt_hw_interrupt_enable(level);

    rt_mutex_release(&raw_context.rx_lock);
    rt_sem_release(&raw_context.rx_notice);
    return RT_EOK;

__failed:
    level = rt_hw_interrupt_disable();
    rt_device_set_rx_indicate(client->device, raw_context.at_rx_indicate);
    client->status = raw_context.saved_status;
    rt_hw_interrupt_enable(level);
    raw_context.device = RT_NULL;
    rt_mutex_release(&raw_context.rx_lock);
    return result;
}

static void gd32vw553_raw_restore(struct at_device *device)
{
    struct at_client *client = device->client;
    rt_base_t level;

    level = rt_hw_interrupt_disable();
    rt_device_set_rx_indicate(client->device, raw_context.at_rx_indicate);
    client->status = raw_context.saved_status;
    rt_hw_interrupt_enable(level);

    raw_context.device = RT_NULL;
}

static void gd32vw553_raw_stop(struct at_device *device)
{
    raw_context.active = RT_FALSE;
    rt_sem_release(&raw_context.rx_notice);
    rt_mutex_take(&raw_context.rx_lock, RT_WAITING_FOREVER);
    gd32vw553_raw_restore(device);
    rt_mutex_release(&raw_context.rx_lock);
}

static void gd32vw553_raw_exec_cipmode_normal(struct at_device *device)
{
    struct at_client *client = device->client;
    static const char cmd[] = "AT+CIPMODE=0\r\n";
    char resp[GD32VW553_RAW_CMD_BUF_SIZE] = {0};
    rt_size_t resp_len = 0;
    rt_tick_t start_tick;
    char ch;

    if (at_client_obj_send(client, cmd, sizeof(cmd) - 1) != sizeof(cmd) - 1)
    {
        return;
    }

    start_tick = rt_tick_get();
    while (rt_tick_get() - start_tick <
           rt_tick_from_millisecond(GD32VW553_RAW_CMD_TIMEOUT_MS))
    {
        if (rt_device_read(client->device, 0, &ch, 1) != 1)
        {
            rt_thread_mdelay(1);
            continue;
        }

        if (resp_len < sizeof(resp) - 1)
        {
            resp[resp_len++] = ch;
            resp[resp_len] = '\0';
        }

        if (rt_strstr(resp, "OK") != RT_NULL ||
            rt_strstr(resp, "ERROR") != RT_NULL)
        {
            break;
        }
    }
}

static int gd32vw553_enter_transparent(struct at_device *device)
{
    at_response_t resp;
    int result;

    resp = at_create_resp(128, 0, 5 * RT_TICK_PER_SECOND);
    if (resp == RT_NULL)
    {
        return -RT_ENOMEM;
    }

    result = at_obj_exec_cmd(device->client, resp, "AT+CIPMODE=1");
    if (result == RT_EOK)
    {
        rt_thread_mdelay(100);
        result = gd32vw553_raw_start(device);
        if (result != RT_EOK)
        {
            at_obj_exec_cmd(device->client, RT_NULL, "AT+CIPMODE=0");
        }
    }

    at_delete_resp(resp);
    return result;
}

static int gd32vw553_exit_transparent(struct at_device *device)
{
    int result;

    if (!raw_context.active || raw_context.device != device)
    {
        return RT_EOK;
    }

    rt_mutex_take(&raw_context.rx_lock, RT_WAITING_FOREVER);
    raw_context.active = RT_FALSE;

    /* Keep +++ in a standalone UART packet as required by AN151. */
    rt_thread_mdelay(GD32VW553_RAW_EXIT_SILENCE_MS);
    if (at_client_obj_send(device->client, "+++", 3) != 3)
    {
        result = -RT_ERROR;
    }
    else
    {
        result = RT_EOK;
    }
    rt_thread_mdelay(GD32VW553_RAW_EXIT_WAIT_MS);

    gd32vw553_raw_exec_cipmode_normal(device);
    gd32vw553_raw_restore(device);
    rt_sem_release(&raw_context.rx_notice);
    rt_mutex_release(&raw_context.rx_lock);

    return result;
}

static int gd32vw553_socket_close(struct at_socket *socket)
{
    struct at_device *device = (struct at_device *)socket->device;
    at_response_t resp;
    int close_result = RT_EOK;
    int result;
    rt_bool_t close_needed;

    rt_mutex_take(&raw_context.tx_lock, RT_WAITING_FOREVER);

    if (!raw_context.connected &&
        (!raw_context.active || raw_context.device != device))
    {
        rt_mutex_release(&raw_context.tx_lock);
        return RT_EOK;
    }

    raw_context.closing = RT_TRUE;
    close_needed = raw_context.connected;
    raw_context.connected = RT_FALSE;
    result = gd32vw553_exit_transparent(device);

    resp = at_create_resp(64, 0, 3 * RT_TICK_PER_SECOND);
    if (resp != RT_NULL)
    {
        if (close_needed)
        {
            close_result = at_obj_exec_cmd(device->client, resp, "AT+CIPCLOSE");
        }
        at_delete_resp(resp);
    }
    else
    {
        close_result = -RT_ENOMEM;
    }
    rt_thread_mdelay(100);
    raw_context.closing = RT_FALSE;
    rt_mutex_release(&raw_context.tx_lock);

    return result != RT_EOK ? result : close_result;
}

static int gd32vw553_socket_connect(struct at_socket *socket, char *ip,
                                     int32_t port, enum at_socket_type type,
                                     rt_bool_t is_client)
{
    struct at_device *device = (struct at_device *)socket->device;
    at_response_t resp;
    int result;

    RT_ASSERT(ip);
    RT_ASSERT(port >= 0);

    if (!is_client)
    {
        return -RT_ERROR;
    }

    resp = at_create_resp(128, 1,
                          rt_tick_from_millisecond(GD32VW553_CONNECT_TIMEOUT_MS));
    if (resp == RT_NULL)
    {
        return -RT_ENOMEM;
    }

    if (type == AT_SOCKET_TCP)
    {
        result = at_obj_exec_cmd(device->client, resp,
                                 "AT+CIPSTART=\"TCP\",\"%s\",%d,0", ip, port);
    }
    else if (type == AT_SOCKET_UDP)
    {
        result = -RT_ENOSYS;
    }
    else
    {
        result = -RT_ERROR;
    }

    if (result == RT_EOK)
    {
        if (gd32vw553_parse_connect_ok(resp) == RT_FALSE)
        {
            result = -RT_ERROR;
        }
    }

    if (result == RT_EOK)
    {
        socket->user_data = (void *)0;
        raw_context.connected = RT_TRUE;
        if (type == AT_SOCKET_TCP)
        {
            result = gd32vw553_enter_transparent(device);
            if (result != RT_EOK)
            {
                gd32vw553_socket_close(socket);
            }
        }
    }

    at_delete_resp(resp);
    return result;
}

static int gd32vw553_socket_send(struct at_socket *socket, const char *buff,
                                  size_t bfsz, enum at_socket_type type)
{
    struct at_device *device = (struct at_device *)socket->device;
    size_t sent_size = 0;
    int result;

    RT_ASSERT(buff);
    RT_ASSERT(bfsz > 0);
    RT_UNUSED(type);

    rt_mutex_take(&raw_context.tx_lock, RT_WAITING_FOREVER);
    if (!raw_context.active || raw_context.device != device ||
        !raw_context.connected || raw_context.closing)
    {
        rt_mutex_release(&raw_context.tx_lock);
        return -RT_ERROR;
    }

    while (sent_size < bfsz)
    {
        size_t packet_size = bfsz - sent_size;

        if (packet_size > GD32VW553_SEND_MAX_SIZE)
        {
            packet_size = GD32VW553_SEND_MAX_SIZE;
        }
        if (at_client_obj_send(device->client, buff + sent_size, packet_size) != packet_size)
        {
            result = sent_size > 0 ? (int)sent_size : -RT_ERROR;
            rt_mutex_release(&raw_context.tx_lock);
            return result;
        }
        sent_size += packet_size;
    }

    rt_mutex_release(&raw_context.tx_lock);
    return (int)sent_size;
}

static rt_bool_t gd32vw553_parse_domain_ip(at_response_t resp, char ip[16])
{
    char parsed_ip[32] = {0};
    rt_size_t line_number;

    for (line_number = 1; line_number <= resp->line_counts; line_number++)
    {
        const char *line = at_resp_get_line(resp, line_number);

        if (line == RT_NULL || rt_strstr(line, "+CIPDOMAIN:") == RT_NULL)
        {
            continue;
        }

        if (at_resp_parse_line_args_by_kw(resp, "+CIPDOMAIN:",
                                          "+CIPDOMAIN:<\"%31[^\"]\">", parsed_ip) > 0 ||
            at_resp_parse_line_args_by_kw(resp, "+CIPDOMAIN:",
                                          "+CIPDOMAIN:\"%31[^\"]\"", parsed_ip) > 0 ||
            at_resp_parse_line_args_by_kw(resp, "+CIPDOMAIN:",
                                          "+CIPDOMAIN:<%31[^>]>", parsed_ip) > 0 ||
            at_resp_parse_line_args_by_kw(resp, "+CIPDOMAIN:",
                                          "+CIPDOMAIN:%31s", parsed_ip) > 0 ||
            rt_sscanf(line, "+CIPDOMAIN: %31s", parsed_ip) == 1)
        {
            ip_addr_t address;

            if (inet_aton(parsed_ip, &address) != 0)
            {
                gd32vw553_copy_value(ip, 16, parsed_ip);
                return RT_TRUE;
            }
        }
    }

    return RT_FALSE;
}

static rt_bool_t gd32vw553_parse_connect_ok(at_response_t resp)
{
    rt_size_t line_number;

    if (resp == RT_NULL)
    {
        return RT_FALSE;
    }

    for (line_number = 1; line_number <= resp->line_counts; line_number++)
    {
        const char *line = at_resp_get_line(resp, line_number);

        if (line == RT_NULL)
        {
            continue;
        }

        if (rt_strstr(line, ",OK") != RT_NULL ||
            rt_strstr(line, "OK") == line)
        {
            return RT_TRUE;
        }
    }

    return RT_FALSE;
}

static int gd32vw553_domain_resolve(const char *name, char ip[16])
{
    struct at_device *device;
    at_response_t resp;
    int result = -RT_ERROR;

    RT_ASSERT(name);
    RT_ASSERT(ip);

    device = at_device_get_first_initialized();
    if (device == RT_NULL)
    {
        return -RT_ERROR;
    }

    resp = at_create_resp(128, 0, 10 * RT_TICK_PER_SECOND);
    if (resp == RT_NULL)
    {
        return -RT_ENOMEM;
    }

    if (at_obj_exec_cmd(device->client, resp, "AT+CIPDOMAIN=\"%s\",1", name) == RT_EOK &&
        gd32vw553_parse_domain_ip(resp, ip))
    {
        result = RT_EOK;
    }

    at_delete_resp(resp);
    return result;
}

static void gd32vw553_socket_set_event_cb(at_socket_evt_t event, at_evt_cb_t cb)
{
    if (event < sizeof(at_evt_cb_set) / sizeof(at_evt_cb_set[0]))
    {
        at_evt_cb_set[event] = cb;
    }
}

static const struct at_socket_ops gd32vw553_socket_ops =
{
    gd32vw553_socket_connect,
    gd32vw553_socket_close,
    gd32vw553_socket_send,
    gd32vw553_domain_resolve,
    gd32vw553_socket_set_event_cb,
#if defined(AT_SW_VERSION_NUM) && AT_SW_VERSION_NUM > 0x10300
    RT_NULL,
#ifdef AT_USING_SOCKET_SERVER
    RT_NULL,
#endif
#endif
};

static void gd32vw553_urc_close(struct at_client *client, const char *data, rt_size_t size)
{
    struct at_device *device;
    struct at_socket *socket;
    const char *client_name = client->device->parent.name;

    RT_UNUSED(data);
    RT_UNUSED(size);

    device = at_device_get_by_name(AT_DEVICE_NAMETYPE_CLIENT, client_name);
    if (device == RT_NULL)
    {
        return;
    }

    raw_context.connected = RT_FALSE;
    socket = &(device->sockets[0]);
    if (at_evt_cb_set[AT_SOCKET_EVT_CLOSED] != RT_NULL)
    {
        at_evt_cb_set[AT_SOCKET_EVT_CLOSED](socket, AT_SOCKET_EVT_CLOSED, RT_NULL, 0);
    }
}

int gd32vw553_socket_init(struct at_device *device)
{
    int result;

    RT_ASSERT(device);

    at_obj_set_urc_table(device->client, urc_table,
                         sizeof(urc_table) / sizeof(urc_table[0]));

    if (raw_context.thread == RT_NULL)
    {
        rt_memset(&raw_context, 0, sizeof(raw_context));
        rt_sem_init(&raw_context.rx_notice, "gd32rx", 0, RT_IPC_FLAG_FIFO);
        rt_mutex_init(&raw_context.rx_lock, "gd32rl", RT_IPC_FLAG_PRIO);
        rt_mutex_init(&raw_context.tx_lock, "gd32tl", RT_IPC_FLAG_PRIO);

        raw_context.thread = rt_thread_create("gd32raw", gd32vw553_raw_rx_entry,
                                              &raw_context, 2048, 9, 10);
        if (raw_context.thread == RT_NULL)
        {
            rt_mutex_detach(&raw_context.tx_lock);
            rt_mutex_detach(&raw_context.rx_lock);
            rt_sem_detach(&raw_context.rx_notice);
            return -RT_ENOMEM;
        }

        result = rt_thread_startup(raw_context.thread);
        if (result != RT_EOK)
        {
            rt_thread_delete(raw_context.thread);
            raw_context.thread = RT_NULL;
            rt_mutex_detach(&raw_context.tx_lock);
            rt_mutex_detach(&raw_context.rx_lock);
            rt_sem_detach(&raw_context.rx_notice);
            return result;
        }
    }

    return RT_EOK;
}

int gd32vw553_socket_class_register(struct at_device_class *class)
{
    RT_ASSERT(class);

    class->socket_num = AT_DEVICE_GD32VW553_SOCKETS_NUM;
    class->socket_ops = &gd32vw553_socket_ops;
    return RT_EOK;
}

#endif /* AT_DEVICE_USING_GD32VW553 && AT_USING_SOCKET */

/*
 * Copyright (c) 2006-2023, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-08-25     moneng       first version for L501
 */

#include <stdio.h>
#include <string.h>
#include <at_device_l501.h>

#define LOG_TAG                        "at.skt.l501"
#include <at_log.h>

#if defined(AT_DEVICE_USING_L501) && defined(AT_USING_SOCKET)

#define L501_MODULE_SEND_MAX_SIZE       1460

#define SET_EVENT(socket, event)       (((socket + 1) << 16) | (event))

/* 事件定义 */
#define L501_EVENT_CONN_OK             (1L << 0)
#define L501_EVENT_SEND_OK             (1L << 1)
#define L501_EVENT_RECV_OK             (1L << 2)
#define L501_EVENT_CLOSE_OK            (1L << 3)
#define L501_EVENT_CONN_FAIL           (1L << 4)
#define L501_EVENT_SEND_FAIL           (1L << 5)
#define L501_EVENT_DOMAIN_OK           (1L << 6)

static at_evt_cb_t at_evt_cb_set[] = {
    [AT_SOCKET_EVT_RECV] = NULL,
    [AT_SOCKET_EVT_CLOSED] = NULL,
};

/* 错误码解析（根据 L501 手册可扩展） */
static void l501_tcpip_errcode_parse(int result)
{
    switch(result) {
        case 0:   LOG_D("Success"); break;
        case 1:   LOG_E("General error"); break;
        case 2:   LOG_E("Invalid parameter"); break;
        case 3:   LOG_E("Network error"); break;
        case 4:   LOG_E("Timeout"); break;
        case 5:   LOG_E("Memory error"); break;
        default:  LOG_E("Unknown err %d", result); break;
    }
}

static int l501_socket_event_send(struct at_device *device, uint32_t event)
{
    return rt_event_send(device->socket_event, event);
}

static int l501_socket_event_recv(struct at_device *device, uint32_t event, uint32_t timeout, rt_uint8_t option)
{
    rt_uint32_t recved;
    int ret = rt_event_recv(device->socket_event, event, option | RT_EVENT_FLAG_CLEAR, timeout, &recved);
    if (ret != RT_EOK) return -RT_ETIMEOUT;
    return (int)recved;
}

/* ---------- Socket 操作 ---------- */
static int l501_socket_close(struct at_socket *socket)
{
    int device_socket = (int)socket->user_data;
    struct at_device *device = (struct at_device *)socket->device;
    struct at_device_l501 *l501 = (struct at_device_l501 *)device->user_data;
    at_response_t resp = at_create_resp(64, 0, rt_tick_from_millisecond(300));
    if (!resp) return -RT_ENOMEM;

    /* 清除旧事件 */
    l501_socket_event_recv(device, SET_EVENT(device_socket, L501_EVENT_CLOSE_OK), 0, RT_EVENT_FLAG_OR);

    int ret = at_obj_exec_cmd(device->client, resp, "AT+CIPCLOSE=%d", device_socket);
    at_delete_resp(resp);
    if (ret != RT_EOK) return ret;

    /* 等待关闭确认，超时 5 秒 */
    int evt = l501_socket_event_recv(device, SET_EVENT(device_socket, L501_EVENT_CLOSE_OK),
                                     5 * RT_TICK_PER_SECOND, RT_EVENT_FLAG_OR);
    if (evt < 0) {
        LOG_E("wait CIPCLOSE event timeout.");
        return -RT_ETIMEOUT;
    }
    l501->socket_opened[device_socket] = RT_FALSE;
    return RT_EOK;
}

static int l501_socket_connect(struct at_socket *socket, char *ip, int32_t port,
                               enum at_socket_type type, rt_bool_t is_client)
{
    const char *type_str = RT_NULL;
    uint32_t event = 0;
    at_response_t resp = RT_NULL;
    int result = RT_EOK, event_result;
    int device_socket = (int)socket->user_data;
    struct at_device *device = (struct at_device *)socket->device;
    struct at_device_l501 *l501 = (struct at_device_l501 *)device->user_data;
    
    l501->socket_type[device_socket] = (type == AT_SOCKET_TCP) ? 0 : 1;
    if (type == AT_SOCKET_UDP) {
        strncpy(l501->remote_ip[device_socket], ip, 15);
        l501->remote_ip[device_socket][15] = '\0';
        l501->remote_port[device_socket] = port;
    }

    if (!is_client) {
        LOG_E("L501 only supports client mode.");
        return -RT_ERROR;
    }

    switch(type) {
        case AT_SOCKET_TCP: type_str = "TCP"; break;
        case AT_SOCKET_UDP: type_str = "UDP"; break;
        default: LOG_E("unsupported type."); return -RT_ERROR;
    }

    resp = at_create_resp(128, 0, rt_tick_from_millisecond(300));
    if (!resp) return -RT_ENOMEM;

    /* 清除旧事件 */
    event = SET_EVENT(device_socket, L501_EVENT_CONN_OK | L501_EVENT_CONN_FAIL);
    l501_socket_event_recv(device, event, 0, RT_EVENT_FLAG_OR);

    #if 0
    /* AT+CIPOPEN=<socket>,<type>,<ip>,<port> */
    if (at_obj_exec_cmd(device->client, resp, "AT+CIPOPEN=%d,%s,\"%s\",%d",
                        device_socket, type_str, ip, port) < 0) {
        result = -RT_ERROR;
        goto __exit;
    }
    #else
    if (type == AT_SOCKET_UDP) {
        // UDP: AT+CIPOPEN=<socket>,"UDP"
        if (at_obj_exec_cmd(device->client, resp, "AT+CIPOPEN=%d,\"UDP\"", device_socket) < 0) {
            result = -RT_ERROR;
            goto __exit;
        }
    } else {
        // TCP: AT+CIPOPEN=<socket>,"TCP","<ip>",<port>
        if (at_obj_exec_cmd(device->client, resp, "AT+CIPOPEN=%d,\"TCP\",\"%s\",%d",
                            device_socket, ip, port) < 0) {
            result = -RT_ERROR;
            goto __exit;
        }
    }
    #endif

    /* 等待 URC 返回，超时 60 秒 */
    if (l501_socket_event_recv(device, SET_EVENT(device_socket, 0),
                               60 * RT_TICK_PER_SECOND, RT_EVENT_FLAG_OR) < 0) {
        LOG_E("wait connect URC timeout.");
        result = -RT_ETIMEOUT;
        goto __exit;
    }

    event_result = l501_socket_event_recv(device, L501_EVENT_CONN_OK | L501_EVENT_CONN_FAIL,
                                          1 * RT_TICK_PER_SECOND, RT_EVENT_FLAG_OR);
    if (event_result < 0) {
        LOG_E("wait connect result timeout.");
        result = -RT_ETIMEOUT;
        goto __exit;
    }

    if (event_result & L501_EVENT_CONN_OK) {
        l501->socket_opened[device_socket] = RT_TRUE;
        result = RT_EOK;
    } else {
        LOG_E("connect failed.");
        result = -RT_ERROR;
    }

__exit:
    if (resp) at_delete_resp(resp);
    return result;
}

static int l501_socket_send(struct at_socket *socket, const char *buff, size_t bfsz,
                            enum at_socket_type type)
{
    uint32_t event;
    int result = 0, event_result;
    size_t cur_pkt, sent_size = 0;
    at_response_t resp = RT_NULL;
    int device_socket = (int)socket->user_data;
    struct at_device *device = (struct at_device *)socket->device;
    struct at_device_l501 *l501 = (struct at_device_l501 *)device->user_data;
    rt_mutex_t lock = device->client->lock;

    resp = at_create_resp(128, 2, rt_tick_from_millisecond(300));
    if (!resp) return -RT_ENOMEM;

    rt_mutex_take(lock, RT_WAITING_FOREVER);

    l501->user_data = (void *)device_socket;   /* 用于 URC 识别当前 socket */

    /* 清除旧 send 事件 */
    event = SET_EVENT(device_socket, L501_EVENT_SEND_OK | L501_EVENT_SEND_FAIL);
    l501_socket_event_recv(device, event, 0, RT_EVENT_FLAG_OR);

    int is_udp = (l501->socket_type[device_socket] == 1);
    
    at_obj_set_end_sign(device->client, '>');  /* 等待 > 提示 */

    while (sent_size < bfsz) {
        cur_pkt = (bfsz - sent_size < L501_MODULE_SEND_MAX_SIZE) ? (bfsz - sent_size) : L501_MODULE_SEND_MAX_SIZE;

        #if 0
        /* AT+CIPSEND=<socket>,<len> */
        if (at_obj_exec_cmd(device->client, resp, "AT+CIPSEND=%d,%d", device_socket, (int)cur_pkt) < 0) {
            result = -RT_ERROR;
            goto __exit;
        }
        #else
        if (is_udp) {
            // UDP 发送：指定远端
            if (at_obj_exec_cmd(device->client, resp, "AT+CIPSEND=%d,%d,\"%s\",%d",
                                device_socket, cur_pkt,
                                l501->remote_ip[device_socket],
                                l501->remote_port[device_socket]) < 0) {
                result = -RT_ERROR;
                goto __exit;
            }
        } else {
            // TCP 发送
            if (at_obj_exec_cmd(device->client, resp, "AT+CIPSEND=%d,%d",
                                device_socket, cur_pkt) < 0) {
                result = -RT_ERROR;
                goto __exit;
            }
        }
        #endif

        /* 发送数据 */
        int len = at_client_obj_send(device->client, buff + sent_size, cur_pkt);
        if (len == 0) {
            result = -RT_ERROR;
            goto __exit;
        }

        /* 等待 send URC */
        #if 0
        if (l501_socket_event_recv(device, SET_EVENT(device_socket, 0),
                                   10 * RT_TICK_PER_SECOND, RT_EVENT_FLAG_OR) < 0) {
            LOG_E("wait send URC timeout.");
            result = -RT_ETIMEOUT;
            goto __exit;
        }
        #endif

        event_result = l501_socket_event_recv(device, L501_EVENT_SEND_OK | L501_EVENT_SEND_FAIL,
                                              1 * RT_TICK_PER_SECOND, RT_EVENT_FLAG_OR);
        if (event_result < 0) {
            LOG_E("wait send result timeout.");
            result = -RT_ETIMEOUT;
            goto __exit;
        }

        if (event_result & L501_EVENT_SEND_FAIL) {
            LOG_E("send failed.");
            result = -RT_ERROR;
            goto __exit;
        }

        sent_size += cur_pkt;
        rt_thread_mdelay(10);  /* 小间隔 */
    }

__exit:
    at_obj_set_end_sign(device->client, 0);
    rt_mutex_release(lock);
    if (resp) at_delete_resp(resp);
    return (result == RT_EOK) ? (int)sent_size : result;
}

static int l501_domain_resolve(const char *name, char ip[16])
{
    at_response_t resp = RT_NULL;
    struct at_device *device = at_device_get_first_initialized();
    struct at_device_l501 *l501 = (struct at_device_l501 *)device->user_data;

    if (!device) {
        LOG_E("no initialized device.");
        return -RT_ERROR;
    }

    resp = at_create_resp(128, 0, rt_tick_from_millisecond(300));
    if (!resp) return -RT_ENOMEM;

    l501->socket_data = ip;   /* 用于 URC 回调写入 IP */
    /* 清除旧事件 */
    l501_socket_event_recv(device, L501_EVENT_DOMAIN_OK, 0, RT_EVENT_FLAG_OR);

    if (at_obj_exec_cmd(device->client, resp, "AT+MDNSGIP=\"%s\"", name) != RT_EOK) {
        l501->socket_data = RT_NULL;
        at_delete_resp(resp);
        return -RT_ERROR;
    }

    /* 等待 URC */
    if (l501_socket_event_recv(device, L501_EVENT_DOMAIN_OK, 10 * RT_TICK_PER_SECOND, RT_EVENT_FLAG_OR) < 0) {
        l501->socket_data = RT_NULL;
        at_delete_resp(resp);
        return -RT_ETIMEOUT;
    }

    if (strlen(ip) < 8) {
        l501->socket_data = RT_NULL;
        at_delete_resp(resp);
        return -RT_ERROR;
    }

    l501->socket_data = RT_NULL;
    at_delete_resp(resp);
    return RT_EOK;
}

static void l501_socket_set_event_cb(at_socket_evt_t event, at_evt_cb_t cb)
{
    if (event < sizeof(at_evt_cb_set)/sizeof(at_evt_cb_set[1]))
        at_evt_cb_set[event] = cb;
}

/* ---------- URC 处理函数 ---------- */
static void urc_netopen_func(struct at_client *client, const char *data, rt_size_t size)
{
    /* 检查 URC 数据中是否包含 "SUCCESS" */
    if (strstr(data, "SUCCESS") != RT_NULL) {
        LOG_D("NETOPEN success.");
    } else {
        LOG_E("NETOPEN failed: %.*s", size, data);
    }
}

static void urc_cipopen_func(struct at_client *client, const char *data, rt_size_t size)
{
    int socket_id = -1;
    struct at_device *device;
    char *client_name = client->device->parent.name;

    device = at_device_get_by_name(AT_DEVICE_NAMETYPE_CLIENT, client_name);
    if (!device) {
        LOG_E("get device failed.");
        return;
    }

    /* 检查是否包含 SUCCESS */
    if (strstr(data, "SUCCESS") != RT_NULL) {
        /* 尝试提取 socket_id，格式：+CIPOPEN: SUCCESS,<socket_id> */
        if (sscanf(data, "+CIPOPEN: SUCCESS,%d", &socket_id) == 1) {
            LOG_D("CIPOPEN success, socket %d", socket_id);
            l501_socket_event_send(device, SET_EVENT(socket_id, L501_EVENT_CONN_OK));
        } else {
            /* 如果无法提取，可能格式为 +CIPOPEN: SUCCESS （无逗号） */
            LOG_W("CIPOPEN SUCCESS but no socket id, using 0");
            l501_socket_event_send(device, SET_EVENT(0, L501_EVENT_CONN_OK));
        }
    } else if (strstr(data, "FAIL") != RT_NULL) {
        int err = -1;
        if (sscanf(data, "+CIPOPEN: FAIL,%d", &err) == 1) {
            LOG_E("CIPOPEN failed with err %d", err);
        } else {
            LOG_E("CIPOPEN failed");
        }
        /* 发送失败事件，socket_id 可能为 0 或未知 */
        l501_socket_event_send(device, SET_EVENT(0, L501_EVENT_CONN_FAIL));
    } else {
        LOG_W("Unknown +CIPOPEN response: %.*s", size, data);
    }
}

static void urc_cipsend_func(struct at_client *client, const char *data, rt_size_t size)
{
    int socket_id = -1;
    struct at_device *device;
    struct at_device_l501 *l501;
    char *client_name = client->device->parent.name;

    device = at_device_get_by_name(AT_DEVICE_NAMETYPE_CLIENT, client_name);
    if (!device) {
        LOG_E("get device failed.");
        return;
    }
    l501 = (struct at_device_l501 *)device->user_data;

    /* 检查是否包含 SUCCESS */
    if (strstr(data, "SUCCESS") != RT_NULL) {
        /* 格式1：+CIPSEND:SUCCESS,<socket>,<sent>,<total> */
        if (sscanf(data, "+CIPSEND:%*[^,],%d", &socket_id) == 1) {
            LOG_D("CIPSEND success, socket %d", socket_id);
            l501_socket_event_send(device, SET_EVENT(socket_id, L501_EVENT_SEND_OK));
        } else if (sscanf(data, "+CIPSEND: SUCCESS,%d", &socket_id) == 1) {
            LOG_D("CIPSEND success, socket %d", socket_id);
            l501_socket_event_send(device, SET_EVENT(socket_id, L501_EVENT_SEND_OK));
        } else {
            /* 备用：使用 user_data 中的 socket id */
            int sock = (int)l501->user_data;
            LOG_W("No socket id in URC, using user_data %d", sock);
            l501_socket_event_send(device, SET_EVENT(sock, L501_EVENT_SEND_OK));
        }
    } else if (strstr(data, "FAIL") != RT_NULL) {
        LOG_E("CIPSEND failed: %.*s", size, data);
        if (sscanf(data, "+CIPSEND:%*[^,],%d", &socket_id) == 1) {
            l501_socket_event_send(device, SET_EVENT(socket_id, L501_EVENT_SEND_FAIL));
        } else {
            int sock = (int)l501->user_data;
            l501_socket_event_send(device, SET_EVENT(sock, L501_EVENT_SEND_FAIL));
        }
    } else {
        LOG_W("Unknown CIPSEND response: %.*s", size, data);
    }
}

static void urc_ipd_func(struct at_client *client, const char *data, rt_size_t size)
{
    int socket_id, len;
    char *recv_buf = RT_NULL;
    struct at_socket *socket;
    struct at_device *device;
    char *client_name = client->device->parent.name;

    if (sscanf(data, "+IPD,%d,%d", &socket_id, &len) != 2) return;

    device = at_device_get_by_name(AT_DEVICE_NAMETYPE_CLIENT, client_name);
    if (!device) {
        LOG_E("get device failed.");
        return;
    }

    recv_buf = (char *)rt_malloc(len + 1);
    if (!recv_buf) {
        LOG_E("no memory for IPD data.");
        /* 丢弃数据 */
        char tmp[32];
        while (len > 0) {
            int rd = (len > 32) ? 32 : len;
            at_client_obj_recv(client, tmp, rd, rt_tick_from_millisecond(100));
            len -= rd;
        }
        return;
    }

    if (at_client_obj_recv(client, recv_buf, len, rt_tick_from_millisecond(500)) != len) {
        LOG_E("recv data incomplete.");
        rt_free(recv_buf);
        return;
    }
    recv_buf[len] = '\0';

    socket = &(device->sockets[socket_id]);
    if (at_evt_cb_set[AT_SOCKET_EVT_RECV]) {
        at_evt_cb_set[AT_SOCKET_EVT_RECV](socket, AT_SOCKET_EVT_RECV, recv_buf, len);
    } else {
        rt_free(recv_buf);
    }
}

static void urc_ciprxget_func(struct at_client *client, const char *data, rt_size_t size)
{
    int socket_id = -1, status = -1, len = -1;
    char *recv_buf = RT_NULL;
    struct at_socket *socket = RT_NULL;
    struct at_device *device = RT_NULL;
    char *client_name = client->device->parent.name;

    /* 解析格式：+CIPRXGET:SUCCESS,<socket>,<status>,<len>, */
    if (sscanf(data, "+CIPRXGET:SUCCESS,%d,%d,%d,", &socket_id, &status, &len) != 3) {
        /* 尝试其他可能格式 */
        if (sscanf(data, "+CIPRXGET:%*[^,],%d,%d,%d,", &socket_id, &status, &len) != 3) {
            LOG_W("Unknown +CIPRXGET response: %.*s", size, data);
            return;
        }
    }

    if (socket_id < 0 || len <= 0) {
        LOG_D("Invalid CIPRXGET: socket=%d, len=%d", socket_id, len);
        return;
    }

    device = at_device_get_by_name(AT_DEVICE_NAMETYPE_CLIENT, client_name);
    if (!device) {
        LOG_E("get device failed.");
        return;
    }
    struct at_device_l501 *l501 = (struct at_device_l501 *)device->user_data;

    /* 检查socket是否已打开，若已关闭则丢弃数据 */
    if (!l501->socket_opened[socket_id]) {
        LOG_W("CIPRXGET for closed socket %d, discard.", socket_id);
        // 丢弃数据
        char tmp[32];
        int remain = len;
        while (remain > 0) {
            int rd = (remain > 32) ? 32 : remain;
            at_client_obj_recv(client, tmp, rd, rt_tick_from_millisecond(100));
            remain -= rd;
        }
        return;
    }

    /* 分配接收缓冲区 */
    recv_buf = (char *)rt_malloc(len + 1);
    if (!recv_buf) {
        LOG_E("no memory for recv buffer (%d)", len);
        /* 丢弃数据 */
        char tmp[32];
        int remain = len;
        while (remain > 0) {
            int rd = (remain > 32) ? 32 : remain;
            at_client_obj_recv(client, tmp, rd, rt_tick_from_millisecond(100));
            remain -= rd;
        }
        return;
    }

    /* 读取实际数据 */
    if (at_client_obj_recv(client, recv_buf, len, rt_tick_from_millisecond(500)) != len) {
        LOG_E("recv data incomplete.");
        rt_free(recv_buf);
        return;
    }
    recv_buf[len] = '\0';

    /* 获取 socket 对象并调用上层回调 */
    socket = &(device->sockets[socket_id]);
    if (at_evt_cb_set[AT_SOCKET_EVT_RECV]) {
        at_evt_cb_set[AT_SOCKET_EVT_RECV](socket, AT_SOCKET_EVT_RECV, recv_buf, len);
    } else {
        rt_free(recv_buf);
    }
}

static void urc_cipclose_func(struct at_client *client, const char *data, rt_size_t size)
{
    int socket_id = -1;
    struct at_device *device;
    char *client_name = client->device->parent.name;

    device = at_device_get_by_name(AT_DEVICE_NAMETYPE_CLIENT, client_name);
    if (!device) {
        LOG_E("get device failed.");
        return;
    }

    /* 解析格式：+CIPCLOSE:SUCCESS,<socket> 或 +CIPCLOSE: FAIL,<socket> */
    if (strstr(data, "SUCCESS") != RT_NULL) {
        if (sscanf(data, "+CIPCLOSE:SUCCESS,%d", &socket_id) == 1) {
            LOG_D("CIPCLOSE success, socket %d", socket_id);
            l501_socket_event_send(device, SET_EVENT(socket_id, L501_EVENT_CLOSE_OK));
        } else {
            /* 若无 socket id，默认使用 0 */
            LOG_W("CIPCLOSE SUCCESS without socket id");
            l501_socket_event_send(device, SET_EVENT(0, L501_EVENT_CLOSE_OK));
        }
    } else {
        LOG_E("CIPCLOSE failed: %.*s", size, data);
        /* 也可发送失败事件，但当前忽略 */
    }
}

static void urc_mdnsgip_func(struct at_client *client, const char *data, rt_size_t size)
{
    char domain[64] = {0}, ip[16] = {0};
    struct at_device *device;
    struct at_device_l501 *l501;
    char *client_name = client->device->parent.name;

    /* 格式1：+MDNSGIP:<domain>,<ip>   （无空格，无引号） */
    if (sscanf(data, "+MDNSGIP:%[^,],%15s", domain, ip) == 2) {
        device = at_device_get_by_name(AT_DEVICE_NAMETYPE_CLIENT, client_name);
        if (!device) return;
        l501 = (struct at_device_l501 *)device->user_data;
        if (l501->socket_data) {
            strncpy((char *)l501->socket_data, ip, 15);
            ((char *)l501->socket_data)[15] = '\0';
            l501_socket_event_send(device, L501_EVENT_DOMAIN_OK);
        }
    }
    /* 格式2：+MDNSGIP:"<domain>","<ip>"  （带引号） */
    else if (sscanf(data, "+MDNSGIP:\"%[^\"]\",\"%[^\"]\"", domain, ip) == 2) {
        device = at_device_get_by_name(AT_DEVICE_NAMETYPE_CLIENT, client_name);
        if (!device) return;
        l501 = (struct at_device_l501 *)device->user_data;
        if (l501->socket_data) {
            strncpy((char *)l501->socket_data, ip, 15);
            ((char *)l501->socket_data)[15] = '\0';
            l501_socket_event_send(device, L501_EVENT_DOMAIN_OK);
        }
    }
    /* 格式3：+MDNSGIP: <result>,<domain>,<ip> （带result和逗号分隔）可再扩展 */
    else {
        LOG_W("Unknown +MDNSGIP response: %.*s", size, data);
    }
}

static const struct at_urc urc_table[] = {
    {"+NETOPEN:",   "\r\n", urc_netopen_func},
    {"+CIPOPEN:",   "\r\n", urc_cipopen_func},
    {"SEND OK",     "\r\n", urc_cipsend_func},
    {"SEND FAIL",   "\r\n", urc_cipsend_func},
    {"+CIPSEND:",   "\r\n", urc_cipsend_func},  /* 备用 */
    {"+CIPRXGET:",  "\r\n", urc_ciprxget_func},
    {"+IPD,",       ":",   urc_ipd_func},       /* 注意分隔符为冒号 */
    {"+CIPCLOSE:",  "\r\n", urc_cipclose_func},
    {"+MDNSGIP:", "\r\n", urc_mdnsgip_func},
};

static const struct at_socket_ops l501_socket_ops = {
    l501_socket_connect,
    l501_socket_close,
    l501_socket_send,
    l501_domain_resolve,
    l501_socket_set_event_cb,
#if defined(AT_SW_VERSION_NUM) && AT_SW_VERSION_NUM > 0x10300
    RT_NULL,
#endif
};

int l501_socket_init(struct at_device *device)
{
    at_obj_set_urc_table(device->client, urc_table, sizeof(urc_table)/sizeof(urc_table[0]));
    return RT_EOK;
}

int l501_socket_class_register(struct at_device_class *class)
{
    class->socket_num = AT_DEVICE_L501_SOCKETS_NUM;
    class->socket_ops = &l501_socket_ops;
    return RT_EOK;
}

#endif /* AT_DEVICE_USING_L501 && AT_USING_SOCKET */

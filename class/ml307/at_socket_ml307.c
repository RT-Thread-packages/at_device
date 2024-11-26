/*
 * Copyright (c) 2006-2023, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-12-16     Jonas        first version
 */

#include <stdio.h>
#include <string.h>

#include <at_device_ml307.h>

#define LOG_TAG                        "at.skt.ml307"
#include <at_log.h>

#if defined(AT_DEVICE_USING_ML307) && defined(AT_USING_SOCKET)
#if !defined (ML307_MODULE_SEND_MAX_SIZE)
#define ML307_MODULE_SEND_MAX_SIZE   4096
#endif
/* set real event by current socket and current state */
#define SET_EVENT(socket, event)       (((socket + 1) << 16) | (event))
//#define SET_EVENT(socket, event)       ((1 << (socket + 16)) | (event)) 

/* AT socket event type */
#define ML307_EVENT_CONN_OK          (1L << 0)
#define ML307_EVENT_SEND_OK          (1L << 1)
#define ML307_EVENT_RECV_OK          (1L << 2)
#define ML307_EVENT_CLOSE_OK         (1L << 3)
#define ML307_EVENT_CONN_FAIL        (1L << 4)
#define ML307_EVENT_SEND_FAIL        (1L << 5)

static at_evt_cb_t at_evt_cb_set[] = {
        [AT_SOCKET_EVT_RECV] = NULL,
        [AT_SOCKET_EVT_CLOSED] = NULL,
};

static int ml307_socket_event_send(struct at_device *device, uint32_t event)
{
    return (int) rt_event_send(device->socket_event, event);
}

static int ml307_socket_event_recv(struct at_device *device, uint32_t event, uint32_t timeout, rt_uint8_t option)
{
    int result = RT_EOK;
    rt_uint32_t recved;

    result = rt_event_recv(device->socket_event, event, option | RT_EVENT_FLAG_CLEAR, timeout, &recved);
    if (result != RT_EOK)
    {
        return -RT_ETIMEOUT;
    }

    return recved;
}

/**
 * close socket by AT commands.
 *
 * @param current socket
 *
 * @return  0: close socket success
 *         -1: send AT commands error
 *         -2: wait socket event timeout
 *         -5: no memory
 */
static int ml307_socket_close(struct at_socket *socket)
{
    uint32_t event = 0;
    int result = RT_EOK;
    int device_socket = (int) socket->user_data;
    struct at_device *device = (struct at_device *) socket->device;

    /* clear socket close event */
    event = SET_EVENT(device_socket, ML307_EVENT_CLOSE_OK);
    ml307_socket_event_recv(device, event, 0, RT_EVENT_FLAG_OR);

    if (at_obj_exec_cmd(device->client, NULL, "AT+MIPCLOSE=%d", device_socket) < 0)
    {
        result = -RT_ERROR;
        goto __exit;
    }

    if (ml307_socket_event_recv(device, event, 1 * RT_TICK_PER_SECOND, RT_EVENT_FLAG_OR) < 0)
    {
        LOG_E("ml307 device(%s) socket(%d) close failed, wait close OK timeout.", device->name, device_socket);
        result = -RT_ETIMEOUT;
        goto __exit;
    }

__exit:
    return result;
}

/**
 * create TCP/UDP client or server connect by AT commands.
 *
 * @param socket current socket
 * @param ip server or client IP address
 * @param port server or client port
 * @param type connect socket type(tcp, udp)
 * @param is_client connection is client
 *
 * @return   0: connect success
 *          -1: connect failed, send commands error or type error
 *          -2: wait socket event timeout
 *          -5: no memory
 */
static int ml307_socket_connect(struct at_socket *socket, char *ip, int32_t port, enum at_socket_type type, rt_bool_t is_client)
{
    uint32_t event = 0;
    rt_bool_t retryed = RT_FALSE;
    at_response_t resp = RT_NULL;
    int result = RT_EOK, event_result = 0;
    int device_socket = (int) socket->user_data;
    struct at_device *device = (struct at_device *) socket->device;

    RT_ASSERT(ip);
    RT_ASSERT(port >= 0);

    resp = at_create_resp(128, 0, 5 * RT_TICK_PER_SECOND);
    if (resp == RT_NULL)
    {
        LOG_E("no memory for ml307 device(%s) response structure.", device->name);
        return -RT_ENOMEM;
    }

__retry:

    /* clear socket connect event */
    event = SET_EVENT(device_socket, ML307_EVENT_CONN_OK | ML307_EVENT_CONN_FAIL);
    ml307_socket_event_recv(device, event, 0, RT_EVENT_FLAG_OR);

    if (is_client)
    {
        switch (type)
        {
        case AT_SOCKET_TCP:
            /* send AT commands(eg: AT+MIPOPEN=0,"TCP","x.x.x.x", 1234) to connect TCP server */
            if (at_obj_exec_cmd(device->client, RT_NULL, "AT+MIPOPEN=%d,\"TCP\",\"%s\",%d", device_socket, ip, port) < 0)
            {
                result = -RT_ERROR;
                goto __exit;
            }
            break;

        case AT_SOCKET_UDP:
            if (at_obj_exec_cmd(device->client, RT_NULL, "AT+MIPOPEN=%d,\"UDP\",\"%s\",%d", device_socket, ip, port) < 0)
            {
                result = -RT_ERROR;
                goto __exit;
            }
            break;

        default:
            LOG_E("ml307 device(%s) not supported connect type : %d.", device->name, type);
            result = -RT_ERROR;
            goto __exit;
        }
            }

    /* waiting result event from AT URC, the device default connection timeout is 75 seconds, but it set to 10 seconds is convenient to use */
    if (ml307_socket_event_recv(device, SET_EVENT(device_socket, 0), 10 * RT_TICK_PER_SECOND, RT_EVENT_FLAG_OR) < 0)
    {
        LOG_E("ml307 device(%s) socket(%d) connect failed, wait connect result timeout.",device->name, device_socket);
        result = -RT_ETIMEOUT;
        goto __exit;
    }
    /* waiting OK or failed result */
    event_result = ml307_socket_event_recv(device,
            ML307_EVENT_CONN_OK | ML307_EVENT_CONN_FAIL, 1 * RT_TICK_PER_SECOND, RT_EVENT_FLAG_OR);
    if (event_result < 0)
    {
        LOG_E("ml307 device(%s) socket(%d) connect failed, wait connect OK|FAIL timeout.", device->name, device_socket);
        result = -RT_ETIMEOUT;
        goto __exit;
    }
    /* check result */
    if (event_result & ML307_EVENT_CONN_FAIL)
    {
        if (retryed == RT_FALSE)
        {
            LOG_D("ml307 device(%s) socket(%d) connect failed, maybe the socket was not be closed at the last time and now will retry.",
                    device->name, device_socket);
            if (ml307_socket_close(socket) < 0)
            {
                result = -RT_ERROR;
                goto __exit;
            }
            retryed = RT_TRUE;
            goto __retry;
        }
        LOG_E("ml307 device(%s) socket(%d) connect failed.", device->name, device_socket);
        result = -RT_ERROR;
        goto __exit;
    }

__exit:
    if (resp)
    {
        at_delete_resp(resp);
    }

    return result;
}

/**
 * send data to server or client by AT commands.
 *
 * @param socket current socket
 * @param buff send buffer
 * @param bfsz send buffer size
 * @param type connect socket type(tcp, udp)
 *
 * @return >=0: the size of send success
 *          -1: send AT commands error or send data error
 *          -2: waited socket event timeout
 *          -5: no memory
 */
static int ml307_socket_send(struct at_socket *socket, const char *buff, size_t bfsz, enum at_socket_type type)
{
    uint32_t event = 0;
    int result = RT_EOK, event_result = 0;
    size_t cur_pkt_size = 0, sent_size = 0;
    at_response_t resp = RT_NULL;
    int device_socket = (int) socket->user_data;
    struct at_device *device = (struct at_device *) socket->device;
    rt_mutex_t lock = device->client->lock;

    RT_ASSERT(buff);

    resp = at_create_resp(128, 2, 5 * RT_TICK_PER_SECOND);
    if (resp == RT_NULL)
    {
        LOG_E("no memory for ml307 device(%s) response structure.", device->name);
        return -RT_ENOMEM;
    }

    rt_mutex_take(lock, RT_WAITING_FOREVER);

    /* clear socket connect event */
    event = SET_EVENT(device_socket, ML307_EVENT_SEND_OK | ML307_EVENT_SEND_FAIL);
    ml307_socket_event_recv(device, event, 0, RT_EVENT_FLAG_OR);

    /* set AT client end sign to deal with '>' sign.*/
    at_obj_set_end_sign(device->client, '>');

    while (sent_size < bfsz)
    {
        if (bfsz - sent_size < ML307_MODULE_SEND_MAX_SIZE)
        {
            cur_pkt_size = bfsz - sent_size;
        }
        else
        {
            cur_pkt_size = ML307_MODULE_SEND_MAX_SIZE;
        }

        /* send the "AT+CIPSEND" commands to AT server than receive the '>' response on the first line. */
        if (at_obj_exec_cmd(device->client, resp, "AT+MIPSEND=%d,%d", device_socket, cur_pkt_size) < 0)
        {
            result = -RT_ERROR;
            goto __exit;
        }

        /* send the real data to server or client */
        result = (int) at_client_obj_send(device->client, buff + sent_size, cur_pkt_size);
        if (result == 0)
        {
            result = -RT_ERROR;
            goto __exit;
        }
        /* waiting result event from AT URC */
        if (ml307_socket_event_recv(device, SET_EVENT(device_socket, 0), 15 * RT_TICK_PER_SECOND, RT_EVENT_FLAG_OR) < 0)
        {
            LOG_E("ml307 device(%s) socket(%d) send failed, wait connect result timeout.", device->name, device_socket);
            result = -RT_ETIMEOUT;
            goto __exit;
        }
        /* waiting OK or failed result */
        event_result = ml307_socket_event_recv(device,
                                                ML307_EVENT_SEND_OK | ML307_EVENT_SEND_FAIL, 5 * RT_TICK_PER_SECOND, RT_EVENT_FLAG_OR);
        if (event_result < 0)
        {
            LOG_E("ml307 device(%s) socket(%d) send failed, wait connect OK|FAIL timeout.", device->name, device_socket);
            result = -RT_ETIMEOUT;
            goto __exit;
        }
        /* check result */
        if (event_result & ML307_EVENT_SEND_FAIL)
        {
            LOG_E("ml307 device(%s) socket(%d) send failed.", device->name, device_socket);
            result = -RT_ERROR;
            goto __exit;
        }

        sent_size += cur_pkt_size;
    }

__exit:
    /* reset the end sign for data conflict */
    at_obj_set_end_sign(device->client, 0);

    rt_mutex_release(lock);

    if (resp)
    {
        at_delete_resp(resp);
    }

    return result;
}

/**
 * domain resolve by AT commands.
 *
 * @param name domain name
 * @param ip parsed IP address, it's length must be 16
 *
 * @return  0: domain resolve success
 *         -1: send AT commands error or response error
 *         -2: wait socket event timeout
 *         -5: no memory
 */
static int ml307_domain_resolve(const char *name, char ip[16])
{
#define RESOLVE_RETRY                  5

    int i, result = RT_EOK;
    char recv_ip[16] = { 0 };
    at_response_t resp = RT_NULL;
    struct at_device *device = RT_NULL;

    RT_ASSERT(name);
    RT_ASSERT(ip);

    device = at_device_get_first_initialized();
    if (device == RT_NULL)
    {
        LOG_E("get first initialization ml307 device failed.");
        return -RT_ERROR;
    }

    /* The maximum response time is 14 seconds, affected by network status */
    resp = at_create_resp(1024, 4, 14 * RT_TICK_PER_SECOND);

    if (resp == RT_NULL)
    {
        LOG_E("no memory for ml307 device(%s) response structure.", device->name);
        return -RT_ENOMEM;
    }

    for (i = 0; i < RESOLVE_RETRY; i++)
    {
        int err_code = 0;

        if (at_obj_exec_cmd(device->client, resp, "AT+MDNSGIP=\"%s\"", name) < 0)
        {
            result = -RT_ERROR;
            goto __exit;
        }

        /* domain name prase error options */
        if (at_resp_parse_line_args_by_kw(resp, "+CME ERROR: ", "+CME ERROR: %d", &err_code) > 0)
        {
            /* 580 - DNS解析失败或错误的IP格式, 581 - DNS忙碌 */
            if (err_code == 580 || err_code == 581)
            {
                result = -RT_ERROR;
                goto __exit;
            }
        }

        /* parse the third line of response data, get the IP address */
        if (at_resp_parse_line_args_by_kw(resp, "+MDNSGIP: ", "%*[^,],\"%[^\"]", recv_ip) < 0)
        {
            rt_thread_mdelay(100);
            /* resolve failed, maybe receive an URC CRLF */
            continue;
        }

        if (rt_strlen(recv_ip) < 8)
        {
            rt_thread_mdelay(100);
            /* resolve failed, maybe receive an URC CRLF */
            continue;
        }
        else
        {
            rt_thread_mdelay(10);
            rt_strncpy(ip, recv_ip, 15);
            ip[15] = '\0';
            break;
        }
    }

__exit:
    if (resp)
    {
        at_delete_resp(resp);
    }

    return result;
}

/**
 * set AT socket event notice callback
 *
 * @param event notice event
 * @param cb notice callback
 */
static void ml307_socket_set_event_cb(at_socket_evt_t event, at_evt_cb_t cb)
{
    if (event < sizeof(at_evt_cb_set) / sizeof(at_evt_cb_set[1]))
    {
        at_evt_cb_set[event] = cb;
    }
}

static void urc_connect_func(struct at_client *client, const char *data, rt_size_t size)
{
    int device_socket = 0;
    struct at_device *device = RT_NULL;
    char *client_name = client->device->parent.name;
    int connect_result = 0;

    RT_ASSERT(data && size);

    device = at_device_get_by_name(AT_DEVICE_NAMETYPE_CLIENT, client_name);
    if (device == RT_NULL)
    {
        LOG_E("get ml307 device by client name(%s) failed.", client_name);
        return;
    }

    /* get the current socket by receive data */
    rt_sscanf(data, "%*s %d,%d", &device_socket, &connect_result);

    if(connect_result == 0)
    {
        ml307_socket_event_send(device, SET_EVENT(device_socket, ML307_EVENT_CONN_OK));
    }
    else
    {
        ml307_socket_event_send(device, SET_EVENT(device_socket, ML307_EVENT_CONN_FAIL));
    }
}

static void urc_send_func(struct at_client *client, const char *data, rt_size_t size)
{
    int device_socket = 0;
    struct at_device *device = RT_NULL;
    char *client_name = client->device->parent.name;
    int send_len = 0;

    RT_ASSERT(data && size);

    device = at_device_get_by_name(AT_DEVICE_NAMETYPE_CLIENT, client_name);
    if (device == RT_NULL)
    {
        LOG_E("get ml307 device by client name(%s) failed.", client_name);
        return;
    }

    /* get the current socket by receive data */
    rt_sscanf(data, "+MIPSEND: %d,%d", &device_socket, &send_len);

    if (send_len >= 0)
    {
        ml307_socket_event_send(device, SET_EVENT(device_socket, ML307_EVENT_SEND_OK));
    }
    else
    {
        ml307_socket_event_send(device, SET_EVENT(device_socket, ML307_EVENT_SEND_FAIL));
    }
}

static void urc_close_func(struct at_client *client, const char *data, rt_size_t size)
{
    int device_socket = 0;
    struct at_device *device = RT_NULL;
    char *client_name = client->device->parent.name;
    int close_result = 0;

    RT_ASSERT(data && size);

    device = at_device_get_by_name(AT_DEVICE_NAMETYPE_CLIENT, client_name);
    if (device == RT_NULL)
    {
        LOG_E("get ml307 device by client name(%s) failed.", client_name);
        return;
    }

    /* get the current socket by receive data */
    rt_sscanf(data, "%d", &device_socket);

    if (close_result == 0)
    {
        ml307_socket_event_send(device, SET_EVENT(device_socket, ML307_EVENT_CLOSE_OK));
    }
    else
    {
        struct at_socket *socket = RT_NULL;

        /* get AT socket object by device socket descriptor */
        socket = &(device->sockets[device_socket]);

        /* notice the socket is disconnect by remote */
        if (at_evt_cb_set[AT_SOCKET_EVT_CLOSED])
        {
            at_evt_cb_set[AT_SOCKET_EVT_CLOSED](socket, AT_SOCKET_EVT_CLOSED, RT_NULL, 0);
        }
    }
}

static void urc_recv_func(struct at_client *client, const char *data, rt_size_t size)
{
    int device_socket = 0;
    rt_int32_t timeout;
    rt_size_t temp_size = 0;
    char temp[8] = {0};
    rt_size_t bfsz = 0;
    char *recv_buf = RT_NULL;
    struct at_socket *socket = RT_NULL;
    struct at_device *device = RT_NULL;
    char *client_name = client->device->parent.name;
    char *ptr = RT_NULL, *data_ptr = RT_NULL, *ptr2 = RT_NULL;

    RT_ASSERT(data && size);

    /* get the current socket and receive buffer size by receive data */
    rt_sscanf(data, "+MIPURC:%*[^,],%d,%d", &device_socket, (int *) &bfsz);
    /* get receive timeout by receive buffer length */
    timeout = bfsz;

    if (device_socket < 0 || bfsz == 0)
    {
        return;
    }

    device = at_device_get_by_name(AT_DEVICE_NAMETYPE_CLIENT, client_name);
    if (device == RT_NULL)
    {
        LOG_E("get ml307 device by client name(%s) failed.", client_name);
        return;
    }

    //ML307 AT指令， TCP和UDP普通模式接收数据格式如下：
    //+MIPURC: "rtcp",<connect_id>,<recv_length>,<data>
    //+MIPURC: "rudp",<connect_id>,<recv_length>,<data>
    ptr = rt_strstr(data,",");
    data_ptr = ptr + 1;
    ptr = rt_strstr(data_ptr,",");
    data_ptr = ptr + 1;
    ptr = rt_strstr(data_ptr,",");
    ptr++;                                  //ptr指向<data>数据起始位置

    temp_size = size - (ptr - data);        //temp_size是接收缓冲区中接收到的<data>实际长度

    recv_buf = (char *) rt_calloc(1, bfsz);
    if (recv_buf == RT_NULL)
    {
        LOG_E("no memory for ml307 device(%s) URC receive buffer (%d).", device->name, bfsz);

        if(bfsz <= temp_size)
        {
            return;
        }

        /* read and clean the coming data */
        while (temp_size < bfsz)
        {
            if (bfsz - temp_size > sizeof(temp))
            {
                at_client_obj_recv(client, temp, sizeof(temp), timeout);
            }
            else
            {
                at_client_obj_recv(client, temp, bfsz - temp_size, timeout);
            }
            temp_size += sizeof(temp);
        }
        return;
    }

    if (temp_size - 2 == bfsz)
    {
        rt_memcpy(recv_buf, ptr, bfsz);
    }
    else if(temp_size < bfsz)
    {
        rt_memcpy(recv_buf, ptr, temp_size);

        ptr2 = recv_buf + temp_size;

        int recv_len = at_client_obj_recv(client, ptr2, bfsz - temp_size, timeout);

        /* sync receive data */
        if (recv_len != (bfsz - temp_size))
        {
            LOG_E("ml307 device(%s) receive size(%d) data failed.", device->name, bfsz);
            rt_free(recv_buf);
            return;
        }
    }
    else
    {
        LOG_E("ml307 device(%s) receive size(%d) data failed.", device->name, bfsz);
        rt_free(recv_buf);
        return;
    }

    /* get AT socket object by device socket descriptor */
    socket = &(device->sockets[device_socket]);

    /* notice the receive buffer and buffer size */
    if (at_evt_cb_set[AT_SOCKET_EVT_RECV])
    {
        at_evt_cb_set[AT_SOCKET_EVT_RECV](socket, AT_SOCKET_EVT_RECV, recv_buf, bfsz);
    }
}
static void urc_state_func(struct at_client *client, const char *data, rt_size_t size)
{
    RT_ASSERT(data);
    LOG_I("URC state data : %.*s", size, data);
}
static void urc_func(struct at_client *client, const char *data, rt_size_t size)
{
    RT_ASSERT(data);
    LOG_I("URC data : %.*s", size, data);
}
static void urc_disc_func(struct at_client *client, const char *data, rt_size_t size)
{
    int device_socket = 0;
    struct at_device *device = RT_NULL;
    char *client_name = client->device->parent.name;
    int connect_state = 0;

    RT_ASSERT(data && size);

    device = at_device_get_by_name(AT_DEVICE_NAMETYPE_CLIENT, client_name);
    if (device == RT_NULL)
    {
        LOG_E("get ml307 device by client name(%s) failed.", client_name);
        return;
    }

    /* get the current socket & connect_state by receive data */
    rt_sscanf(data, "%*[^ ] \"%*[^\"]\",%d,%d", &device_socket, &connect_state);

//    LOG_I("device_socket = %d, connect_state = %d", device_socket, connect_state);

    //connect_state: 1. The servr close connection; 2. Connection exception; 3. Deactivate PDP context.
    if (connect_state > 0)
    {
        struct at_socket *socket = RT_NULL;

        /* get AT socket object by device socket descriptor */
        socket = &(device->sockets[device_socket]);

        /* notice the socket is disconnect by remote */
        if (at_evt_cb_set[AT_SOCKET_EVT_CLOSED])
        {
            at_evt_cb_set[AT_SOCKET_EVT_CLOSED](socket, AT_SOCKET_EVT_CLOSED, RT_NULL, 0);
        }
    }
}
static void urc_drop_func(struct at_client *client, const char *data, rt_size_t size)
{
    RT_ASSERT(data);
    LOG_I("URC data : %.*s", size, data);
}
static void urc_disc_drop_func(struct at_client *client, const char *data, rt_size_t size)
{
    RT_ASSERT(data && size);

    switch(*(data + 11))
    {
        case 'i' : urc_disc_func(client, data, size); break;            //+MIPURC: "disconn",<connect_id>,<connect_state>
        case 'r' : urc_drop_func(client, data, size); break;            //+MIPURC: "drop",<connect_id>,<drop_length>
        default  : urc_func(client, data, size);      break;
    }
}
static void urc_mipurc_func(struct at_client *client, const char *data, rt_size_t size)
{
    RT_ASSERT(data && size);

    switch(*(data + 10))
    {
        case 's' : urc_state_func(client, data, size); break;           //+MIPURC: "state",<connect_id>,<connect_state>
        case 'r' : urc_recv_func(client, data, size); break;            //+MIPURC: "recv",<connect_id>,<recv_length>
        case 'd' : urc_disc_drop_func(client, data, size); break;       //+MIPURC: "disconn",<connect_id>,<connect_state>或+MIPURC: "drop",<connect_id>,<drop_length>
        default  : urc_func(client, data, size);      break;
    }
}

static void urc_cme_err_func(struct at_client *client, const char *data, rt_size_t size)
{
    RT_ASSERT(data);
    LOG_I("URC CME ERROR Code : %.*s", size, data);
}
/* ML307 device URC table for the socket data */
static const struct at_urc urc_table[] =
{
    {"+MIPOPEN:",   "\r\n",                urc_connect_func},
    {"",            "CONNECT\r\n",         urc_connect_func},
    {"+MIPSEND:",   "\r\n",                urc_send_func},
    {"+MIPCLOSE:",  "\r\n",                urc_close_func},
    {"+MIPURC:",    "\r\n",                urc_mipurc_func},
    {"+CME ERROR:", "\r\n",                urc_cme_err_func},
};

static struct at_socket_ops ml307_socket_ops =
{
    ml307_socket_connect,
    ml307_socket_close,
    ml307_socket_send,
    ml307_domain_resolve,
    ml307_socket_set_event_cb,
#if defined(AT_SW_VERSION_NUM) && AT_SW_VERSION_NUM > 0x10300
    RT_NULL,
#endif
};

int ml307_socket_init(struct at_device *device)
{
    RT_ASSERT(device);

    /* register URC data execution function  */
    at_obj_set_urc_table(device->client, urc_table, sizeof(urc_table) / sizeof(urc_table[0]));

    return RT_EOK;
}

int ml307_socket_class_register(struct at_device_class *class)
{
    RT_ASSERT(class);

    class->socket_num = AT_DEVICE_ML307_SOCKETS_NUM;
    class->socket_ops = &ml307_socket_ops;

    return RT_EOK;
}

#endif /* AT_DEVICE_USING_ML307 && AT_USING_SOCKET */

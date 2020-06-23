/*
 * File      : at_socket_m5311.c
 * This file is part of RT-Thread RTOS
 * COPYRIGHT (C) 2006 - 2018, RT-Thread Development Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Change Logs:
 * Date           Author       Notes
 * 2020-03-17     LXGMAX       the first version
 */
 
#include <stdio.h>
#include <string.h>


#define LOG_TAG                        "at.skt.m5311"
#include <at_log.h>


#if defined(AT_DEVICE_USING_M5311) && defined(AT_USING_SOCKET)

#define M5311_MODULE_SEND_MAX_SIZE       1024

/* set real event by current socket and current state */
#define SET_EVENT(socket, event)       (((socket + 1) << 16) | (event))

/* AT socket event type */
#define M5311_EVENT_CONN_OK              (1L << 0)
#define M5311_EVENT_SEND_OK              (1L << 1)
#define M5311_EVENT_RECV_OK              (1L << 2)
#define M5311_EVNET_CLOSE_OK             (1L << 3)
#define M5311_EVENT_CONN_FAIL            (1L << 4)
#define M5311_EVENT_SEND_FAIL            (1L << 5)

static at_evt_cb_t at_evt_cb_set[] =
{
    [AT_SOCKET_EVT_RECV]    = NULL,
    [AT_SOCKET_EVT_CLOSED]  = NULL,
};

static int m5311_socket_event_send(struct at_device *device, uint32_t event)
{
    return (int) rt_event_send(device->socket_event, event);
}

static int m5311_socket_event_recv(struct at_device *device, uint32_t event, uint32_t timeout, rt_uint8_t option)
{
    int result = 0;
    rt_uint32_t recved = 0;

    result = rt_event_recv(device->socket_event, event, option | RT_EVENT_FLAG_CLEAR, timeout, &recved);
    if (result != RT_EOK){
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
static int m5311_socket_close(struct at_socket *socket)
{
    int result = 0;
    at_response_t resp = RT_NULL;
    int device_socket = (int) socket->user_data;
    struct at_device *device  = (struct at_device *) socket->device;

    resp = at_create_resp(64, 0, rt_tick_from_millisecond(300));
    if (resp == RT_NULL){
        LOG_E("no memory for resp create.", device->name);
        return -RT_ENOMEM;
    }

    /* clear socket close event */
    m5311_socket_event_recv(device, SET_EVENT(device_socket, M5311_EVNET_CLOSE_OK), 0, RT_EVENT_FLAG_OR);

    if (at_obj_exec_cmd(device->client, resp, "AT+IPCLOSE=%d", device_socket) < 0){
        result = -RT_ERROR;
        goto __exit;
    }

    if (m5311_socket_event_recv(device, SET_EVENT(device_socket, M5311_EVNET_CLOSE_OK),
            rt_tick_from_millisecond(300 * 3), RT_EVENT_FLAG_AND) < 0)
    {
        LOG_E("%s device socket(%d) close failed, wait close OK timeout.", device->name, device_socket);
        result = -RT_ETIMEOUT;
        goto __exit;
    }

__exit:
    if (resp){
        at_delete_resp(resp);
    }
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
static int m5311_socket_connect(struct at_socket *socket, char *ip, int32_t port, enum at_socket_type type, rt_bool_t is_client)
{
    rt_bool_t retryed = RT_FALSE;
    at_response_t resp = RT_NULL;
    int result = 0, event_result = 0;
    int device_socket = (int) socket->user_data;
    struct at_device *device = (struct at_device *) socket->device;

    resp = at_create_resp(128, 0, rt_tick_from_millisecond(300));
    if (resp == RT_NULL)
    {
        LOG_E("no memory for resp create.");
        return -RT_ENOMEM;
    }

    RT_ASSERT(ip);
    RT_ASSERT(port >= 0);

__retry:

    /* clear socket connect event */
    event_result = SET_EVENT(device_socket, M5311_EVENT_CONN_OK | M5311_EVENT_CONN_FAIL);
    m5311_socket_event_recv(device, event_result, 0, RT_EVENT_FLAG_OR);

    if (is_client){
        switch (type){
            case AT_SOCKET_TCP:
                /* send AT commands(eg: AT+IPSTART=0,"TCP","x.x.x.x", 1234) to connect TCP server */
                /* AT+IPSTART=<sockid>,<type>,<addr>,<port>[,<cid>[,<domian>[,<protocol>]]] */
                if (at_obj_exec_cmd(device->client, resp,
                                "AT+IPSTART=%d,\"TCP\",\"%s\",%d", device_socket, ip, port) < 0)
                {
                    result = -RT_ERROR;
                    goto __exit;
                }
                break;

            case AT_SOCKET_UDP:
                if (at_obj_exec_cmd(device->client, resp,
                                "AT+IPSTART=%d,\"UDP\",\"%s\",%d", device_socket, ip, port) < 0)
                {
                    result = -RT_ERROR;
                    goto __exit;
                }
                break;

            default:
                LOG_E("%s device not supported connect type : %d.", device->name, type);
                return -RT_ERROR;
        }
    }

    /* waiting result event from AT URC, the device default connection timeout is 75 seconds, but it set to 10 seconds is convenient to use.*/
    if (m5311_socket_event_recv(device, SET_EVENT(device_socket, 0), 10 * RT_TICK_PER_SECOND, RT_EVENT_FLAG_OR) < 0)
    {
        LOG_E("%s device socket(%d) wait connect result timeout.", device->name, device_socket);
        result = -RT_ETIMEOUT;
        goto __exit;
    }
    /* waiting OK or failed result */
    if ((event_result = m5311_socket_event_recv(device, M5311_EVENT_CONN_OK | M5311_EVENT_CONN_FAIL,
            1 * RT_TICK_PER_SECOND, RT_EVENT_FLAG_OR)) < 0)
    {
        LOG_E("%s device socket(%d) wait connect OK|FAIL timeout.", device->name, device_socket);
        result = -RT_ETIMEOUT;
        goto __exit;
    }
    /* check result */
    if (event_result & M5311_EVENT_CONN_FAIL){
        if (retryed == RT_FALSE){
            LOG_D("%s device socket(%d) connect failed, the socket was not be closed and now will retry.",
                    device->name, device_socket);
            if (m5311_socket_close(socket) < 0){
                goto __exit;
            }
            retryed = RT_TRUE;
            goto __retry;
        }
        LOG_E("%s device socket(%d) connect failed.", device->name, device_socket);
        result = -RT_ERROR;
        goto __exit;
    }

__exit:
    if (resp){
        at_delete_resp(resp);
    }

    return result;
}
/*
 * 检查数据包收发情况
 * */
static int at_get_send_size(struct at_socket *socket,
                            size_t *sent,
                            size_t *received,
                            size_t *tx_buf_left,
                            size_t *unsent,
                            size_t *unacked)
{
    int result = 0;
    at_response_t resp = RT_NULL;
    int device_socket = (int) socket->user_data;
    struct at_device *device = (struct at_device *) socket->device;

    resp = at_create_resp(64, 0, 5 * RT_TICK_PER_SECOND);
    if (resp == RT_NULL){
        LOG_E("no memory for resp create.");
        result = -RT_ENOMEM;
        goto __exit;
    }

    if (at_obj_exec_cmd(device->client, resp, "AT+IPSACK=%d", device_socket) < 0){
        result = -RT_ERROR;
        goto __exit;
    }

    /* Response:<sent>,<received>[,<tx_buf_left>,<unsent>,<unacked>] */
    if (at_resp_parse_line_args_by_kw(resp, "+IPSACK:", "+IPSACK: %d, %d, %d, %d, %d", \
                                      sent, received, tx_buf_left,unsent,unacked) <= 0)
    {
        LOG_E("%s device prase \"AT+IPSACK\" cmd error.", device->name);
        result = -RT_ERROR;
        goto __exit;
    }

__exit:
    if (resp){
        at_delete_resp(resp);
    }

    return result;
}
/*
 * 等待发送完成
 * */
static int at_wait_send_finish(struct at_socket *socket, size_t settings_size)
{
    /* get the timeout by the input data size */
    rt_tick_t timeout = rt_tick_from_millisecond(settings_size);
    rt_tick_t last_time = rt_tick_get();
    size_t sent = 0, received = 0, tx_buf_left = 0, unsent = 0, unacked = 0;

    while (rt_tick_get() - last_time <= timeout){
        at_get_send_size(socket, &sent, &received, &tx_buf_left, &unsent, &unacked);
        if (tx_buf_left == 0){
            return RT_EOK;
        }
        rt_thread_mdelay(50);
    }

    return -RT_ETIMEOUT;
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
static int m5311_socket_send(struct at_socket *socket, const char *buff, size_t bfsz, enum at_socket_type type)
{
    int result = 0, event_result = 0;
    size_t pkt_size = 0, sent_size = 0;
    at_response_t resp = RT_NULL;
    int device_socket = (int) socket->user_data;
    struct at_device *device = (struct at_device *) socket->device;
    struct at_device_m5311 *m5311 = (struct at_device_m5311 *) device->user_data;
    rt_mutex_t lock = device->client->lock;

    RT_ASSERT(buff);

    resp = at_create_resp(128, 2, 5 * RT_TICK_PER_SECOND);
    if (resp == RT_NULL){
        LOG_E("no memory for resp create(m5311).");
        return -RT_ENOMEM;
    }

    rt_mutex_take(lock, RT_WAITING_FOREVER);

    /* Clear socket send event */
    event_result = SET_EVENT(device_socket, M5311_EVENT_SEND_OK | M5311_EVENT_SEND_FAIL);
    m5311_socket_event_recv(device, event_result, 0, RT_EVENT_FLAG_OR);

    /* set current socket for send URC event */
    m5311->user_data = (void *) device_socket;
    /* set AT client end sign to deal with '>' sign.*/
    at_obj_set_end_sign(device->client, '>');

    while (sent_size < bfsz){
        if (bfsz - sent_size < M5311_MODULE_SEND_MAX_SIZE){
            pkt_size = bfsz - sent_size;
        }else{
            pkt_size = M5311_MODULE_SEND_MAX_SIZE;
        }

        /* send the "AT+IPSEND" commands to AT server include string payload. */
        if (at_obj_exec_cmd(device->client, resp, "AT+IPSEND=%d,%d,\"%s\"", device_socket, pkt_size, buff) < 0){
            result = -RT_ERROR;
            goto __exit;
        }

        /* send the real data to server or client
        result = (int) at_client_obj_send(device->client, buff + sent_size, pkt_size);
        if (result == 0){
            result = -RT_ERROR;
            goto __exit;
        }
        */

        /* waiting result event from AT URC */
        if (m5311_socket_event_recv(device, SET_EVENT(device_socket, 0), 15 * RT_TICK_PER_SECOND, RT_EVENT_FLAG_OR) < 0){
            LOG_E("%s device socket(%d) wait send result timeout.", device->name, device_socket);
            result = -RT_ETIMEOUT;
            goto __exit;
        }
        /* waiting OK or failed result */
        if ((event_result = m5311_socket_event_recv(device, M5311_EVENT_SEND_OK | M5311_EVENT_SEND_FAIL,
                1 * RT_TICK_PER_SECOND, RT_EVENT_FLAG_OR)) < 0)
        {
            LOG_E("%s device socket(%d) wait send OK|FAIL timeout.", device->name, device_socket);
            result = -RT_ETIMEOUT;
            goto __exit;
        }
        /* check result */
        if (event_result & M5311_EVENT_SEND_FAIL){
            LOG_E("%s device socket(%d) send failed.", device->name, device_socket);
            result = -RT_ERROR;
            goto __exit;
        }

        if (type == AT_SOCKET_TCP){
            at_wait_send_finish(socket, pkt_size);
        }

        sent_size += pkt_size;
    }

__exit:
    /* reset the end sign for data conflict */
    at_obj_set_end_sign(device->client, 0);

    rt_mutex_release(lock);

    if (resp){
        at_delete_resp(resp);
    }

    return result > 0 ? sent_size : result;
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
static int m5311_domain_resolve(const char *name, char ip[32])
{
#define RESOLVE_RETRY                  5

    int i, result = RT_EOK;
    char recv_ip[32] = { 0 };
    at_response_t resp = RT_NULL;
    struct at_device *device = RT_NULL;

    RT_ASSERT(name);
    RT_ASSERT(ip);

    device = at_device_get_first_initialized();
    if (device == RT_NULL){
        LOG_E("get first init device failed.");
        return -RT_ERROR;
    }

    /* The maximum response time is 14 seconds, affected by network status */
    resp = at_create_resp(128, 4, 14 * RT_TICK_PER_SECOND);
    if (resp == RT_NULL){
        LOG_E("no memory for resp create.");
        return -RT_ENOMEM;
    }

    for(i = 0; i < RESOLVE_RETRY; i++){
        if (at_obj_exec_cmd(device->client, resp, "AT+CMDNS=\"%s\"", name) < 0){
            result = -RT_ERROR;
            goto __exit;
        }

        /* parse the third line of response data, get the IP address */
        if(at_resp_parse_line_args_by_kw(resp, "+CMDNS: %s", recv_ip) < 0){
            rt_thread_mdelay(100);
            /* resolve failed, maybe receive an URC CRLF */
            continue;
        }

        if (rt_strlen(recv_ip) < 8){
            rt_thread_mdelay(100);
            /* resolve failed, maybe receive an URC CRLF */
            continue;
        }else{
            rt_thread_mdelay(10);
            rt_strncpy(ip, recv_ip, 32);
            ip[32] = '\0';
            break;
        }
    }

__exit:
    if (resp){
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
static void m5311_socket_set_event_cb(at_socket_evt_t event, at_evt_cb_t cb)
{
    if (event < sizeof(at_evt_cb_set) / sizeof(at_evt_cb_set[1])){
        at_evt_cb_set[event] = cb;
    }
}

static void urc_connect_func(struct at_client *client, const char *data, rt_size_t size)
{
    int device_socket = 0;
    struct at_device *device = RT_NULL;
    char *client_name = client->device->parent.name;

    RT_ASSERT(data && size);

    device = at_device_get_by_name(AT_DEVICE_NAMETYPE_CLIENT, client_name);
    if (device == RT_NULL){
        LOG_E("get device(%s) failed.", client_name);
        return;
    }

    sscanf(data, "%d%*[^0-9]", &device_socket);

    if (rt_strstr(data, "CONNECT OK")){
        m5311_socket_event_send(device, SET_EVENT(device_socket, M5311_EVENT_CONN_OK));
    }else{
        m5311_socket_event_send(device, SET_EVENT(device_socket, M5311_EVENT_CONN_FAIL));
    }
}

static void urc_send_func(struct at_client *client, const char *data, rt_size_t size)
{
    int device_socket = 0;
    struct at_device *device = RT_NULL;
    struct at_device_m5311 *m5311 = RT_NULL;
    char *client_name = client->device->parent.name;

    RT_ASSERT(data && size);

    device = at_device_get_by_name(AT_DEVICE_NAMETYPE_CLIENT, client_name);
    if (device == RT_NULL){
        LOG_E("get device(%s) failed.", client_name);
        return;
    }
    m5311 = (struct at_device_m5311 *) device->user_data;
    device_socket = (int) m5311->user_data;

    if (rt_strstr(data, "SEND OK")){
        m5311_socket_event_send(device, SET_EVENT(device_socket, M5311_EVENT_SEND_OK));
    }
    else if (rt_strstr(data, "SEND FAIL")){
        m5311_socket_event_send(device, SET_EVENT(device_socket, M5311_EVENT_SEND_FAIL));
    }
}

static void urc_close_func(struct at_client *client, const char *data, rt_size_t size)
{
    int device_socket = 0;
    struct at_device *device = RT_NULL;
    char *client_name = client->device->parent.name;

    RT_ASSERT(data && size);

    device = at_device_get_by_name(AT_DEVICE_NAMETYPE_CLIENT, client_name);
    if (device == RT_NULL){
        LOG_E("get device(%s) failed.", client_name);
        return;
    }

    sscanf(data, "%d%*s", &device_socket);

    if (rt_strstr(data, "CLOSE OK")){
        m5311_socket_event_send(device, SET_EVENT(device_socket, M5311_EVNET_CLOSE_OK));
    }else if (rt_strstr(data, "CLOSED")){
        struct at_socket *socket = RT_NULL;

        /* get at socket object by device socket descriptor */
        socket = &(device->sockets[device_socket]);

        /* notice the socket is disconnect by remote */
        if (at_evt_cb_set[AT_SOCKET_EVT_CLOSED]){
            at_evt_cb_set[AT_SOCKET_EVT_CLOSED](socket, AT_SOCKET_EVT_CLOSED, NULL, 0);
        }
    }
}

static void urc_recv_func(struct at_client *client, const char *data, rt_size_t size)
{
    int device_socket = 0;
    rt_int32_t timeout;
    rt_size_t bfsz = 0, temp_size = 0;
    char *recv_buf = RT_NULL, temp[8] = {0};
    struct at_socket *socket = RT_NULL;
    struct at_device *device = RT_NULL;
    char *client_name = client->device->parent.name;

    RT_ASSERT(data && size);

    device = at_device_get_by_name(AT_DEVICE_NAMETYPE_CLIENT, client_name);
    if (device == RT_NULL){
        LOG_E("get device(%s) failed.", client_name);
        return;
    }

    /* get the current socket and receive buffer size by receive data */
    sscanf(data, "+RECEIVE: %d, %d", &device_socket, (int *) &bfsz);

    /* set receive timeout by receive buffer length, not less than 10 ms */
    timeout = bfsz > 10 ? bfsz : 10;

    if (device_socket < 0 || bfsz == 0){
        return;
    }

    recv_buf = (char *) rt_calloc(1, bfsz);
    if (recv_buf == RT_NULL){
        LOG_E("no memory for receive buffer (%d).", device->name, bfsz);
        /* read and clean the coming data */
        while (temp_size < bfsz){
            if (bfsz - temp_size > sizeof(temp)){
                at_client_obj_recv(client, temp, sizeof(temp), timeout);
            }else{
                at_client_obj_recv(client, temp, bfsz - temp_size, timeout);
            }
            temp_size += sizeof(temp);
        }
        return;
    }

    /* sync receive data */
    if (at_client_obj_recv(client, recv_buf, bfsz, timeout) != bfsz){
        LOG_E("%s device receive size(%d) data failed.",  device->name, bfsz);
        rt_free(recv_buf);
        return;
    }

    /* get at socket object by device socket descriptor */
    socket = &(device->sockets[device_socket]);

    /* notice the receive buffer and buffer size */
    if (at_evt_cb_set[AT_SOCKET_EVT_RECV]){
        at_evt_cb_set[AT_SOCKET_EVT_RECV](socket, AT_SOCKET_EVT_RECV, recv_buf, bfsz);
    }
}

static const struct at_urc urc_table[] =
{
    {"",            ", CONNECT OK\r\n",     urc_connect_func},
    {"",            ", CONNECT FAIL\r\n",   urc_connect_func},
    {"SEND OK",     "\r\n",                 urc_send_func},
    {"SEND FAIL",   "\r\n",                 urc_send_func},
    {"",            ", CLOSE OK\r\n",       urc_close_func},
    {"",            ", CLOSED\r\n",         urc_close_func},
    {"+RECEIVE:",   "\r\n",                 urc_recv_func},
};

static const struct at_socket_ops m5311_socket_ops =
{
    m5311_socket_connect,
    m5311_socket_close,
    m5311_socket_send,
    m5311_domain_resolve,
    m5311_socket_set_event_cb,
};

int m5311_socket_init(struct at_device *device)
{
    RT_ASSERT(device);

    /* register URC data execution function  */
    at_obj_set_urc_table(device->client, urc_table, sizeof(urc_table) / sizeof(urc_table[0]));

    return RT_EOK;
}

int m5311_socket_class_register(struct at_device_class *class)
{
    RT_ASSERT(class);

    class->socket_num = AT_DEVICE_M5311_SOCKETS_NUM;
    class->socket_ops = &m5311_socket_ops;

    return RT_EOK;
}

#endif /* AT_DEVICE_USING_M5311 && AT_USING_SOCKET */

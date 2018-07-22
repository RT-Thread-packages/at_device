/*
 * File      : at_socket_m26.c
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
 * 2018-06-12     chenyong     first version
 */
 
#include <stdio.h>
#include <string.h>

#include <rtthread.h>
#include <sys/socket.h>

#include <at.h>
#include <at_socket.h>

#ifndef AT_DEVICE_NOT_SELECTED

#define M26_MODULE_SEND_MAX_SIZE       1460

/* set real event by current socket and current state */
#define SET_EVENT(socket, event)       (((socket + 1) << 16) | (event))

/* AT socket event type */
#define M26_EVENT_CONN_OK              (1L << 0)
#define M26_EVENT_SEND_OK              (1L << 1)
#define M26_EVENT_RECV_OK              (1L << 2)
#define M26_EVNET_CLOSE_OK             (1L << 3)
#define M26_EVENT_CONN_FAIL            (1L << 4)
#define M26_EVENT_SEND_FAIL            (1L << 5)

static int cur_socket;
static rt_event_t at_socket_event;
static rt_mutex_t at_event_lock;
static at_evt_cb_t at_evt_cb_set[] = {
        [AT_SOCKET_EVT_RECV] = NULL,
        [AT_SOCKET_EVT_CLOSED] = NULL,
};

static int at_socket_event_send(uint32_t event)
{
    return (int) rt_event_send(at_socket_event, event);
}

static int at_socket_event_recv(uint32_t event, uint32_t timeout, rt_uint8_t option)
{
    int result = 0;
    rt_uint32_t recved;

    result = rt_event_recv(at_socket_event, event, option | RT_EVENT_FLAG_CLEAR, timeout, &recved);
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
static int m26_socket_close(int socket)
{
    int result = 0;

    rt_mutex_take(at_event_lock, RT_WAITING_FOREVER);
    cur_socket = socket;

    if (at_exec_cmd(RT_NULL, "AT+QICLOSE=%d", socket) < 0)
    {
        result = -RT_ERROR;
        goto __exit;
    }

    if (at_socket_event_recv(SET_EVENT(socket, M26_EVNET_CLOSE_OK), rt_tick_from_millisecond(300*3), RT_EVENT_FLAG_AND) < 0)
    {
        LOG_E("socket (%d) close failed, wait close OK timeout.", socket);
        result = -RT_ETIMEOUT;
        goto __exit;
    }

__exit:
    rt_mutex_release(at_event_lock);

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
static int m26_socket_connect(int socket, char *ip, int32_t port, enum at_socket_type type, rt_bool_t is_client)
{
    int result = 0, event_result = 0;
    rt_bool_t retryed = RT_FALSE;

    RT_ASSERT(ip);
    RT_ASSERT(port >= 0);

    /* lock AT socket connect */
    rt_mutex_take(at_event_lock, RT_WAITING_FOREVER);

__retry:

    if (is_client)
    {
        switch (type)
        {
        case AT_SOCKET_TCP:
            /* send AT commands(eg: AT+QIOPEN=0,"TCP","x.x.x.x", 1234) to connect TCP server */
            if (at_exec_cmd(RT_NULL, "AT+QIOPEN=%d,\"TCP\",\"%s\",%d", socket, ip, port) < 0)
            {
                result = -RT_ERROR;
                goto __exit;
            }
            break;

        case AT_SOCKET_UDP:
            if (at_exec_cmd(RT_NULL, "AT+QIOPEN=%d,\"UDP\",\"%s\",%d", socket, ip, port) < 0)
            {
                result = -RT_ERROR;
                goto __exit;
            }
            break;

        default:
            LOG_E("Not supported connect type : %d.", type);
            return -RT_ERROR;
        }
    }

    /* waiting result event from AT URC, the device default connection timeout is 75 seconds, but it set to 10 seconds is convenient to use.*/
    if (at_socket_event_recv(SET_EVENT(socket, 0), rt_tick_from_millisecond(10 * 1000), RT_EVENT_FLAG_OR) < 0)
    {
        LOG_E("socket (%d) connect failed, wait connect result timeout.", socket);
        result = -RT_ETIMEOUT;
        goto __exit;
    }
    /* waiting OK or failed result */
    if ((event_result = at_socket_event_recv(M26_EVENT_CONN_OK | M26_EVENT_CONN_FAIL, rt_tick_from_millisecond(1 * 1000),
            RT_EVENT_FLAG_OR)) < 0)
    {
        LOG_E("socket (%d) connect failed, wait connect OK|FAIL timeout.", socket);
        result = -RT_ETIMEOUT;
        goto __exit;
    }
    /* check result */
    if (event_result & M26_EVENT_CONN_FAIL)
    {
        if (!retryed)
        {
            LOG_E("socket (%d) connect failed, maybe the socket was not be closed at the last time and now will retry.", socket);
            if (m26_socket_close(socket) < 0)
            {
                goto __exit;
            }
            retryed = RT_TRUE;
            goto __retry;
        }
        LOG_E("socket (%d) connect failed, failed to establish a connection.", socket);
        result = -RT_ERROR;
        goto __exit;
    }

__exit:
    /* unlock AT socket connect */
    rt_mutex_release(at_event_lock);

    return result;
}

static int at_get_send_size(int socket, size_t *size, size_t *acked, size_t *nacked)
{
    at_response_t resp = at_create_resp(64, 0, rt_tick_from_millisecond(5000));
    int result = 0;

    if (!resp)
    {
        LOG_E("No memory for response structure!");
        result = -RT_ENOMEM;
        goto __exit;
    }

    if (at_exec_cmd(resp, "AT+QISACK=%d", socket) < 0)
    {
        result = -RT_ERROR;
        goto __exit;
    }

    if (at_resp_parse_line_args(resp, 2, "+QISACK: %d, %d, %d", size, acked, nacked) <= 0)
    {
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

static int at_wait_send_finish(int socket, size_t settings_size)
{
    rt_tick_t timeout = rt_tick_from_millisecond(settings_size);
    rt_tick_t last_time = rt_tick_get();
    size_t size = 0, acked = 0, nacked = 0xFFFF;

    while (rt_tick_get() - last_time <= timeout)
    {
        at_get_send_size(socket, &size, &acked, &nacked);
        if (nacked == 0)
        {
            return RT_EOK;
        }
        rt_thread_delay(rt_tick_from_millisecond(50));
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
static int m26_socket_send(int socket, const char *buff, size_t bfsz, enum at_socket_type type)
{
    int result = 0, event_result = 0;
    at_response_t resp = RT_NULL;
    size_t cur_pkt_size = 0, sent_size = 0;

    RT_ASSERT(buff);

    resp = at_create_resp(128, 2, rt_tick_from_millisecond(5000));
    if (!resp)
    {
        LOG_E("No memory for response structure!");
        return -RT_ENOMEM;
    }

    rt_mutex_take(at_event_lock, RT_WAITING_FOREVER);

    /* set current socket for send URC event */
    cur_socket = socket;
    /* set AT client end sign to deal with '>' sign.*/
    extern int at_set_end_sign(char ch);
    at_set_end_sign('>');

    while (sent_size < bfsz)
    {
        if (bfsz - sent_size < M26_MODULE_SEND_MAX_SIZE)
        {
            cur_pkt_size = bfsz - sent_size;
        }
        else
        {
            cur_pkt_size = M26_MODULE_SEND_MAX_SIZE;
        }

        /* send the "AT+QISEND" commands to AT server than receive the '>' response on the first line. */
        if (at_exec_cmd(resp, "AT+QISEND=%d,%d", socket, cur_pkt_size) < 0)
        {
            result = -RT_ERROR;
            goto __exit;
        }

        /* send the real data to server or client */
        result = (int) at_client_send(buff + sent_size, cur_pkt_size);
        if (result == 0)
        {
            result = -RT_ERROR;
            goto __exit;
        }

        /* waiting result event from AT URC */
        if (at_socket_event_recv(SET_EVENT(socket, 0), rt_tick_from_millisecond(300*3), RT_EVENT_FLAG_OR) < 0)
        {
            LOG_E("socket (%d) send failed, wait connect result timeout.", socket);
            result = -RT_ETIMEOUT;
            goto __exit;
        }
        /* waiting OK or failed result */
        if ((event_result = at_socket_event_recv(M26_EVENT_SEND_OK | M26_EVENT_SEND_FAIL, rt_tick_from_millisecond(1 * 1000),
                RT_EVENT_FLAG_OR)) < 0)
        {
            LOG_E("socket (%d) send failed, wait connect OK|FAIL timeout.", socket);
            result = -RT_ETIMEOUT;
            goto __exit;
        }
        /* check result */
        if (event_result & M26_EVENT_SEND_FAIL)
        {
            LOG_E("socket (%d) send failed, return failed.", socket);
            result = -RT_ERROR;
            goto __exit;
        }

        if (type == AT_SOCKET_TCP)
        {
            at_wait_send_finish(socket, cur_pkt_size);
        }

        sent_size += cur_pkt_size;
    }


__exit:
    /* reset the end sign for data conflict */
    at_set_end_sign(0);

    rt_mutex_release(at_event_lock);

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
static int m26_domain_resolve(const char *name, char ip[16])
{
    int result = 0;
    char recv_ip[16] = { 0 };
    at_response_t resp = RT_NULL;

    RT_ASSERT(name);
    RT_ASSERT(ip);

    resp = at_create_resp(128, 4, rt_tick_from_millisecond(5000));
    if (!resp)
    {
        LOG_E("No memory for response structure!");
        return -RT_ENOMEM;
    }

    rt_mutex_take(at_event_lock, RT_WAITING_FOREVER);

__restart:
    if (at_exec_cmd(resp, "AT+QIDNSGIP=\"%s\"", name) < 0)
    {
        result = -RT_ERROR;
        goto __exit;
    }

    /* parse the third line of response data, get the IP address */
    at_resp_parse_line_args(resp, 4, "%s", recv_ip);

    if (strlen(recv_ip) < 8)
    {
        rt_thread_delay(rt_tick_from_millisecond(100));
        /* resolve failed, maybe receive an URC CRLF */
        goto __restart;
    }

    strncpy(ip, recv_ip, 15);
    ip[15] = '\0';

__exit:
    rt_mutex_release(at_event_lock);

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
static void m26_socket_set_event_cb(at_socket_evt_t event, at_evt_cb_t cb)
{
    if (event < sizeof(at_evt_cb_set) / sizeof(at_evt_cb_set[1]))
    {
        at_evt_cb_set[event] = cb;
    }
}

static void urc_connect_func(const char *data, rt_size_t size)
{
    int socket = 0;

    RT_ASSERT(data && size);

    sscanf(data, "%d%*[^0-9]", &socket);

    if (strstr(data, "CONNECT OK"))
    {
        at_socket_event_send(SET_EVENT(socket, M26_EVENT_CONN_OK));
    }
    else
    {
        at_socket_event_send(SET_EVENT(socket, M26_EVENT_CONN_FAIL));
    }
}

static void urc_send_func(const char *data, rt_size_t size)
{
    RT_ASSERT(data && size);

    if (strstr(data, "SEND OK"))
    {
        at_socket_event_send(SET_EVENT(cur_socket, M26_EVENT_SEND_OK));
    }
    else if (strstr(data, "SEND FAIL"))
    {
        at_socket_event_send(SET_EVENT(cur_socket, M26_EVENT_SEND_FAIL));
    }
}

static void urc_close_func(const char *data, rt_size_t size)
{
    int socket = 0;

    RT_ASSERT(data && size);

    if (strstr(data, "CLOSE OK"))
    {
        at_socket_event_send(SET_EVENT(cur_socket, M26_EVNET_CLOSE_OK));
    }
    else if (strstr(data, "CLOSED"))
    {
        sscanf(data, "%d, CLOSED", &socket);
        /* notice the socket is disconnect by remote */
        if (at_evt_cb_set[AT_SOCKET_EVT_CLOSED])
        {
            at_evt_cb_set[AT_SOCKET_EVT_CLOSED](socket, AT_SOCKET_EVT_CLOSED, NULL, 0);
        }
    }
}

static void urc_recv_func(const char *data, rt_size_t size)
{
    int socket = 0;
    rt_size_t bfsz = 0, temp_size = 0;
    char *recv_buf = RT_NULL, temp[8];

    RT_ASSERT(data && size);

    /* get the current socket and receive buffer size by receive data */
    sscanf(data, "+RECEIVE: %d, %d", &socket, (int *) &bfsz);

    if (socket < 0 || bfsz == 0)
        return;

    recv_buf = rt_calloc(1, bfsz);
    if (!recv_buf)
    {
        LOG_E("no memory for URC receive buffer (%d)!", bfsz);
        /* read and clean the coming data */
        while (temp_size < bfsz)
        {
            if (bfsz - temp_size > sizeof(temp))
            {
                at_client_recv(temp, sizeof(temp));
            }
            else
            {
                at_client_recv(temp, bfsz - temp_size);
            }
            temp_size += sizeof(temp);
        }
        return;
    }

    /* sync receive data */
    if (at_client_recv(recv_buf, bfsz) != bfsz)
    {
        LOG_E("receive size(%d) data failed!", bfsz);
        rt_free(recv_buf);
        return;
    }

    /* notice the receive buffer and buffer size */
    if (at_evt_cb_set[AT_SOCKET_EVT_RECV])
    {
        at_evt_cb_set[AT_SOCKET_EVT_RECV](socket, AT_SOCKET_EVT_RECV, recv_buf, bfsz);
    }
}

static void urc_ping_func(const char *data, rt_size_t size)
{
    static int icmp_seq = 0;
    int result, recv_len, time, ttl;
    char dst_ip[16] = { 0 };

    RT_ASSERT(data);

    sscanf(data, "+QPING: %d,%[^,],%d,%d,%d", &result, dst_ip, &recv_len, &time, &ttl);

    switch(result)
    {
    case 0:
        rt_kprintf("%d bytes from %s icmp_seq=%d ttl=%d time=%d ms\n", recv_len, dst_ip, icmp_seq++, ttl, time);
        break;
    case 1:
        rt_kprintf("ping request timeout!\n");
        break;
    case 2:
        icmp_seq = 0;
        break;
    case 3:
        rt_kprintf("ping: TCP/IP protocol stack is busy\n");
        break;
    case 4:
        rt_kprintf("ping: unknown remote server host\n");
        break;
    default:
        break;
    }

}

static void urc_func(const char *data, rt_size_t size)
{
    RT_ASSERT(data);

    LOG_I("URC data : %.*s", size, data);
}

static const struct at_urc urc_table[] = {
        {"RING",        "\r\n",                 urc_func},
        {"Call Ready",  "\r\n",                 urc_func},
        {"RDY",         "\r\n",                 urc_func},
        {"NO CARRIER",  "\r\n",                 urc_func},
        {"",            ", CONNECT OK\r\n",     urc_connect_func},
        {"",            ", CONNECT FAIL\r\n",   urc_connect_func},
        {"SEND OK",     "\r\n",                 urc_send_func},
        {"SEND FAIL",   "\r\n",                 urc_send_func},
        {"",            ", CLOSE OK\r\n",       urc_close_func},
        {"",            ", CLOSED\r\n",         urc_close_func},
        {"+RECEIVE:",   "\r\n",                 urc_recv_func},
        {"+QPING:",     "\r\n",                 urc_ping_func},
};

/* AT client port initialization */
int at_client_port_init(void)
{
    /* create current AT socket event */
    at_socket_event = rt_event_create("at_sock_event", RT_IPC_FLAG_FIFO);
    if (!at_socket_event)
    {
        LOG_E("AT client port initialize failed! at_sock_event create failed!");
        return -RT_ENOMEM;
    }

    /* create current AT socket lock */
    at_event_lock = rt_mutex_create("at_event_lock", RT_IPC_FLAG_FIFO);
    if (!at_event_lock)
    {
        LOG_E("AT client port initialize failed! at_sock_lock create failed!");
        rt_event_delete(at_socket_event);
        return -RT_ENOMEM;
    }

    /* register URC data execution function  */
    at_set_urc_table(urc_table, sizeof(urc_table) / sizeof(urc_table[0]));

    return RT_EOK;
}

#define AT_SEND_CMD(resp, resp_line, cmd)                                                                       \
    do                                                                                                          \
    {                                                                                                           \
        if (at_exec_cmd(at_resp_set_info(resp, 64, resp_line, rt_tick_from_millisecond(5000)), cmd) < 0)        \
        {                                                                                                       \
            return -RT_ERROR;                                                                                   \
        }                                                                                                       \
    } while(0);                                                                                                 \

int m26_net_init(void)
{
#define CSQ_RETRY                      10
#define CREG_RETRY                     10
#define CGREG_RETRY                    10

    at_response_t resp = RT_NULL;
    int i, qimux, qimode;
    char parsed_data[10];

    resp = at_create_resp(64, 0, rt_tick_from_millisecond(5000));
    if (!resp)
    {
        LOG_E("No memory for response structure!");
        return -RT_ENOMEM;
    }
    LOG_D("Start initializing the M26 module");
    /* wait M26 startup finish */
    rt_thread_delay(rt_tick_from_millisecond(8000));
    /* disable echo */
    AT_SEND_CMD(resp, 0, "ATE0");
    /* get module version */
    AT_SEND_CMD(resp, 0, "ATI");
    /* show module version */
    for (i = 0; i < resp->line_counts - 1; i++)
    {
        LOG_D("%s", at_resp_get_line(resp, i + 1))
    }
    /* check SIM card */
    AT_SEND_CMD(resp, 2, "AT+CPIN?");
    if (!at_resp_get_line_by_kw(resp, "READY"))
    {
        LOG_E("SIM card detection failed");
        return -RT_ERROR;
    }
    /* waiting for dirty data to be digested */
    rt_thread_delay(rt_tick_from_millisecond(10));
    /* check signal strength */
    for (i = 0; i < CSQ_RETRY; i++)
    {
        AT_SEND_CMD(resp, 0, "AT+CSQ");
        at_resp_parse_line_args(resp, 2, "+CSQ: %s", &parsed_data);
        if (strncmp(parsed_data, "99,99", sizeof(parsed_data)))
        {
            LOG_D("Signal strength: %s", parsed_data);
            break;
        }
        rt_thread_delay(rt_tick_from_millisecond(1000));
    }
    if (i == CSQ_RETRY)
    {
        LOG_E("Signal strength check failed (%s)", parsed_data);
        return -RT_ERROR;
    }
    /* check the GSM network is registered */
    for (i = 0; i < CREG_RETRY; i++)
    {
        AT_SEND_CMD(resp, 0, "AT+CREG?");
        at_resp_parse_line_args(resp, 2, "+CREG: %s", &parsed_data);
        if (!strncmp(parsed_data, "0,1", sizeof(parsed_data)) || !strncmp(parsed_data, "0,5", sizeof(parsed_data)))
        {
            LOG_D("GSM network is registered (%s)", parsed_data);
            break;
        }
        rt_thread_delay(rt_tick_from_millisecond(1000));
    }
    if (i == CREG_RETRY)
    {
        LOG_E("The GSM network is register failed (%s)", parsed_data);
        return -RT_ERROR;
    }
    /* check the GPRS network is registered */
    for (i = 0; i < CGREG_RETRY; i++)
    {
        AT_SEND_CMD(resp, 0, "AT+CGREG?");
        at_resp_parse_line_args(resp, 2, "+CGREG: %s", &parsed_data);
        if (!strncmp(parsed_data, "0,1", sizeof(parsed_data)) || !strncmp(parsed_data, "0,5", sizeof(parsed_data)))
        {
            LOG_D("GPRS network is registered (%s)", parsed_data);
            break;
        }
        rt_thread_delay(rt_tick_from_millisecond(1000));
    }
    if (i == CGREG_RETRY)
    {
        LOG_E("The GPRS network is register failed (%s)", parsed_data);
        return -RT_ERROR;
    }

    AT_SEND_CMD(resp, 0, "AT+QIFGCNT=0");
    AT_SEND_CMD(resp, 0, "AT+QICSGP=1, \"CMNET\"");
    AT_SEND_CMD(resp, 0, "AT+QIMODE?");

    at_resp_parse_line_args(resp, 2, "+QIMODE: %d", &qimode);
    if (qimode == 1)
    {
        AT_SEND_CMD(resp, 0, "AT+QIMODE=0");
    }
    /* Set to multiple connections */
    AT_SEND_CMD(resp, 0, "AT+QIMUX?");
    at_resp_parse_line_args(resp, 2, "+QIMUX: %d", &qimux);
    if (qimux == 0)
    {
        AT_SEND_CMD(resp, 0, "AT+QIMUX=1");
    }

    AT_SEND_CMD(resp, 2, "AT+QIDEACT");
    AT_SEND_CMD(resp, 0, "AT+QIREGAPP");
    AT_SEND_CMD(resp, 0, "AT+QIACT");
    AT_SEND_CMD(resp, 2, "AT+QILOCIP");

    if (resp)
    {
        at_delete_resp(resp);
    }

    LOG_I("AT network initialize success!");

    return RT_EOK;
}

int m26_ping(int argc, char **argv)
{
    at_response_t resp = RT_NULL;

    if (argc != 2)
    {
        rt_kprintf("Please input: at_ping <host address>\n");
        return -RT_ERROR;
    }

    resp = at_create_resp(64, 0, rt_tick_from_millisecond(5000));
    if (!resp)
    {
        rt_kprintf("No memory for response structure!\n");
        return -RT_ENOMEM;
    }

    if (at_exec_cmd(resp, "AT+QPING=\"%s\"", argv[1]) < 0)
    {
        rt_kprintf("AT send ping commands error!\n");
        return -RT_ERROR;
    }

    if (resp)
    {
        at_delete_resp(resp);
    }

    return RT_EOK;
}

#ifdef FINSH_USING_MSH
#include <finsh.h>
MSH_CMD_EXPORT_ALIAS(m26_net_init, at_net_init, initialize AT network);
MSH_CMD_EXPORT_ALIAS(m26_ping, at_ping, AT ping network host);
#endif

static const struct at_device_ops m26_socket_ops = {
    .connect =              m26_socket_connect,
    .close =                m26_socket_close,
    .send =                 m26_socket_send,
    .domain_resolve =       m26_domain_resolve,
    .set_event_cb =         m26_socket_set_event_cb,
};

static int at_socket_device_init(void)
{
    m26_net_init();

    at_scoket_device_register(&m26_socket_ops);

    return 0;
}
INIT_APP_EXPORT(at_socket_device_init);

#endif /* AT_DEVICE_NOT_SELECTED */

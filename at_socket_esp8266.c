/*
 * File      : at_socket_esp8266.c
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
 * 2018-06-20     chenyong     first version
 */
 
#include <stdio.h>
#include <string.h>

#include <rtthread.h>
#include <sys/socket.h>

#include <rt_at.h>
#include <at_socket.h>

#ifndef AT_DEVICE_NOT_SELECTED

#define ESP8266_MODULE_SEND_MAX_SIZE   2048

/* set real event by current socket and current state */
#define SET_EVENT(socket, event)       (((socket + 1) << 16) | (event))

/* AT socket event type */
#define ESP8266_EVENT_CONN_OK          (1L << 0)
#define ESP8266_EVENT_SEND_OK          (1L << 1)
#define ESP8266_EVENT_RECV_OK          (1L << 2)
#define ESP8266_EVNET_CLOSE_OK         (1L << 3)
#define ESP8266_EVENT_CONN_FAIL        (1L << 4)
#define ESP8266_EVENT_SEND_FAIL        (1L << 5)

static int cur_socket;
static int cur_send_bfsz;
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
    uint32_t recved;

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
static int esp8266_socket_close(int socket)
{
    rt_at_response_t resp = RT_NULL;

    resp = rt_at_create_resp(64, 0, rt_tick_from_millisecond(5000));
    if (!resp)
    {
        LOG_E("No memory for response structure!");
        return -RT_ENOMEM;
    }

    if (rt_at_exec_cmd(resp, "AT+CIPCLOSE=%d", socket) < 0)
    {
        LOG_E("socket(%d) close failed.", socket);
        rt_at_delete_resp(resp);
        return -RT_ERROR;
    }

    if (resp)
    {
        rt_at_delete_resp(resp);
    }

    return RT_EOK;
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
static int esp8266_socket_connect(int socket, char *ip, int32_t port, enum at_socket_type type, rt_bool_t is_client)
{
    rt_at_response_t resp = RT_NULL;
    int result = RT_EOK;
    rt_bool_t retryed = RT_FALSE;

    RT_ASSERT(ip);
    RT_ASSERT(port >= 0);

    resp = rt_at_create_resp(128, 0, rt_tick_from_millisecond(5000));
    if (!resp)
    {
        LOG_E("No memory for response structure!");
        return -RT_ENOMEM;
    }

__retry:

    if (is_client)
    {
        switch (type)
        {
        case AT_SOCKET_TCP:
            /* send AT commands to connect TCP server */
            if (rt_at_exec_cmd(resp, "AT+CIPSTART=%d,\"TCP\",\"%s\",%d,60", socket, ip, port) < 0)
            {
                result = -RT_ERROR;
            }
            break;

        case AT_SOCKET_UDP:
            if (rt_at_exec_cmd(resp, "AT+CIPSTART=%d,\"UDP\",\"%s\",%d", socket, ip, port) < 0)
            {
                result = -RT_ERROR;
            }
            break;

        default:
            LOG_E("Not supported connect type : %d.", type);
            result = -RT_ERROR;
            goto __exit;
        }
    }

    if (result != RT_EOK && !retryed)
    {
        LOG_D("udp socket (%d) connect failed, maybe the socket was not be closed at the last time and now will retry.", socket);
        if (esp8266_socket_close(socket) < 0)
        {
            goto __exit;
        }
        retryed = RT_TRUE;
        result = RT_EOK;
        goto __retry;
    }

__exit:

    if (result != RT_EOK)
    {
        LOG_E("socket (%d) connect failed, failed to establish a connection.", socket);
    }
    if (resp)
    {
        rt_at_delete_resp(resp);
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
static int esp8266_socket_send(int socket, const char *buff, size_t bfsz, enum at_socket_type type)
{
    int result = RT_EOK;
    int event_result = 0;
    rt_at_response_t resp = RT_NULL;
    size_t cur_pkt_size = 0, sent_size = 0;

    RT_ASSERT(buff);
    RT_ASSERT(bfsz > 0);

    resp = rt_at_create_resp(128, 2, rt_tick_from_millisecond(5000));
    if (!resp)
    {
        LOG_E("No memory for response structure!");
        return -RT_ENOMEM;
    }

    rt_mutex_take(at_event_lock, RT_WAITING_FOREVER);

    /* set current socket for send URC event */
    cur_socket = socket;
    /* set AT client end sign to deal with '>' sign.*/
    extern int rt_at_set_end_sign(char ch);
    rt_at_set_end_sign('>');

    while (sent_size < bfsz)
    {
        if (bfsz - sent_size < ESP8266_MODULE_SEND_MAX_SIZE)
        {
            cur_pkt_size = bfsz - sent_size;
        }
        else
        {
            cur_pkt_size = ESP8266_MODULE_SEND_MAX_SIZE;
        }

        /* send the "AT+CIPSEND" commands to AT server than receive the '>' response on the first line. */
        if (rt_at_exec_cmd(resp, "AT+CIPSEND=%d,%d", socket, cur_pkt_size) < 0)
        {
            result = -RT_ERROR;
            goto __exit;
        }

        /* send the real data to server or client */
        result = (int) rt_at_client_send(buff + sent_size, cur_pkt_size);
        if (result == 0)
        {
            result = -RT_ERROR;
            goto __exit;
        }

        /* waiting result event from AT URC */
        if (at_socket_event_recv(SET_EVENT(socket, 0), rt_tick_from_millisecond(1 * 1000), RT_EVENT_FLAG_OR) < 0)
        {
            LOG_E("socket (%d) send failed, wait connect result timeout.", socket);
            result = -RT_ETIMEOUT;
            goto __exit;
        }
        /* waiting OK or failed result */
        if ((event_result = at_socket_event_recv(ESP8266_EVENT_SEND_OK | ESP8266_EVENT_SEND_FAIL, rt_tick_from_millisecond(1 * 1000),
                RT_EVENT_FLAG_OR)) < 0)
        {
            LOG_E("socket (%d) send failed, wait connect OK|FAIL timeout.", socket);
            result = -RT_ETIMEOUT;
            goto __exit;
        }
        /* check result */
        if (event_result & ESP8266_EVENT_SEND_FAIL)
        {
            LOG_E("socket (%d) send failed, return failed.", socket);
            result = -RT_ERROR;
            goto __exit;
        }

        if (type == AT_SOCKET_TCP)
        {
            cur_pkt_size = cur_send_bfsz;
        }

        sent_size += cur_pkt_size;
    }

__exit:
    /* reset the end sign for data */
    rt_at_set_end_sign(0);

    rt_mutex_release(at_event_lock);

    if (resp)
    {
        rt_at_delete_resp(resp);
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
 *         -2: wait socket event timeout
 *         -5: no memory
 */
static int esp8266_domain_resolve(const char *name, char ip[16])
{
    int result = RT_EOK;
    char recv_ip[16] = { 0 };
    rt_at_response_t resp = RT_NULL;

    RT_ASSERT(name);
    RT_ASSERT(ip);

    resp = rt_at_create_resp(128, 0, rt_tick_from_millisecond(5000));
    if (!resp)
    {
        LOG_E("No memory for response structure!");
        return -RT_ENOMEM;
    }

    rt_mutex_take(at_event_lock, RT_WAITING_FOREVER);

__restart:
    if (rt_at_exec_cmd(resp, "AT+CIPDOMAIN=\"%s\"", name) < 0)
    {
        result = -RT_ERROR;
        goto __exit;
    }

    /* parse the third line of response data, get the IP address */
    at_resp_parse_line_args(resp, 1, "+CIPDOMAIN:%s", recv_ip);

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
        rt_at_delete_resp(resp);
    }

    return result;

}

/**
 * set AT socket event notice callback
 *
 * @param event notice event
 * @param cb notice callback
 */
static void esp8266_socket_set_event_cb(at_socket_evt_t event, at_evt_cb_t cb)
{
    if (event < sizeof(at_evt_cb_set) / sizeof(at_evt_cb_set[1]))
    {
        at_evt_cb_set[event] = cb;
    }
}

static void urc_send_func(const char *data, rt_size_t size)
{
    RT_ASSERT(data && size);

    if (strstr(data, "SEND OK"))
    {
        at_socket_event_send(SET_EVENT(cur_socket, ESP8266_EVENT_SEND_OK));
    }
    else if (strstr(data, "SEND FAIL"))
    {
        at_socket_event_send(SET_EVENT(cur_socket, ESP8266_EVENT_SEND_FAIL));
    }
}

static void urc_send_bfsz_func(const char *data, rt_size_t size)
{
    int send_bfsz = 0;

    RT_ASSERT(data && size);

    sscanf(data, "Recv %d bytes", &send_bfsz);

    cur_send_bfsz = send_bfsz;
}

static void urc_close_func(const char *data, rt_size_t size)
{
    int socket = 0;

    RT_ASSERT(data && size);

    sscanf(data, "%d,CLOSED", &socket);
    /* notice the socket is disconnect by remote */
    if (at_evt_cb_set[AT_SOCKET_EVT_CLOSED])
    {
        at_evt_cb_set[AT_SOCKET_EVT_CLOSED](socket, AT_SOCKET_EVT_CLOSED, RT_NULL, 0);
    }
}

static void urc_recv_func(const char *data, rt_size_t size)
{
    int socket = 0;
    rt_size_t bfsz = 0, temp_size = 0;
    char *recv_buf = RT_NULL, temp[8];

    RT_ASSERT(data && size);

    /* get the current socket and receive buffer size by receive data */
    sscanf(data, "+IPD,%d,%d:", &socket, (int *) &bfsz);

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
                rt_at_client_recv(temp, sizeof(temp));
            }
            else
            {
                rt_at_client_recv(temp, bfsz - temp_size);
            }
            temp_size += sizeof(temp);
        }
        return;
    }

    /* sync receive data */
    if (rt_at_client_recv(recv_buf, bfsz) != bfsz)
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

static void urc_busy_p_func(const char *data, rt_size_t size)
{
    RT_ASSERT(data && size);

    LOG_D("system is processing a commands and it cannot respond to the current commands.");
}

static void urc_busy_s_func(const char *data, rt_size_t size)
{
    RT_ASSERT(data && size);

    LOG_D("system is sending data and it cannot respond to the current commands.");
}

static void urc_func(const char *data, rt_size_t size)
{
    RT_ASSERT(data && size);

    if(strstr(data, "WIFI CONNECTED"))
    {
        LOG_I("ESP8266 WIFI is connected.");
    }
    else if(strstr(data, "WIFI DISCONNECT"))
    {
        LOG_I("ESP8266 WIFI is disconnect.");
    }
}

static struct rt_at_urc urc_table[] = {
        {"SEND OK",          "\r\n",           urc_send_func},
        {"SEND FAIL",        "\r\n",           urc_send_func},
        {"Recv",             "bytes\r\n",      urc_send_bfsz_func},
        {"",                 ",CLOSED\r\n",    urc_close_func},
        {"+IPD",             ":",              urc_recv_func},
        {"busy p",           "\r\n",           urc_busy_p_func},
        {"busy s",           "\r\n",           urc_busy_s_func},
        {"WIFI CONNECTED",   "\r\n",           urc_func},
        {"WIFI DISCONNECT",  "\r\n",           urc_func},
};

/* AT client port initialization */
int rt_at_client_port_init(void)
{
    /* create current AT socket event */
    at_socket_event = rt_event_create("at_sock_event", RT_IPC_FLAG_FIFO);
    if (!at_socket_event)
    {
        LOG_E("RT AT client port initialize failed! at_sock_event create failed!");
        return -RT_ENOMEM;
    }

    /* create current AT socket lock */
    at_event_lock = rt_mutex_create("at_event_lock", RT_IPC_FLAG_FIFO);
    if (!at_event_lock)
    {
        LOG_E("RT AT client port initialize failed! at_sock_lock create failed!");
        rt_event_delete(at_socket_event);
        return -RT_ENOMEM;
    }

    /* register URC data execution function  */
    rt_at_set_urc_table(urc_table, sizeof(urc_table) / sizeof(urc_table[0]));

    return RT_EOK;
}

#define AT_SEND_CMD(resp, cmd)                                                                          \
    do                                                                                                  \
    {                                                                                                   \
        if (rt_at_exec_cmd(rt_at_resp_set_info(resp, 256, 0, rt_tick_from_millisecond(5000)), cmd) < 0) \
        {                                                                                               \
            LOG_E("RT AT send commands(%s) error!", cmd);                                               \
            return -1;                                                                                  \
        }                                                                                               \
    } while(0);                                                                                         \

static int esp8266_net_init(void)
{
    rt_at_response_t resp = RT_NULL;
    rt_size_t i;

    resp = rt_at_create_resp(128, 0, rt_tick_from_millisecond(5000));
    if (!resp)
    {
        LOG_E("No memory for response structure!");
        return -RT_ENOMEM;
    }
    /* reset module */
    AT_SEND_CMD(resp, "AT+RST");
    /* reset waiting delay */
    rt_thread_delay(rt_tick_from_millisecond(1000));
    /* disable echo */
    AT_SEND_CMD(resp, "ATE0");
    /* set current mode to Wi-Fi station */
    AT_SEND_CMD(resp, "AT+CWMODE=1");
    /* get module version */
    AT_SEND_CMD(resp, "AT+GMR");
    /* show module version */
    for (i = 0; i < resp->line_counts - 1; i++)
    {
        LOG_D("%s", at_resp_get_line(resp, i + 1))
    }
    /* connect to WiFi AP */
    if (rt_at_exec_cmd(rt_at_resp_set_info(resp, 128, 0, 20 * RT_TICK_PER_SECOND), "AT+CWJAP=\"%s\",\"%s\"",
            AT_DEVICE_WIFI_SSID, AT_DEVICE_WIFI_PASSWORD) != RT_EOK)
    {
        LOG_E("AT network initialize failed, check ssid(%s) and password(%s).", AT_DEVICE_WIFI_SSID, AT_DEVICE_WIFI_PASSWORD);
        return -RT_ERROR;
    }

    AT_SEND_CMD(resp, "AT+CIPMUX=1");

    if (resp)
    {
        rt_at_delete_resp(resp);
    }

    LOG_I("AT network initialize success!");

    return RT_EOK;
}

int esp8266_ping(int argc, char **argv)
{
    rt_at_response_t resp = RT_NULL;
    static int icmp_seq;
    int req_time;

    if (argc != 2)
    {
        rt_kprintf("Please input: at_ping <host address>\n");
        return -RT_ERROR;
    }

    resp = rt_at_create_resp(64, 0, rt_tick_from_millisecond(5000));
    if (!resp)
    {
        rt_kprintf("No memory for response structure!\n");
        return -RT_ENOMEM;
    }

    for(icmp_seq = 1; icmp_seq <= 4; icmp_seq++)
    {
        if (rt_at_exec_cmd(resp, "AT+PING=\"%s\"", argv[1]) < 0)
        {
            rt_kprintf("ping: unknown remote server host\n");
            rt_at_delete_resp(resp);
            return -RT_ERROR;
        }

        at_resp_parse_line_args(resp, 1, "+%d", &req_time);
        if (req_time)
        {
            rt_kprintf("32 bytes from %s icmp_seq=%d time=%d ms\n", argv[1], icmp_seq, req_time);
        }
    }

    if (resp)
    {
        rt_at_delete_resp(resp);
    }

    return RT_EOK;
}

#ifdef FINSH_USING_MSH
#include <finsh.h>
MSH_CMD_EXPORT_ALIAS(esp8266_net_init, at_net_init, initialize AT network);
MSH_CMD_EXPORT_ALIAS(esp8266_ping, at_ping, AT ping network host);
#endif

static const struct at_device_ops esp8266_socket_ops = {
    .connect =              esp8266_socket_connect,
    .close =                esp8266_socket_close,
    .send =                 esp8266_socket_send,
    .domain_resolve =       esp8266_domain_resolve,
    .set_event_cb =         esp8266_socket_set_event_cb,
};

static int at_socket_device_init(void)
{
    esp8266_net_init();

    at_scoket_device_register(&esp8266_socket_ops);

    return 0;
}
INIT_APP_EXPORT(at_socket_device_init);

#endif /* AT_DEVICE_NOT_SELECTED */

/*
 * File      : at_socket_usrk7.c
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
 * 2020-07-26     Wei Fu      first version
 */

#include <stdio.h>
#include <string.h>

#include "at_device_usrk7.h"

#define LOG_TAG                       "at.skt.usrk7"
#include <at_log.h>

#if defined(AT_DEVICE_USING_USRK7) && defined(AT_USING_SOCKET)

#define USRK7_MODULE_SEND_MAX_SIZE   512

static rt_thread_t thread;
static at_pass_through_t ptb = RT_NULL;
struct at_device_usrk7 *usrk7_dev = RT_NULL;
static rt_int32_t usrk7_socket_fd[AT_DEVICE_USRK7_SOCKETS_NUM] = {-1};

static at_evt_cb_t at_evt_cb_set[] = {
        [AT_SOCKET_EVT_RECV] = NULL,
        [AT_SOCKET_EVT_CLOSED] = NULL,
};

 static void usrk7_socket_recv_thread_entry(void *parameter)
{

    struct at_socket *socket;
    at_pass_through_t ptb = parameter;
    struct at_device_usrk7 *usrk7 = usrk7_dev;
    struct at_device *device = &(usrk7->device);
    struct at_client *client = device->client;
    // local buffer for testing
    char recv_buff[AT_CLIENT_PASS_THROUGH_BUF_SIZE] = {0};

    while (1)
    {
        //clean the buffer before the transmission
        ptb->buf_len = 0;
        rt_memset(ptb->buf, 0x00, ptb->buf_size);
        //use the buffer for the transmission
        client->ptb = ptb;

        if (rt_sem_take(client->ptb_notice, ptb->timeout) == RT_EOK)
        {
            if (ptb->buf_len > 0) {
                //copy the data from client pass through buffer
                rt_memcpy(recv_buff, ptb->buf, ptb->buf_len);

                //process the received data
                /* get at socket object by device socket descriptor */
                socket = &(device->sockets[usrk7_dev->device_socket]);
                //rt_kprintf("%s @ %d socket[%d]\n",
                //           recv_buff, *socket, usrk7_dev->device_socket);

                /* notice the receive buffer and buffer size */
                if (at_evt_cb_set[AT_SOCKET_EVT_RECV])
                {
                    //rt_kprintf("@%p %d\n",
                    //at_evt_cb_set[AT_SOCKET_EVT_RECV], ptb->buf_len);
                    at_evt_cb_set[AT_SOCKET_EVT_RECV](socket,
                                                      AT_SOCKET_EVT_RECV,
                                                      recv_buff, ptb->buf_len);
                }
            }
        }
    }

    return;
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
static int usrk7_socket_close(struct at_socket *socket)
{
    int device_socket = (int) socket->user_data;
    struct at_device *device = (struct at_device *) socket->device;
    int port = 0;

    rt_thread_delete(thread);

    usrk7_cmdmode(device);
    rt_thread_mdelay(500);

//    usrk7_at_sock_set(device, "SOCKA1", "TCPC", "0.0.0.0", &port);
//    rt_thread_mdelay(USRK7_RESET_MDELAY);
//    usrk7_at_sock_set(device, "SOCKB1", "NONE", "0.0.0.0", &port);
//    rt_thread_mdelay(USRK7_RESET_MDELAY);

    usrk7_socket_fd[device_socket] = -1;

    if (ptb)
        at_delete_ptb(ptb);

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
static int usrk7_socket_connect(struct at_socket *socket,
                                char *ip, int32_t port,
                                enum at_socket_type type,
                                rt_bool_t is_client)
{
    int result = RT_EOK;
    at_response_t resp = RT_NULL;
    int device_socket = (int) socket->user_data;
    struct at_device *device = (struct at_device *) socket->device;
    struct at_client *client = device->client;
    int socket_fd = -1;

    RT_ASSERT(ip);
    RT_ASSERT(port >= 0);

    usrk7_dev = (struct at_device_usrk7 *) device->user_data;
    usrk7_dev->device_socket = device_socket;

    switch (type)
    {
        case AT_SOCKET_TCP:
            /* send AT commands */
            if (is_client)
            {
                usrk7_at_sock_set(device, "SOCKA1", "TCPC", ip, &port);
            }
            else
            {
                usrk7_at_sock_set(device, "SOCKA1", "TCPS", ip, &port);
            }

            break;

        case AT_SOCKET_UDP:
            if (is_client)
            {
                usrk7_at_sock_set(device, "SOCKA1", "UDPS", ip, &port);
            }
            else
            {
                usrk7_at_sock_set(device, "SOCKA1", "UDPS", ip, &port);
            }
            break;

        default:
            LOG_E("not supported connect type %d.", type);
            result = -RT_ERROR;
            goto __exit;
    }
    /* you *MUST* wait for 2s, because of the "reset" in usrk7_at_sock_set */
    rt_thread_mdelay(USRK7_RESET_MDELAY);

    socket_fd = 1;
    usrk7_socket_fd[device_socket] = socket_fd;

    // create a pass through buffer object
    ptb = at_create_ptb(AT_CLIENT_PASS_THROUGH_BUF_SIZE,
                        AT_CLIENT_PASS_THROUGH_TIMEOUT);
    if (!ptb)
    {
        LOG_E("Create pass through object failed.");
        return -RT_ERROR;
    }

    rt_mutex_take(client->lock, RT_WAITING_FOREVER);

    usrk7_entmode(device);
    rt_thread_mdelay(USRK7_DEFAULT_MDELAY);
    /* create receive thread */
    thread = rt_thread_create("usrk7_recv", usrk7_socket_recv_thread_entry,
                              ptb, USRK7_THREAD_STACK_SIZE, 25, 10);
    if (thread)
    {
        rt_thread_startup(thread);
    }
    else
    {
        LOG_E("rt_thread_startup fail. Can't receive data.");
        result = -RT_ERROR;
    }

__exit:
    rt_mutex_release(client->lock);
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
 * @return -1: unsupport
 */
static int usrk7_socket_send(struct at_socket *socket,
                             const char *buff, size_t bfsz,
                             enum at_socket_type type)
{
    int result = RT_EOK;
    size_t cur_pkt_size = 0, sent_size = 0;
    int device_socket = (int) socket->user_data;
    struct at_device *device = (struct at_device *) socket->device;
    struct at_device_usrk7 *usrk7 = (struct at_device_usrk7 *) device->user_data;
    rt_mutex_t lock = device->client->lock;

    RT_ASSERT(buff);
    RT_ASSERT(bfsz > 0);

    rt_mutex_take(lock, RT_WAITING_FOREVER);

    while (sent_size < bfsz)
    {
        if (bfsz - sent_size < USRK7_MODULE_SEND_MAX_SIZE)
        {
            cur_pkt_size = bfsz - sent_size;
        }
        else
        {
            cur_pkt_size = USRK7_MODULE_SEND_MAX_SIZE;
        }
        if (!rt_device_write(device->client->device,
                             sent_size, buff, cur_pkt_size))
        {
            LOG_E("%s device send date fail.", device->name);
            result = -RT_ERROR;
            break;
        }
        sent_size += cur_pkt_size;
    }

    rt_mutex_release(lock);
    return result > 0 ? sent_size : result;
}


/**
 * domain resolve by AT commands. (unsupported by USR-K7)
 *
 * @param name domain name
 * @param ip parsed IP address, it's length must be 16
 *
 * @return -1: unsupport
 */
static int usrk7_domain_resolve(const char *name, char ip[16])
{
    return -RT_ERROR;
}
/**
 * set AT socket event notice callback
 *
 * @param event notice event
 * @param cb notice callback
 */
static void usrk7_socket_set_event_cb(at_socket_evt_t event, at_evt_cb_t cb)
{
    if (event < sizeof(at_evt_cb_set) / sizeof(at_evt_cb_set[1]))
    {
        at_evt_cb_set[event] = cb;
    }
}

static const struct at_socket_ops usrk7_socket_ops =
{
    usrk7_socket_connect,
    usrk7_socket_close,
    usrk7_socket_send,
    usrk7_domain_resolve,
    usrk7_socket_set_event_cb,
};

int usrk7_socket_init(struct at_device *device)
{
    RT_ASSERT(device);
    /* Nothing need to init */
    return RT_EOK;
}

int usrk7_socket_class_register(struct at_device_class *class)
{
    RT_ASSERT(class);
    class->socket_ops = &usrk7_socket_ops;
    class->socket_num = AT_DEVICE_USRK7_SOCKETS_NUM;
    return RT_EOK;
}

#endif /* AT_DEVICE_USING_USRK7 && AT_USING_SOCKET */

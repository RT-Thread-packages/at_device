/*
 * File      : at_sample_usrk7.c
 * This file is part of RT-Thread RTOS
 * COPYRIGHT (C) 2006 - 2020, RT-Thread Development Team
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
#include <rtthread.h>

#include "at_device_usrk7.h"

#define LOG_TAG                        "at.sample.usrk7"
#include <at_log.h>

#define USRK7_SAMPLE_DEIVCE_NAME     "usrk7"

//default exit code: 0x1B, ^[, ESC
#define USRK7_SAMPLE_EXIT_CODE     0x1B

static struct at_device_usrk7 usrk7_dev =
{
    USRK7_SAMPLE_DEIVCE_NAME,
    USRK7_SAMPLE_CLIENT_NAME,

    USRK7_SAMPLE_RECV_BUFF_LEN,
};

 static void usrk7_recv_thread_entry(void *parameter)
{
    struct at_device_usrk7 *usrk7 = &usrk7_dev;
    struct at_device *device = &(usrk7->device);
    struct at_client *client = device->client;
    at_pass_through_t ptb = parameter;

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

                //process the received data, rt_kprintf just a example
                recv_buff[ptb->buf_len] = '\0';
                // just print them out as string
                rt_kprintf ("%s", recv_buff);
            }
        }
    }
    return;
}
static int usrk7_socket_setup(int argc, char *argv[])
{
    rt_err_t ret = RT_EOK;
    struct at_device_usrk7 *usrk7 = &usrk7_dev;
    struct at_device *device = &(usrk7->device);
    int port = atoi(argv[4]);

    if (argc < 5)
    {
        LOG_E("usrk7_connect <socket_name> <socket_type> <ip> <port>. ");
        LOG_I("eg: usrk7_connect SOCKA1 TCPS 192.168.23.133 12345 ");
        LOG_I("socket_name: SOCKA1, SOCKB1 TCPS 192.168.23.133 12345 ");
        LOG_I("socket_type: TCPS/UDPS(SOCKA1 only), TCPC/UDPC ");
        return -RT_ERROR;
    }

    usrk7_at_sock_set(device, argv[1], argv[2], argv[3], &port);
    /* you *MUST* wait for 2s, because of the "reset" in usrk7_at_sock_set */
    rt_thread_mdelay(2000);

    return ret;
}
MSH_CMD_EXPORT(usrk7_socket_setup, usrk7 socket setup sample );

static int usrk7_io(int argc, char *argv[])
{
    char ch;
    rt_err_t ret = RT_EOK;
    rt_thread_t thread;
    struct at_device_usrk7 *usrk7 = &usrk7_dev;
    struct at_device *device = &(usrk7->device);
    struct at_client *client = device->client;
    at_pass_through_t ptb = RT_NULL;

    // create a pass through buffer object
    ptb = at_create_ptb(AT_CLIENT_PASS_THROUGH_BUF_SIZE,
                        AT_CLIENT_PASS_THROUGH_TIMEOUT);
    if (!ptb)
    {
        LOG_E("Create pass through object failed.");
        return -RT_ERROR;
    }
    LOG_I("usrk7_io is usrk7 pass-through mode test sample, press ^[ to exit.");

    rt_mutex_take(client->lock, RT_WAITING_FOREVER);

    usrk7_entmode(device);
    //rt_thread_mdelay(USRK7_DEFAULT_MDELAY);

    /* create receive thread */
    thread = rt_thread_create("usrk7_recv", usrk7_recv_thread_entry,
                                          ptb, 1024, 25, 10);
    if (thread)
    {
        rt_thread_startup(thread);
    }
    else
    {
        LOG_E("rt_thread_startup fail. Can't receive data.");
        ret = -RT_ERROR;
    }

    while (ch != USRK7_SAMPLE_EXIT_CODE)
    {
        //get data from standard input
        ch = getchar();
        if (!rt_device_write(client->device, 0, &ch, 1))
        {
            LOG_E("%s device send date fail.", device->name);
            ret = -RT_ERROR;
            break;
        }
    }

    rt_thread_delete(thread);

    // You MUST wait 2s to exit the transmission mode
    rt_thread_mdelay(USRK7_RESET_MDELAY);
    // back to the command mode
    usrk7_cmdmode(device);

    if (ptb)
        at_delete_ptb(ptb);

    return ret;
}
MSH_CMD_EXPORT(usrk7_io, usrk7 socket receive/send date sample );

static int usrk7_device_register(void)
{
    struct at_device_usrk7 *usrk7 = &usrk7_dev;

    return at_device_register(&(usrk7->device),
                              usrk7->device_name,
                              usrk7->client_name,
                              AT_DEVICE_CLASS_USRK7,
                              (void *) usrk7);
}
INIT_APP_EXPORT(usrk7_device_register);

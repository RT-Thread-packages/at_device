 /*
 * File      : at_device_bc28.c
 * This file is part of RT-Thread RTOS
 * Copyright (c) 2020, RudyLo <luhuadong@163.com>
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
 * Date           Author            Notes
 * 2020-02-12     luhuadong         first version
 */

#include <stdio.h>
#include <string.h>

#include <at_device_bc28.h>

#define LOG_TAG                        "at.dev.bc28"
#include <at_log.h>

#ifdef AT_DEVICE_USING_BC28

#define BC28_WAIT_CONNECT_TIME          5000
#define BC28_THREAD_STACK_SIZE          2048
#define BC28_THREAD_PRIORITY            (RT_THREAD_PRIORITY_MAX/2)

static void bc28_power_on(struct at_device *device)
{
    struct at_device_bc28 *bc28 = RT_NULL;
    
    bc28 = (struct at_device_bc28 *)device->user_data;
    bc28->power_status = RT_TRUE;

    /* not need to set pin configuration for bc28 device power on */
    if (bc28->power_pin == -1)
    {
        return(RT_EOK);
    }
    
    rt_pin_write(bc28->power_pin, PIN_HIGH);
    rt_thread_mdelay(500);
    rt_pin_write(bc28->power_pin, PIN_LOW);

    return(RT_EOK);
}

static void bc28_power_off(struct at_device *device)
{
    at_response_t resp = RT_NULL;
    struct at_device_bc28 *bc28 = RT_NULL;
    
    resp = at_create_resp(64, 0, rt_tick_from_millisecond(300));
    if (resp == RT_NULL)
    {
        LOG_D("no memory for resp create.");
        return(-RT_ERROR);
    }
    
    if (at_obj_exec_cmd(device->client, resp, "AT+QPOWD=0") != RT_EOK)
    {
        LOG_D("power off fail.");
        at_delete_resp(resp);
        return(-RT_ERROR);
    }
    
    at_delete_resp(resp);
    
    bc28 = (struct at_device_bc28 *)device->user_data;
    bc28->power_status = RT_FALSE;
    
    return(RT_EOK);
}

at_device_bc28 functions


static int bc28_power_on();
static int bc28_power_off();
static int bc28_sleep();
static int bc28_wakeup();

static int bc28_check_link_status();
static int bc28_netdev_set_info();

static void bc28_check_link_status_entry();
static int bc28_netdev_check_link_status();

static int bc28_netdev_set_up();
static int bc28_netdev_set_down();
static int bc28_netdev_set_dns_server();
static int bc28_netdev_ping();

const struct netdev_ops bc28_netdev_ops =
{
    bc28_netdev_set_up,
    bc28_netdev_set_down,
    RT_NULL,
    bc28_netdev_set_dns_server,
    RT_NULL,
#ifdef NETDEV_USING_PING
    bc28_netdev_ping,
#endif
    RT_NULL,
};


static struct netdev *bc28_netdev_add();

static void bc28_init_thread_entry();

static int bc28_net_init();


static int bc28_init(struct at_device *device)
{
    struct at_device_bc28 *bc28 = (struct at_device_bc28 *)device->user_data;

    /* initialize AT client */
    at_client_init(bc28->client_name, bc28->recv_bufsz);

    device->client = at_client_get(bc28->client_name);
    if (device->client == RT_NULL)
    {
        LOG_E("get AT client(%s) failed.", bc28->client_name);
        return -RT_ERROR;
    }

    /* register URC data execution function  */
#ifdef AT_USING_SOCKET
    bc28_socket_init(device);
#endif
}

static int bc28_deinit(struct at_device *device)
{
    RT_ASSERT(device);
    
    return bc28_netdev_set_down(device->netdev);
}

static int bc28_control(struct at_device *device, int cmd, void *arg)
{
    int result = -RT_ERROR;

    RT_ASSERT(device);

    switch (cmd)
    {
    case AT_DEVICE_CTRL_SLEEP:
        result = bc28_sleep(device);
        break;
    case AT_DEVICE_CTRL_WAKEUP:
        result = bc28_wakeup(device);
        break;
    case AT_DEVICE_CTRL_POWER_ON:
    case AT_DEVICE_CTRL_POWER_OFF:
    case AT_DEVICE_CTRL_RESET:
    case AT_DEVICE_CTRL_LOW_POWER:
    case AT_DEVICE_CTRL_NET_CONN:
    case AT_DEVICE_CTRL_NET_DISCONN:
    case AT_DEVICE_CTRL_SET_WIFI_INFO:
    case AT_DEVICE_CTRL_GET_SIGNAL:
    case AT_DEVICE_CTRL_GET_GPS:
    case AT_DEVICE_CTRL_GET_VER:
        LOG_W("not support the control command(%d).", cmd);
        break;
    default:
        LOG_E("input error control command(%d).", cmd);
        break;
    }

    return result;
}

const struct at_device_ops bc28_device_ops =
{
    bc28_init,
    bc28_deinit,
    bc28_control,
};

static int bc28_device_class_register(void)
{
    struct at_device_class *class = RT_NULL;

    class = (struct at_device_class *) rt_calloc(1, sizeof(struct at_device_class));
    if (class == RT_NULL)
    {
        LOG_E("no memory for device class create.");
        return -RT_ENOMEM;
    }

    /* fill bc26 device class object */
#ifdef AT_USING_SOCKET
    bc28_socket_class_register(class);
#endif
    class->device_ops = &bc28_device_ops;

    return at_device_class_register(class, AT_DEVICE_CLASS_BC28);
}
INIT_DEVICE_EXPORT(bc28_device_class_register);

#endif /* AT_DEVICE_USING_BC28 */
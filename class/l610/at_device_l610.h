/*
 * Copyright (c) 2006-2023, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2020-10-28     zhangyang     first version
 */

#ifndef __AT_DEVICE_L610_H__
#define __AT_DEVICE_L610_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <at_device.h>

/* The maximum number of sockets supported by the l610 device */
#define AT_DEVICE_L610_SOCKETS_NUM    6

struct at_device_l610
{
    char *device_name;
    char *client_name;

    int power_pin;
    int power_status_pin;
    size_t recv_line_num;
    struct at_device device;

    void *socket_data;
    void *user_data;

    rt_bool_t power_status;
    rt_bool_t sleep_status;
};

#ifdef AT_USING_SOCKET

/* l610 device socket initialize */
int l610_socket_init(struct at_device *device);

/* l610 device class socket register */
int l610_socket_class_register(struct at_device_class *class);

#endif /* AT_USING_SOCKET */

#ifdef __cplusplus
}
#endif

#endif /* __AT_DEVICE_L610_H__ */

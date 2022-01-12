/*
 * Copyright (c) 2006-2022, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2019-11-23     luliang    first version
 */

#ifndef __A9G_DEVICE_H__
#define __A9G_DEVICE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>

#include <at_device.h>

/* The maximum number of sockets supported by the a9g device */
#define AT_DEVICE_A9G_SOCKETS_NUM       8

struct at_device_a9g
{
    char *device_name;
    char *client_name;

    int power_pin;
    int power_status_pin;
    size_t recv_line_num;
    struct at_device device;

    void *user_data;
};

#ifdef AT_USING_SOCKET

/* a9g device socket initialize */
int a9g_socket_init(struct at_device *device);

/* a9g device class socket register */
int a9g_socket_class_register(struct at_device_class *class);

#endif /* AT_USING_SOCKET */

#ifdef __cplusplus
}
#endif


#endif  /*  __AT_DEVICE_A9G_H__*/

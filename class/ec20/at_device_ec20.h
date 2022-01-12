/*
 * Copyright (c) 2006-2022, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2019-05-16     chenyong     first version
 */

#ifndef __AT_DEVICE_EC20_H__
#define __AT_DEVICE_EC20_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>

#include <at_device.h>

/* The maximum number of sockets supported by the ec20 device */
#define AT_DEVICE_EC20_SOCKETS_NUM  5

struct at_device_ec20
{
    char *device_name;
    char *client_name;

    int power_pin;
    int power_status_pin;
    size_t recv_line_num;
    struct at_device device;

    void *socket_data;
    void *user_data;
};

#ifdef AT_USING_SOCKET

/* ec20 device socket initialize */
int ec20_socket_init(struct at_device *device);

/* ec20 device class socket register */
int ec20_socket_class_register(struct at_device_class *class);

#endif /* AT_USING_SOCKET */

#ifdef __cplusplus
}
#endif

#endif /* __AT_DEVICE_EC20_H__ */

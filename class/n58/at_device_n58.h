/*
 * Copyright (c) 2006-2023, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author            Notes
 * 2020-05-22     shuobatian        first version
 */

#ifndef __AT_DEVICE_N58_H__
#define __AT_DEVICE_N58_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>

#include <at_device.h>

/* The maximum number of sockets supported by the N58 device */
#define AT_DEVICE_N58_SOCKETS_NUM      4

struct at_device_n58
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

/* n58 device socket initialize */
int n58_socket_init(struct at_device *device);

/* n58 device class socket register */
int n58_socket_class_register(struct at_device_class *class);

#endif /* AT_USING_SOCKET */

#ifdef __cplusplus
}
#endif

#endif /* __AT_DEVICE_N58_H__ */

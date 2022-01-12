/*
 * Copyright (c) 2006-2022, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2019-12-30     qiyongzhong     first version
 */

#ifndef __AT_DEVICE_ME3616_H__
#define __AT_DEVICE_ME3616_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>

#include <at_device.h>

/* The maximum number of sockets supported by the me3616 device */
#define AT_DEVICE_ME3616_SOCKETS_NUM  5

struct at_device_me3616
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

/* me3616 device socket initialize */
int me3616_socket_init(struct at_device *device);

/* me3616 device class socket register */
int me3616_socket_class_register(struct at_device_class *class);

#endif /* AT_USING_SOCKET */

#ifdef __cplusplus
}
#endif

#endif /* __AT_DEVICE_ME3616_H__ */


/*
 * Copyright (c) 2006-2022, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author            Notes
 * 2020-10-27     qiyongzhong       first version
 */

#ifndef __AT_DEVICE_EC200X_H__
#define __AT_DEVICE_EC200X_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>

#include <at_device.h>

/* The maximum number of sockets supported by the ec200x device */
#define AT_DEVICE_N720_SOCKETS_NUM  6

struct at_device_n720
{
    char *device_name;
    char *client_name;

    int power_pin;
    int power_status_pin;
    int wakeup_pin;
    size_t recv_line_num;
    void (*power_ctrl)(int is_on);
    struct at_device device;

    void *socket_data;
    void *user_data;

    rt_bool_t power_status;
    rt_bool_t sleep_status;
};

#ifdef AT_USING_SOCKET

/* n720 device socket initialize */
int n720_socket_init(struct at_device *device);

/* n720 device class socket register */
int n720_socket_class_register(struct at_device_class *class);

#endif /* AT_USING_SOCKET */

#ifdef __cplusplus
}
#endif

#endif /* __AT_DEVICE_EC200X_H__ */


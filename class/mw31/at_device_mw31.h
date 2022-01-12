/*
 * Copyright (c) 2006-2022, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2019-06-23     flybreak     first version
 */

#ifndef __AT_DEVICE_MW31_H__
#define __AT_DEVICE_MW31_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>

#include <at_device.h>

/* The maximum number of sockets supported by the mw31 device */
#define AT_DEVICE_MW31_SOCKETS_NUM  5

struct at_device_mw31
{
    char *device_name;
    char *client_name;

    char *wifi_ssid;
    char *wifi_password;
    size_t recv_line_num;
    struct at_device device;

    void *user_data;
};

#ifdef AT_USING_SOCKET

/* mw31 device socket initialize */
int mw31_socket_init(struct at_device *device);

/* mw31 device class socket register */
int mw31_socket_class_register(struct at_device_class *class);

#endif /* AT_USING_SOCKET */

#ifdef __cplusplus
}
#endif

#endif /* __AT_DEVICE_MW31_H__ */

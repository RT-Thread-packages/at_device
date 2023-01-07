/*
 * Copyright (c) 2006-2023, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-12-16     Jonas        first version
 */

#ifndef __ML305_DEVICE_H__
#define __ML305_DEVICE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>

#include <at_device.h>

#define ML305_AT_RESP_TIMEOUT       RT_TICK_PER_SECOND

/* The maximum number of sockets supported by the ml305 device */
#define AT_DEVICE_ML305_SOCKETS_NUM         6

struct at_device_ml305
{
    char *device_name;
    char *client_name;

    int power_pin;
    int power_status_pin;
    size_t recv_buff_size;
    struct at_device device;

    void *user_data;
};

#ifdef AT_USING_SOCKET

/* ml305 device socket initialize */
int ml305_socket_init(struct at_device *device);

/* ml305 device class socket register */
int ml305_socket_class_register(struct at_device_class *class);

#endif /* AT_USING_SOCKET */

#ifdef __cplusplus
}
#endif

#endif  /*  __AT_DEVICE_ML305_H__*/

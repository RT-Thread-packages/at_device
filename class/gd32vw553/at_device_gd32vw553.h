/*
 * Copyright (c) 2006-2026, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-09-22     CYFS         add GD32VW553 device configuration
 */

#ifndef __AT_DEVICE_GD32VW553_H__
#define __AT_DEVICE_GD32VW553_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>

#include <at_device.h>

#define AT_DEVICE_GD32VW553_SOCKETS_NUM  1

struct at_device_gd32vw553
{
    char *device_name;
    char *client_name;

    char *wifi_ssid;
    char *wifi_password;
    rt_uint32_t baudrate;
    size_t recv_line_num;
    struct at_device device;

    void *user_data;
    rt_bool_t use_reset_pin;
    rt_base_t reset_pin;
    rt_uint8_t reset_active_level;
};

#ifdef AT_USING_SOCKET
int gd32vw553_socket_init(struct at_device *device);
int gd32vw553_socket_class_register(struct at_device_class *class);
#endif

#ifdef __cplusplus
}
#endif

#endif /* __AT_DEVICE_GD32VW553_H__ */

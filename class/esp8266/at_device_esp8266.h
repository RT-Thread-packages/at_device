/*
 * Copyright (c) 2006-2023, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2019-05-16     chenyong     first version
 */

#ifndef __AT_DEVICE_ESP8266_H__
#define __AT_DEVICE_ESP8266_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>

#include <at_device.h>
#define ESP8266_DEFAULT_AT_VERSION         "1.4.0.0"
#define ESP8266_DEFAULT_AT_VERSION_NUM     0x1040000

/* The maximum number of sockets supported by the esp8266 device */
#define AT_DEVICE_ESP8266_SOCKETS_NUM  5

struct at_device_esp8266
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

/* esp8266 device socket initialize */
int esp8266_socket_init(struct at_device *device);

/* esp8266 device class socket register */
int esp8266_socket_class_register(struct at_device_class *class);

/* convert the esp8266 AT version string to hexadecimal */
unsigned int esp8266_at_version_to_hex(const char *str);

/* obtain the esp8266 AT version number */
unsigned int esp8266_get_at_version(void);
#endif /* AT_USING_SOCKET */

#ifdef __cplusplus
}
#endif

#endif /* __AT_DEVICE_ESP8266_H__ */

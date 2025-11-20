/*
 * Copyright (c) 2006-2023, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2019-05-16     chenyong     first version
 */

#ifndef __AT_DEVICE_ESP32_H__
#define __AT_DEVICE_ESP32_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>

#include <at_device.h>
#define ESP32_DEFAULT_AT_VERSION         "1.4.0.0"
#define ESP32_DEFAULT_AT_VERSION_NUM     0x1040000

/* The maximum number of sockets supported by the esp32 device */
#define AT_DEVICE_ESP32_SOCKETS_NUM  5

struct at_device_esp32
{
    char *device_name;
    char *client_name;

    char *wifi_ssid;
    char *wifi_password;
    size_t recv_line_num;
    struct at_device device;

    uint16_t urc_socket;
    void *user_data;
};
typedef struct at_AP_INFO
{
    uint8_t ecn;       /* 加密方式 */
    char ssid[32];     /* 无线网络名称，最多31字节 + 终止符 */
    int8_t rssi;       /* 信号强度 */
    uint8_t mac[6];    /* MAC地址 */
    uint8_t channel;   /* 频道 */
} at_ap_info_t;
#ifdef AT_USING_SOCKET

/* esp32 device socket initialize */
int esp32_socket_init(struct at_device *device);

/* esp32 device class socket register */
int esp32_socket_class_register(struct at_device_class *class);

/* convert the esp32 AT version string to hexadecimal */
unsigned int esp32_at_version_to_hex(const char *str);

/* obtain the esp32 AT version number */
unsigned int esp32_get_at_version(void);
#endif /* AT_USING_SOCKET */

/* scan the AP information */
int esp32_scan_ap(struct at_device *device, at_ap_info_t *ap_info, uint8_t num);
#ifdef __cplusplus
}
#endif

#endif /* __AT_DEVICE_ESP32_H__ */

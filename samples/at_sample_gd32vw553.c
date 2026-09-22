/*
 * Copyright (c) 2006-2026, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-09-22     CYFS         add GD32VW553 registration sample
 */

#include <at_device_gd32vw553.h>
#include <drivers/dev_pin.h>

#ifndef GD32VW553_SAMPLE_DEVICE_NAME
#define GD32VW553_SAMPLE_DEVICE_NAME       "wifi0"
#endif

#ifndef GD32VW553_SAMPLE_CLIENT_NAME
#define GD32VW553_SAMPLE_CLIENT_NAME       "uart4"
#endif

#ifndef GD32VW553_SAMPLE_WIFI_SSID
#define GD32VW553_SAMPLE_WIFI_SSID         "rtthread"
#endif

#ifndef GD32VW553_SAMPLE_WIFI_PASSWORD
#define GD32VW553_SAMPLE_WIFI_PASSWORD     "12345678"
#endif

#ifndef GD32VW553_SAMPLE_BAUDRATE
#define GD32VW553_SAMPLE_BAUDRATE          115200
#endif

#ifndef GD32VW553_SAMPLE_RECV_LINE_NUM
#define GD32VW553_SAMPLE_RECV_LINE_NUM     1024
#endif

#ifndef GD32VW553_SAMPLE_RESET_PIN
#define GD32VW553_SAMPLE_RESET_PIN          (-1)
#endif

#ifndef GD32VW553_SAMPLE_RESET_ACTIVE_LEVEL
#define GD32VW553_SAMPLE_RESET_ACTIVE_LEVEL PIN_LOW
#endif

static struct at_device_gd32vw553 gd32vw5530 =
{
    .device_name = GD32VW553_SAMPLE_DEVICE_NAME,
    .client_name = GD32VW553_SAMPLE_CLIENT_NAME,
    .wifi_ssid = GD32VW553_SAMPLE_WIFI_SSID,
    .wifi_password = GD32VW553_SAMPLE_WIFI_PASSWORD,
    .baudrate = GD32VW553_SAMPLE_BAUDRATE,
    .recv_line_num = GD32VW553_SAMPLE_RECV_LINE_NUM,
    .use_reset_pin = GD32VW553_SAMPLE_RESET_PIN >= 0 ? RT_TRUE : RT_FALSE,
    .reset_pin = GD32VW553_SAMPLE_RESET_PIN,
    .reset_active_level = GD32VW553_SAMPLE_RESET_ACTIVE_LEVEL,
};

static int gd32vw553_device_register(void)
{
    struct at_device_gd32vw553 *gd32vw553 = &gd32vw5530;

    return at_device_register(&(gd32vw553->device),
                              gd32vw553->device_name,
                              gd32vw553->client_name,
                              AT_DEVICE_CLASS_GD32VW553,
                              (void *)gd32vw553);
}
INIT_APP_EXPORT(gd32vw553_device_register);

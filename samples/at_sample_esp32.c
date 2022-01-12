/*
 * Copyright (c) 2006-2022, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2019-05-10     chenyong     first version
 */

#include <at_device_esp32.h>

#define LOG_TAG                        "at.sample.esp"
#include <at_log.h>

#define ESP32_SAMPLE_DEIVCE_NAME     "esp32"

static struct at_device_esp32 esp0 =
{
    ESP32_SAMPLE_DEIVCE_NAME,
    ESP32_SAMPLE_CLIENT_NAME,

    ESP32_SAMPLE_WIFI_SSID,
    ESP32_SAMPLE_WIFI_PASSWORD,
    ESP32_SAMPLE_RECV_BUFF_LEN,
};

static int esp32_device_register(void)
{
    struct at_device_esp32 *esp32 = &esp0;

    return at_device_register(&(esp32->device),
                              esp32->device_name,
                              esp32->client_name,
                              AT_DEVICE_CLASS_ESP32,
                              (void *) esp32);
}
INIT_APP_EXPORT(esp32_device_register);

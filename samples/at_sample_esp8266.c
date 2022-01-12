/*
 * Copyright (c) 2006-2022, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2019-05-10     chenyong     first version
 */

#include <at_device_esp8266.h>

#define LOG_TAG                        "at.sample.esp"
#include <at_log.h>

#define ESP8266_SAMPLE_DEIVCE_NAME     "esp0"

static struct at_device_esp8266 esp0 =
{
    ESP8266_SAMPLE_DEIVCE_NAME,
    ESP8266_SAMPLE_CLIENT_NAME,

    ESP8266_SAMPLE_WIFI_SSID,
    ESP8266_SAMPLE_WIFI_PASSWORD,
    ESP8266_SAMPLE_RECV_BUFF_LEN,
};

static int esp8266_device_register(void)
{
    struct at_device_esp8266 *esp8266 = &esp0;

    return at_device_register(&(esp8266->device),
                              esp8266->device_name,
                              esp8266->client_name,
                              AT_DEVICE_CLASS_ESP8266,
                              (void *) esp8266);
}
INIT_APP_EXPORT(esp8266_device_register);

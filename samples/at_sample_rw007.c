/*
 * Copyright (c) 2006-2022, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2019-05-13     chenyong     first version
 */

#include <at_device_rw007.h>

#define LOG_TAG                        "at.sample.rw007"
#include <at_log.h>

#define RW007_SAMPLE_DEIVCE_NAME       "r0"

static struct at_device_rw007 r0 =
{
    RW007_SAMPLE_DEIVCE_NAME,
    RW007_SAMPLE_CLIENT_NAME,

    RW007_SAMPLE_WIFI_SSID,
    RW007_SAMPLE_WIFI_PASSWORD,
    RW007_SAMPLE_RECV_BUFF_LEN,
};

static int rw007_device_register(void)
{
    struct at_device_rw007 *rw007 = &r0;

    return at_device_register(&(rw007->device),
                              rw007->device_name,
                              rw007->client_name,
                              AT_DEVICE_CLASS_RW007,
                              (void *) rw007);
}
INIT_APP_EXPORT(rw007_device_register);

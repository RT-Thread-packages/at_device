/*
 * Copyright (c) 2006-2022, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2019-05-13     chenyong     first version
 */

#include <at_device_m26.h>

#define LOG_TAG                        "at.sample.m26"
#include <at_log.h>

#define M26_SAMPLE_DEIVCE_NAME        "m0"

static struct at_device_m26 m0 =
{
    M26_SAMPLE_DEIVCE_NAME,
    M26_SAMPLE_CLIENT_NAME,

    M26_SAMPLE_POWER_PIN,
    M26_SAMPLE_STATUS_PIN,
    M26_SAMPLE_RECV_BUFF_LEN,
};

static int m26_device_register(void)
{
    struct at_device_m26 *m26 = &m0;

    return at_device_register(&(m26->device),
                              m26->device_name,
                              m26->client_name,
                              AT_DEVICE_CLASS_M26_MC20,
                              (void *) m26);
}
INIT_APP_EXPORT(m26_device_register);

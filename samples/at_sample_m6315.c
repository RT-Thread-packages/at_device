/*
 * Copyright (c) 2006-2023, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2019-05-13     chenyong     first version
 */

#include <at_device_m6315.h>

#define LOG_TAG                        "at.sample.m6315"
#include <at_log.h>

#define M6315_SAMPLE_DEIVCE_NAME     "m6315"

static struct at_device_m6315 m6315_dev =
{
    M6315_SAMPLE_DEIVCE_NAME,
    M6315_SAMPLE_CLIENT_NAME,

    M6315_SAMPLE_POWER_PIN,
    M6315_SAMPLE_STATUS_PIN,
    M6315_SAMPLE_RECV_BUFF_LEN,
};

static int m6315_device_register(void)
{
    struct at_device_m6315 *m6315 = &m6315_dev;

    return at_device_register(&(m6315->device),
                              m6315->device_name,
                              m6315->client_name,
                              AT_DEVICE_CLASS_M6315,
                              (void *) m6315);
}
INIT_APP_EXPORT(m6315_device_register);

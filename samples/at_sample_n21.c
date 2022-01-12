/*
 * Copyright (c) 2006-2022, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2019-12-07     liang.shao     first version
 */

#include <at_device_n21.h>

#define LOG_TAG                        "at.sample"
#include <at_log.h>

#define N21_SAMPLE_DEIVCE_NAME     "n21"

static struct at_device_n21 sim0 =
{
    N21_SAMPLE_DEIVCE_NAME,
    N21_SAMPLE_CLIENT_NAME,

    N21_SAMPLE_POWER_PIN,
    N21_SAMPLE_STATUS_PIN,
    N21_SAMPLE_RECV_BUFF_LEN,
};

static int n21_device_register(void)
{
    struct at_device_n21 *n21 = &sim0;

    return at_device_register(&(n21->device),
                              n21->device_name,
                              n21->client_name,
                              AT_DEVICE_CLASS_N21,
                              (void *) n21);
}
INIT_APP_EXPORT(n21_device_register);
//MSH_CMD_EXPORT(n21_device_register,n21_device_register);

/*
 * Copyright (c) 2006-2022, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author         Notes
 * 2020-05-22     shuobatian     first version
 */

#include <at_device_n58.h>

#define LOG_TAG                        "at.sample"
#include <at_log.h>

#define N58_SAMPLE_DEIVCE_NAME     "n58"

static struct at_device_n58 sim0 =
{
    N58_SAMPLE_DEIVCE_NAME,
    N58_SAMPLE_CLIENT_NAME,

    N58_SAMPLE_POWER_PIN,
    N58_SAMPLE_STATUS_PIN,
    N58_SAMPLE_RECV_BUFF_LEN,
};

static int n58_device_register(void)
{
    struct at_device_n58 *n58 = &sim0;

    return at_device_register(&(n58->device),
                              n58->device_name,
                              n58->client_name,
                              AT_DEVICE_CLASS_N58,
                              (void *) n58);
}
INIT_APP_EXPORT(n58_device_register);
//MSH_CMD_EXPORT(n58_device_register,n58_device_register);

/*
 * Copyright (c) 2006-2022, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2019-05-13     chenyong     first version
 */

#include <at_device_ec20.h>

#define LOG_TAG                        "at.sample.ec20"
#include <at_log.h>

#define EC20_SAMPLE_DEIVCE_NAME        "e0"

static struct at_device_ec20 e0 =
{
    EC20_SAMPLE_DEIVCE_NAME,
    EC20_SAMPLE_CLIENT_NAME,

    EC20_SAMPLE_POWER_PIN,
    EC20_SAMPLE_STATUS_PIN,
    EC20_SAMPLE_RECV_BUFF_LEN,
};

static int ec20_device_register(void)
{
    struct at_device_ec20 *ec20 = &e0;

    return at_device_register(&(ec20->device),
                              ec20->device_name,
                              ec20->client_name,
                              AT_DEVICE_CLASS_EC20,
                              (void *) ec20);
}
INIT_APP_EXPORT(ec20_device_register);

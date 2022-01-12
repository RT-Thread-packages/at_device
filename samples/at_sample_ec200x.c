/*
 * Copyright (c) 2006-2022, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author            Notes
 * 2019-12-13     qiyongzhong       first version
 */

#include <at_device_ec200x.h>

#define LOG_TAG                        "at.sample.ec200x"
#include <at_log.h>

#define EC200X_SAMPLE_DEIVCE_NAME        "ec200x"

static struct at_device_ec200x _dev =
{
    EC200X_SAMPLE_DEIVCE_NAME,
    EC200X_SAMPLE_CLIENT_NAME,

    EC200X_SAMPLE_POWER_PIN,
    EC200X_SAMPLE_STATUS_PIN,
    EC200X_SAMPLE_WAKEUP_PIN,
    EC200X_SAMPLE_RECV_BUFF_LEN,
};

static int ec200x_device_register(void)
{
    struct at_device_ec200x *ec200x = &_dev;

    return at_device_register(&(ec200x->device),
                              ec200x->device_name,
                              ec200x->client_name,
                              AT_DEVICE_CLASS_EC200X,
                              (void *) ec200x);
}
INIT_APP_EXPORT(ec200x_device_register);


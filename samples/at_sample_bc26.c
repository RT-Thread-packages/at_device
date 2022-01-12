/*
 * Copyright (c) 2006-2022, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author            Notes
 * 2019-12-13     qiyongzhong       first version
 */

#include <at_device_bc26.h>

#define LOG_TAG                        "at.sample.bc26"
#include <at_log.h>

#define BC26_SAMPLE_DEIVCE_NAME        "bc26"

static struct at_device_bc26 _dev =
{
    BC26_SAMPLE_DEIVCE_NAME,
    BC26_SAMPLE_CLIENT_NAME,

    BC26_SAMPLE_POWER_PIN,
    BC26_SAMPLE_STATUS_PIN,
    BC26_SAMPLE_RECV_BUFF_LEN,
};

static int bc26_device_register(void)
{
    struct at_device_bc26 *bc26 = &_dev;

    return at_device_register(&(bc26->device),
                              bc26->device_name,
                              bc26->client_name,
                              AT_DEVICE_CLASS_BC26,
                              (void *) bc26);
}
INIT_APP_EXPORT(bc26_device_register);


/*
 * Copyright (c) 2006-2022, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author            Notes
 * 2020-02-13     luhuadong         first version
 */

#include <at_device_bc28.h>

#define LOG_TAG                        "at.sample.bc28"
#include <at_log.h>

#define BC28_SAMPLE_DEIVCE_NAME        "bc28"

static struct at_device_bc28 _dev =
{
    BC28_SAMPLE_DEIVCE_NAME,
    BC28_SAMPLE_CLIENT_NAME,

    BC28_SAMPLE_POWER_PIN,
    BC28_SAMPLE_STATUS_PIN,
    BC28_SAMPLE_RECV_BUFF_LEN,
};

static int bc28_device_register(void)
{
    struct at_device_bc28 *bc28 = &_dev;

    return at_device_register(&(bc28->device),
                              bc28->device_name,
                              bc28->client_name,
                              AT_DEVICE_CLASS_BC28,
                              (void *) bc28);
}
INIT_APP_EXPORT(bc28_device_register);


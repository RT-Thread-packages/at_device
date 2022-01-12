/*
 * Copyright (c) 2006-2022, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2020-10-28     zhangyang     first version
 */

#include <at_device_l610.h>

#define LOG_TAG                         "at.sample.l610"
#include <at_log.h>

#define L610_SAMPLE_DEIVCE_NAME         "l610"

static struct at_device_l610 sim0 =
{
    L610_SAMPLE_DEIVCE_NAME,
    L610_SAMPLE_CLIENT_NAME,

    L610_SAMPLE_POWER_PIN,
    L610_SAMPLE_STATUS_PIN,
    L610_SAMPLE_RECV_BUFF_LEN,
};

static int l610_device_register(void)
{
    struct at_device_l610 *l610 = &sim0;

    return at_device_register(&(l610->device),
                            l610->device_name,
                            l610->client_name,
                            AT_DEVICE_CLASS_L610,
                            (void *) l610);
}
INIT_APP_EXPORT(l610_device_register);

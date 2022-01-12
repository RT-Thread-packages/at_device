/*
 * Copyright (c) 2006-2022, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author            Notes
 * 2019-12-30     qiyongzhong       first version
 */

#include <at_device_me3616.h>

#define LOG_TAG                        "at.sample.me3616"
#include <at_log.h>

#define ME3616_SAMPLE_DEIVCE_NAME        "me3616"

static struct at_device_me3616 _dev =
{
    ME3616_SAMPLE_DEIVCE_NAME,
    ME3616_SAMPLE_CLIENT_NAME,

    ME3616_SAMPLE_POWER_PIN,
    ME3616_SAMPLE_STATUS_PIN,
    ME3616_SAMPLE_RECV_BUFF_LEN,
};

static int me3616_device_register(void)
{
    struct at_device_me3616 *me3616 = &_dev;

    return at_device_register(&(me3616->device),
                              me3616->device_name,
                              me3616->client_name,
                              AT_DEVICE_CLASS_ME3616,
                              (void *) me3616);
}
INIT_APP_EXPORT(me3616_device_register);


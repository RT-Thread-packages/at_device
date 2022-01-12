/*
 * Copyright (c) 2006-2022, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2019-12-07     liang.shao     first version
 */

#include <at_device_air720.h>

#define LOG_TAG                        "at.sample"
#include <at_log.h>

#define AIR720_SAMPLE_DEIVCE_NAME     "air720"

static struct at_device_air720 sim0 =
{
    AIR720_SAMPLE_DEIVCE_NAME,
    AIR720_SAMPLE_CLIENT_NAME,

    AIR720_SAMPLE_POWER_PIN,
    AIR720_SAMPLE_STATUS_PIN,
    AIR720_SAMPLE_RECV_BUFF_LEN,
};

static int air720_device_register(void)
{
    struct at_device_air720 *air720 = &sim0;

    return at_device_register(&(air720->device),
                              air720->device_name,
                              air720->client_name,
                              AT_DEVICE_CLASS_AIR720,
                              (void *) air720);
}
INIT_APP_EXPORT(air720_device_register);
//MSH_CMD_EXPORT(air720_device_register,air720_device_register);

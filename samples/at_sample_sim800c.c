/*
 * Copyright (c) 2006-2022, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2019-05-13     chenyong     first version
 */

#include <at_device_sim800c.h>

#define LOG_TAG                        "at.sample.sim800"
#include <at_log.h>

#define SIM800C_SAMPLE_DEIVCE_NAME     "sim0"

static struct at_device_sim800c sim0 =
{
    SIM800C_SAMPLE_DEIVCE_NAME,
    SIM800C_SAMPLE_CLIENT_NAME,

    SIM800C_SAMPLE_POWER_PIN,
    SIM800C_SAMPLE_STATUS_PIN,
    SIM800C_SAMPLE_RECV_BUFF_LEN,
};

static int sim800c_device_register(void)
{
    struct at_device_sim800c *sim800c = &sim0;

    return at_device_register(&(sim800c->device),
                              sim800c->device_name,
                              sim800c->client_name,
                              AT_DEVICE_CLASS_SIM800C,
                              (void *) sim800c);
}
INIT_APP_EXPORT(sim800c_device_register);

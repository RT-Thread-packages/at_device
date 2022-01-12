/*
 * Copyright (c) 2006-2022, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2019-05-13     chenyong     first version
 */

#include <at_device_sim76xx.h>

#define LOG_TAG                        "at.sample.sim76"
#include <at_log.h>

#define SIM76XX_SAMPLE_DEIVCE_NAME     "sim1"

static struct at_device_sim76xx sim1 =
{
    SIM76XX_SAMPLE_DEIVCE_NAME,
    SIM76XX_SAMPLE_CLIENT_NAME,

    SIM76XX_SAMPLE_POWER_PIN,
    SIM76XX_SAMPLE_STATUS_PIN,
    SIM76XX_SAMPLE_RECV_BUFF_LEN,
};

static int sim76xx_device_register(void)
{
    struct at_device_sim76xx *sim76xx = &sim1;

    return at_device_register(&(sim76xx->device),
                              sim76xx->device_name,
                              sim76xx->client_name,
                              AT_DEVICE_CLASS_SIM76XX,
                              (void *) sim76xx);
}
INIT_APP_EXPORT(sim76xx_device_register);

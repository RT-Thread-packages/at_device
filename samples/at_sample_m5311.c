/*
 * Copyright (c) 2006-2022, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author            Notes
 * 2020-03-09     LXGMAX       first version
 */

#include <at_device_m5311.h>

#define LOG_TAG              "at.sample.m5311"
#include <at_log.h>

/* Requirement:
 * AT_CMD_MAX_LEN              -> 2048
 * RT_SERIAL_RB_BUFSZ          -> 4096
 * M5311_SAMPLE_RECV_BUFF_LEN  -> 2048
 */

#define M5311_SAMPLE_DEVICE_NAME    "m5311"

static struct at_device_m5311 nb_m5311 = {
        M5311_SAMPLE_DEVICE_NAME,
        M5311_SAMPLE_CLIENT_NAME,

        M5311_SAMPLE_POWER_PIN,
        M5311_SAMPLE_RECV_BUFF_LEN,
};

static int m5311_device_register(void)
{
    struct at_device_m5311 *m5311 = &nb_m5311;

    return at_device_register(&(m5311->device),
                                m5311->device_name,
                                m5311->client_name,
                                AT_DEVICE_CLASS_M5311,
                                (void *) m5311);
}

INIT_APP_EXPORT(m5311_device_register);

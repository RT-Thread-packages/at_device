/*
 * Copyright (c) 2006-2022, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2019-11-23     luliang     first version
 */

#include <at_device_a9g.h>

#define LOG_TAG                   "at.sample.a9g"
#include <at_log.h>

#define A9G_SAMPLE_DEIVCE_NAME     "a9g0"

static struct at_device_a9g a9g0 =
{
    A9G_SAMPLE_DEIVCE_NAME,
    A9G_SAMPLE_CLIENT_NAME,

    A9G_SAMPLE_POWER_PIN,
    A9G_SAMPLE_STATUS_PIN,
    A9G_SAMPLE_RECV_BUFF_LEN,
};

static int a9g_device_register(void)
{
    struct at_device_a9g *a9g = &a9g0;

    return at_device_register(&(a9g->device),
                              a9g->device_name,
                              a9g->client_name,
                              AT_DEVICE_CLASS_A9G,
                              (void *) a9g);
}
INIT_APP_EXPORT(a9g_device_register);

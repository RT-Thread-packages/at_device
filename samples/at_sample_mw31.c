/*
 * Copyright (c) 2006-2022, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2019-06-23     flybreak     first version
 */

#include <at_device_mw31.h>

#define LOG_TAG                        "at.sample.mw31"
#include <at_log.h>

#define MW31_SAMPLE_DEIVCE_NAME     "mw0"

static struct at_device_mw31 mw0 =
{
    MW31_SAMPLE_DEIVCE_NAME,
    MW31_SAMPLE_CLIENT_NAME,

    MW31_SAMPLE_WIFI_SSID,
    MW31_SAMPLE_WIFI_PASSWORD,
    MW31_SAMPLE_RECV_BUFF_LEN,
};

static int mw31_device_register(void)
{
    struct at_device_mw31 *mw31 = &mw0;

    return at_device_register(&(mw31->device),
                              mw31->device_name,
                              mw31->client_name,
                              AT_DEVICE_CLASS_MW31,
                              (void *) mw31);
}
INIT_APP_EXPORT(mw31_device_register);

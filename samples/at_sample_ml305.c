/*
 * Copyright (c) 2006-2023, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-12-16     Jonas        first version
 */

#include <at_device_ml305.h>

#define LOG_TAG                         "at.sample.ml305"
#include <at_log.h>

#define ML305_SAMPLE_DEIVCE_NAME        "ml3050"

#define ML305_SAMPLE_CLIENT_NAME        "uart3"

#if !defined (ML305_SAMPLE_POWER_PIN)
    #define ML305_SAMPLE_POWER_PIN      0x1C
#endif

#if !defined (ML305_SAMPLE_STATUS_PIN)
    #define ML305_SAMPLE_STATUS_PIN     0x1E
#endif

#if !defined (ML305_SAMPLE_RECV_BUFF_LEN)
    #define ML305_SAMPLE_RECV_BUFF_LEN  4096
#endif

#if !defined (AT_DEVICE_CLASS_ML305)
    #define AT_DEVICE_CLASS_ML305       0xFFFFU
#endif
static struct at_device_ml305 ml3050 =
{
    ML305_SAMPLE_DEIVCE_NAME,
    ML305_SAMPLE_CLIENT_NAME,

    ML305_SAMPLE_POWER_PIN,
    ML305_SAMPLE_STATUS_PIN,
    ML305_SAMPLE_RECV_BUFF_LEN,
};

static int ml305_device_register(void)
{
    struct at_device_ml305 *ml305 = &ml3050;

    return at_device_register(&(ml305->device),
                              ml305->device_name,
                              ml305->client_name,
                              AT_DEVICE_CLASS_ML305,
                              (void *) ml305);
}
INIT_APP_EXPORT(ml305_device_register);

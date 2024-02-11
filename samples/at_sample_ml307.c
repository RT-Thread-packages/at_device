/*
 * Copyright (c) 2006-2023, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-12-16     Jonas        first version
 */

#include <at_device_ml307.h>

#define LOG_TAG                         "at.sample.ml307"
#include <at_log.h>

#define ML307_SAMPLE_DEIVCE_NAME        "ml3070"

#define ML307_SAMPLE_CLIENT_NAME        "uart2"

#if !defined (ML307_SAMPLE_POWER_PIN)
    #define ML307_SAMPLE_POWER_PIN      0x1C
#endif

#if !defined (ML307_SAMPLE_STATUS_PIN)
    #define ML307_SAMPLE_STATUS_PIN     0x1E
#endif

#if !defined (ML307_SAMPLE_RECV_BUFF_LEN)
    #define ML307_SAMPLE_RECV_BUFF_LEN  4096
#endif

#if !defined (AT_DEVICE_CLASS_ML307)
    #define AT_DEVICE_CLASS_ML307       0xFFFFU
#endif
static struct at_device_ml307 ml3070 =
{
    ML307_SAMPLE_DEIVCE_NAME,
    ML307_SAMPLE_CLIENT_NAME,

    ML307_SAMPLE_POWER_PIN,
    ML307_SAMPLE_STATUS_PIN,
    ML307_SAMPLE_RECV_BUFF_LEN,
};

static int ml307_device_register(void)
{
    struct at_device_ml307 *ml307 = &ml3070;

    return at_device_register(&(ml307->device),
                              ml307->device_name,
                              ml307->client_name,
                              AT_DEVICE_CLASS_ML307,
                              (void *) ml307);
}
INIT_APP_EXPORT(ml307_device_register);

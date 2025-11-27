/*
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2025-05-13     liuyucai     first version
 */
#include <at_device_fsc_bw236.h>

#define LOG_TAG "at.sample.fsc"
#include <at_log.h>

#define FSC_BW236_SAMPLE_DEIVCE_NAME "fsc0"

#ifndef FSC_BW236_SAMPLE_CLIENT_NAME
#define FSC_BW236_SAMPLE_CLIENT_NAME "uart3"
#endif

#ifndef FSC_BW236_SAMPLE_WIFI_SSID
#define FSC_BW236_SAMPLE_WIFI_SSID "test_ssid"
#endif

#ifndef FSC_BW236_SAMPLE_WIFI_PASSWORD
#define FSC_BW236_SAMPLE_WIFI_PASSWORD "12345678"

#endif
#ifndef FSC_BW236_SAMPLE_RECV_BUFF_LEN
#define FSC_BW236_SAMPLE_RECV_BUFF_LEN ( 256 * 6 )
#endif

static struct at_device_fsc_bw236 fsc0 = {
    FSC_BW236_SAMPLE_DEIVCE_NAME,   FSC_BW236_SAMPLE_CLIENT_NAME,   FSC_BW236_SAMPLE_WIFI_SSID,
    FSC_BW236_SAMPLE_WIFI_PASSWORD, FSC_BW236_SAMPLE_RECV_BUFF_LEN,
};

static int fsc_bw236_device_register( void )
{
    struct at_device_fsc_bw236* fsc_bw236 = &fsc0;
    return at_device_register( &( fsc_bw236->device ),
                               fsc_bw236->device_name,
                               fsc_bw236->client_name,
                               AT_DEVICE_CLASS_FSC_BW236,
                               ( void* )fsc_bw236 );
}
INIT_APP_EXPORT( fsc_bw236_device_register );

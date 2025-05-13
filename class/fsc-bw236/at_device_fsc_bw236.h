/*
 * Copyright (c) 2006-2023, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2025-04-27    liuyucai     first version
 */

#ifndef __AT_DEVICE_FSC_BW236_H__
#define __AT_DEVICE_FSC_BW236_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <at_device.h>
#include <stdlib.h>
#include "rtthread.h"

#define FSC_BW236_DEFAULT_AT_VERSION     "1.1.0.0"
#define FSC_BW236_DEFAULT_AT_VERSION_NUM 0x1010000

#define FSC_BW236_MODULE_SEND_MAX_SIZE 1804 /*< 单次最大1460字节 */
#define FSC_BW236_MODULE_RECV_MAX_SIZE 1460 /*< 单次最大1460字节 */

/* The maximum number of sockets supported by the fsc_bw236 device */
#define AT_DEVICE_FSC_BW236_SOCKETS_NUM 1 /*<  目前只支持一个TCP客户端套接字连接*/

typedef enum
{
    FSC_CONNECT_TYPE_BT_SLAVE,
    FSC_CONNECT_TYPE_BT_MASTER,
    FSC_CONNECT_TYPE_WIFI,
    FSC_CONNECT_TYPE_TCP_SERVER,
    FSC_CONNECT_TYPE_TCP_CLIENT,
    FSC_CONNECT_TYPE_SSL_CLIENT,
    FSC_CONNECT_TYPE_MQTT,
}  fsc_bw236_connect_type_t;

struct at_device_fsc_bw236
{
    char* device_name;
    char* client_name;

    char*            wifi_ssid;
    char*            wifi_password;
    size_t           recv_line_num;
    struct at_device device;
    void*                  user_data;
};
int         chk_connect_status( struct at_device* device, fsc_bw236_connect_type_t type, int8_t retry );
char*       reverse_find_nth_end_sign( char* buf, uint16_t len, char end_sign, uint8_t nth );
int         count_digits( long num );
#ifdef AT_USING_SOCKET

/* fsc_bw236 device socket initialize */
int fsc_bw236_socket_init( struct at_device* device );

/* fsc_bw236 device class socket register */
int fsc_bw236_socket_class_register( struct at_device_class* class );

/* convert the fsc_bw236 AT version string to hexadecimal */
unsigned int fsc_bw236_at_version_to_hex( const char* str );

/* obtain the fsc_bw236 AT version number */
unsigned int fsc_bw236_get_at_version( void );
#endif /* AT_USING_SOCKET */

#ifdef __cplusplus
}
#endif

#endif /* __AT_DEVICE_FSC_BW236_H__ */
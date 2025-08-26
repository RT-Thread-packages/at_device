/*
 * Copyright (c) 2006-2023, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2025-05-13     liuyucai     first version
 */

#include <at_device_fsc_bw236.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LOG_TAG "at.skt.fsc"
#include <at_log.h>

#if defined( AT_DEVICE_USING_FSC_BW236 ) && defined( AT_USING_SOCKET )

at_evt_cb_t                       at_evt_cb_set[] = {
    [AT_SOCKET_EVT_RECV]   = NULL,
    [AT_SOCKET_EVT_CLOSED] = NULL,
#ifdef AT_USING_SOCKET_SERVER
    [AT_SOCKET_EVT_CONNECTED] = NULL,
#endif
};

#ifdef AT_USING_SOCKET_SERVER
#error "AT_USING_SOCKET_SERVER is not supported by fsc_bw236 module."
/* The module can accept TCP connections, but because there is no connection hint, it doesn't know when the
 * client connected.*/
static void urc_connected_func( struct at_client* client, const char* data, rt_size_t size );
#endif
static void                urc_recv_callback( struct at_client* client, const char* data, rt_size_t size );
static const struct at_urc urc_table[] = {
    { "+WFDATA=", "", urc_recv_callback },
};

#ifdef AT_USING_SOCKET_SERVER
static const struct at_urc urc_table_with_server[] = {
    { "", ",CONNECT\r\n", urc_connected_func },
};
#endif

#ifdef AT_USING_SOCKET_SERVER
static int fsc_bw236_server_number = 0;
#endif

/**
 * close socket by AT commands.
 *
 * @param current socket
 *
 * @return  0: close socket success
 *         -1: send AT commands error
 *         -2: wait socket event timeout
 *         -5: no memory
 */
static int fsc_bw236_socket_close( struct at_socket* socket )
{
    int           result = RT_EOK;
    at_response_t resp   = RT_NULL;

    struct at_device* device = ( struct at_device* )socket->device;

    resp = at_create_resp( 64, 0, rt_tick_from_millisecond( 2000 ) );
    if ( resp == RT_NULL ) {
        LOG_E( "no memory for resp create." );
        return -RT_ENOMEM;
    }
    if ( socket->type == AT_SOCKET_UDP ) {
        /* 开启UDP连接后 模组会一直监听 关闭只能执行 AT+REBOOT 重启模组 并且需要大量等待时间 */
        /* 在连接的时候 去做一些处理 */
    }
    else if ( socket->type == AT_SOCKET_TCP ) {
        result = at_obj_exec_cmd( device->client, resp, "AT+CLOSE" );
    }
    if ( resp ) {
        at_delete_resp( resp );
    }

    return result;
}
/**
 * create TCP/UDP client or server connect by AT commands.
 *
 * @param socket current socket
 * @param ip server or client IP address
 * @param port server or client port
 * @param type connect socket type(tcp, udp)
 * @param is_client connection is client
 *
 * @return   0: connect success
 *          -1: connect failed, send commands error or type error
 *          -2: wait socket event timeout
 *          -5: no memory
 */
static int fsc_bw236_socket_connect(
    struct at_socket* socket, char* ip, int32_t port, enum at_socket_type type, rt_bool_t is_client )
{

    int               result        = RT_EOK;
    rt_bool_t         retryed       = RT_FALSE;
    at_response_t     resp          = RT_NULL;
    int               device_socket = ( int )socket->user_data;
    struct at_device* device        = ( struct at_device* )socket->device;

    RT_ASSERT( ip );
    RT_ASSERT( port >= 0 );

    resp = at_create_resp( 128, 0, 5 * RT_TICK_PER_SECOND );
    if ( resp == RT_NULL ) {
        LOG_E( "no memory for resp create." );
        return -RT_ENOMEM;
    }

__retry:
    if ( is_client ) {
        switch ( type ) {
        case AT_SOCKET_TCP:
            /* send AT commands to connect TCP server */

            if ( at_obj_exec_cmd( device->client, resp, "AT+SOCK=TCPC,%d,%s,%d", 8080, ip, port ) < 0 ) {
                result = -RT_ERROR;
            }
            if ( at_obj_exec_cmd( device->client, resp, "AT+WLANC=3" ) < 0 ) {
                result = -RT_ERROR;
            }
            if ( RT_EOK != chk_connect_status( device, FSC_CONNECT_TYPE_TCP_CLIENT, 2 ) ) {
                result = -RT_ERROR;
            }
            break;
        case AT_SOCKET_UDP: {
            /* 保存之前的ip port*/
            static char*    dev_udp_ip;
            static int32_t  dev_udp_port;
            static uint16_t dev_udp_default_port = 4000;
            /*如果ip port 与之前不同 则需要重新设置udp socket 并保存新的ip port*/
            if ( !dev_udp_ip || rt_strcmp( dev_udp_ip, ip ) || dev_udp_port != port ) {
                do {
                    if ( dev_udp_default_port > 4002 ) {
                        if ( at_obj_exec_cmd( device->client, resp, "AT+REBOOT" ) < 0 ) {
                            result = -RT_ERROR;
                            break;
                        }
                        dev_udp_default_port = 4000;
                        rt_thread_delay( 7000 );  // wait for clear udp socket
                    }

                    if ( at_obj_exec_cmd( device->client, resp, "AT+SOCK=UDP,%d", dev_udp_default_port )
                         < 0 ) {
                        result = -RT_ERROR;
                        break;
                    }
                    if ( at_obj_exec_cmd(
                             device->client, resp, "AT+SOCK=UDP,%d,%s,%d", dev_udp_default_port, ip, port )
                         < 0 ) {
                        result = -RT_ERROR;
                        break;
                    }
                    if ( at_obj_exec_cmd( device->client, resp, "AT+WLANC=3" ) < 0 ) {
                        result = -RT_ERROR;
                        break;
                    }
                    if ( dev_udp_ip ) {
                        rt_free( dev_udp_ip );
                    }
                    dev_udp_ip = rt_strdup( ip );
                    if ( dev_udp_ip == RT_NULL ) {
                        LOG_E( "no memory for F:%s L:%d create.", __FUNCTION__, __LINE__ );
                        goto __exit;
                    }
                    ++dev_udp_default_port;
                    dev_udp_port = port;
                    rt_kprintf( "save ip:%s, port:%d\n", dev_udp_ip, dev_udp_port );
                } while ( 0 );
            }
            break;
        }
        default:
            LOG_E( "not supported connect type %d.", type );
            result = -RT_ERROR;
            goto __exit;
        }
    }

    if ( result != RT_EOK && retryed == RT_FALSE ) {
        LOG_E( "%s device socket (%d) connect failed, the socket was not be closed and now will connect "
               "retry.",
               device->name,
               device_socket );
        if ( fsc_bw236_socket_close( socket ) < 0 ) {
            goto __exit;
        }
        retryed = RT_TRUE;
        result  = RT_EOK;
        goto __retry;
    }

__exit:
    if ( resp ) {
        at_delete_resp( resp );
    }

    return result;
}

#ifdef AT_USING_SOCKET_SERVER

/**
 * create TCP/UDP or server connect by AT commands.
 *
 * @param socket current socket
 * @param backlog waiting to handdle work, useless in "at mode"
 *
 * @return   0: connect success
 *          -1: connect failed, send commands error or type error
 *          -2: wait socket event timeout
 *          -5: no memory
 */
int fsc_bw236_socket_listen( struct at_socket* socket, int backlog )
{
    return 0;
}
#endif

/**
 * send data to server or client by AT commands.
 *
 * @param socket current socket
 * @param buff send buffer
 * @param bfsz send buffer size
 * @param type connect socket type(tcp, udp)
 *
 * @return >=0: the size of send success
 *          -1: send AT commands error or send data error
 *          -2: waited socket event timeout
 *          -5: no memory
 */
static int
fsc_bw236_socket_send( struct at_socket* socket, const char* buff, size_t bfsz, enum at_socket_type type )
{
    int               result       = RT_EOK;
    size_t            cur_pkt_size = 0, sent_size = 0;
    at_response_t     resp     = RT_NULL;
    struct at_device* device   = ( struct at_device* )socket->device;
    rt_mutex_t        lock     = device->client->lock;
    uint8_t*          send_cmd = NULL;
    RT_ASSERT( buff );
    RT_ASSERT( bfsz > 0 );

    resp = at_create_resp( 64, 0, RT_TICK_PER_SECOND );
    if ( resp == RT_NULL ) {
        LOG_E( "no memory for resp create." );
        return -RT_ENOMEM;
    }
    send_cmd = rt_malloc( 20 + FSC_BW236_MODULE_SEND_MAX_SIZE );
    if ( send_cmd == RT_NULL ) {
        LOG_E( "no memory for F:%s L:%d create.", __FUNCTION__, __LINE__ );
        result = -RT_ENOMEM;
        goto __exit;
    }
    rt_mutex_take( lock, RT_WAITING_FOREVER );

    uint8_t default_tcps_socket_num = 3;
    uint8_t tcps_socket_num;
    if ( type == AT_SOCKET_UDP ) {
        tcps_socket_num = default_tcps_socket_num + 1;  // MAX+1
    }

    while ( sent_size < bfsz ) {
        if ( bfsz - sent_size < FSC_BW236_MODULE_SEND_MAX_SIZE ) {
            cur_pkt_size = bfsz - sent_size;
        }
        else {
            cur_pkt_size = FSC_BW236_MODULE_SEND_MAX_SIZE;
        }

        uint8_t format_len =
            rt_sprintf( ( char* )send_cmd, "AT+WFSEND=%d,%d,", tcps_socket_num, cur_pkt_size );
        rt_memcpy( send_cmd + format_len, buff, cur_pkt_size );
        rt_memcpy( send_cmd + format_len + cur_pkt_size, "\r\n", 2 );

        at_client_obj_send( device->client, ( char* )send_cmd, format_len + cur_pkt_size + 2 );

        sent_size += cur_pkt_size;
    }
    result = bfsz;
__exit:
    // device->client->resp = NULL;

    rt_free( send_cmd );

    rt_mutex_release( lock );

    if ( resp ) {
        at_delete_resp( resp );
    }

    return result > 0 ? sent_size : result;
}

/**
 * @brief   DNS(domain name resolution) function
 *
 * @param name domain name
 * @param ip parsed IP address, it's length must be 16
 *
 * @return  0: domain resolve success
 *         -2: wait socket event timeout
 *         -5: no memory
 */
static int fsc_bw236_domain_resolve( const char* name, char ip[ 16 ] )
{
    int               result        = RT_EOK;
    char              recv_ip[ 16 ] = { 0 };
    at_response_t     resp          = RT_NULL;
    struct at_device* device        = RT_NULL;
    const char*       line          = RT_NULL;
    RT_ASSERT( name );
    RT_ASSERT( ip );

    device = at_device_get_first_initialized();
    if ( device == RT_NULL ) {
        LOG_E( "get first init device failed." );
        return -RT_ERROR;
    }

    resp = at_create_resp( 128, 0, 10 * RT_TICK_PER_SECOND );
    if ( resp == RT_NULL ) {
        LOG_E( "no memory for resp create." );
        return -RT_ENOMEM;
    }
    if ( at_obj_exec_cmd( device->client, resp, "AT+SOCK=TCPC,9100,%s,8080", name ) < 0 ) {
        result = -RT_ERROR;
        /* !!! 解析失败只能硬件断电复位 并且失败并没有提示 一旦失败 任何AT指令都失去作用*/
        LOG_E( "Module entered fatal state, REQUIRED HARDWARE RESET!" );
        goto __exit;
    }

    if ( at_obj_exec_cmd( device->client, resp, "AT+SOCK" ) < 0 ) {
        result = -RT_ERROR;
        goto __exit;
    }

    line = at_resp_get_line( resp, 2 );
    rt_sscanf( line, "+SOCK=TCPC,9100,%s,8080", recv_ip );
    rt_strncpy( ip, recv_ip, 15 );
    ip[ 15 ] = '\0';

__exit:

    if ( resp ) {
        at_delete_resp( resp );
    }

    return result;
}

/**
 * set AT socket event notice callback
 *
 * @param event notice event
 * @param cb notice callback
 */
static void fsc_bw236_socket_set_event_cb( at_socket_evt_t event, at_evt_cb_t cb )
{
    if ( event < sizeof( at_evt_cb_set ) / sizeof( at_evt_cb_set[ 1 ] ) ) {
        at_evt_cb_set[ event ] = cb;
    }
}

static const struct at_socket_ops fsc_bw236_socket_ops = {
    fsc_bw236_socket_connect,
    fsc_bw236_socket_close,
    fsc_bw236_socket_send,
    fsc_bw236_domain_resolve,
    fsc_bw236_socket_set_event_cb,
#if defined( AT_SW_VERSION_NUM ) && AT_SW_VERSION_NUM > 0x10300
    RT_NULL,
#ifdef AT_USING_SOCKET_SERVER
    fsc_bw236_socket_listen,
#endif
#endif
};

#ifdef AT_USING_SOCKET_SERVER
static void urc_connected_func( struct at_client* client, const char* data, rt_size_t size ) {}
#endif
static void urc_recv_callback( struct at_client* client, const char* data, rt_size_t size )
{
    struct at_device* device      = RT_NULL;
    struct at_socket* socket      = RT_NULL;
    char*             client_name = client->device->parent.name;

    device = at_device_get_by_name( AT_DEVICE_NAMETYPE_CLIENT, client_name );
    if ( device == RT_NULL ) {
        LOG_E( "get device(%s) failed.", client_name );
        return;
    }
    socket = device->sockets;
    if ( socket->state == AT_SOCKET_CONNECT ) {
        /**
         * default max_num: 3
         * TCPS: 0D 0A +WFDATA=<0-max_num>,3,123 0D 0A
         * TCPC: 0D 0A +WFDATA=3,123 0D 0A
         * UDP: 0D 0A +WFDATA=<max_num+1>,3,123 0D 0A
         */
        char* pos;
        char  cnt_buf[ 6 ] = { 0 };
        pos                = cnt_buf;
        char tmp[ 2 ];

        if ( socket->type == AT_SOCKET_UDP ) {
            at_client_obj_recv( client, tmp, 2, 300 );
        }
        while ( 1 ) {
            at_client_obj_recv( client, pos, 1, 300 );
            if ( *pos == ',' ) {
                break;
            }
            ++pos;
        }
        int   cnt = atoi( cnt_buf );
        char* buf = ( char* )rt_malloc( cnt );
        if ( !buf ) {
            LOG_E( "no memory for F:%s L:%d create.", __FUNCTION__, __LINE__ );
        }
        else {
            rt_size_t recv_len = at_client_obj_recv( client, buf, cnt, 300 );
            if ( recv_len == cnt ) {
                if ( at_evt_cb_set[ AT_SOCKET_EVT_RECV ] ) {
                    at_evt_cb_set[ AT_SOCKET_EVT_RECV ]( socket, AT_SOCKET_EVT_RECV, buf, cnt );
                    return;
                }
            }
            else {
                LOG_E( "receive size(%d) data failed.", recv_len );
            }
        }
        while ( at_client_obj_recv( client, tmp, 1, 0 ) );
    }
}

int fsc_bw236_socket_init( struct at_device* device )
{
    RT_ASSERT( device );

    /* register URC data execution function  */
    at_obj_set_urc_table( device->client, urc_table, sizeof( urc_table ) / sizeof( urc_table[ 0 ] ) );

    return RT_EOK;
}

int fsc_bw236_socket_class_register( struct at_device_class* class )
{
    RT_ASSERT( class );

    class->socket_num = AT_DEVICE_FSC_BW236_SOCKETS_NUM;
    class->socket_ops = &fsc_bw236_socket_ops;

    return RT_EOK;
}
#endif /* AT_DEVICE_USING_FSC_BW236 && AT_USING_SOCKET */

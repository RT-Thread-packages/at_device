/*
 * Copyright (c) 2006-2023, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2025-04-27    liuyucai     first version
 */

#include <at_device_fsc_bw236.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LOG_TAG "at.dev.fsc"

#include <at_log.h>

#ifdef AT_DEVICE_USING_FSC_BW236

#define FSC_BW236_WAIT_CONNECT_TIME 5000
#define FSC_BW236_THREAD_STACK_SIZE 2048
#define FSC_BW236_THREAD_PRIORITY   ( RT_THREAD_PRIORITY_MAX / 2 )
#define FSC_BW236_SCAN_MAX_WIFI_NUM 20

unsigned int FSC_BW236_GMR_AT_VERSION;

typedef enum
{
    CONNECT_STATUS_UNINITIALIZED,
    CONNECT_STATUS_IDLE,
    CONNECT_STATUS_CONNECTING,
    CONNECT_STATUS_CONNECTED,
} connect_status_t;
/**
 *@brief Inversely search for the nth ending character, return to the address after it
 *@param buf input buffer pointer
 *@param len Buffer valid data length (bytes)
 *@param end_sign end symbol identification
 *@param nth nth appearance of search (0=first)
 *@return Success: The first character address after the ending character
 *Failed: NULL
 *@note 1. Find direction: end of buffer → head
 *2. nth >= Returns NULL when the actual occurrences are
 */
char* reverse_find_nth_end_sign( char* buf, uint16_t len, char end_sign, uint8_t nth )
{
    char*    p     = buf;
    uint16_t count = 0;

    for ( int i = len - 1; i >= 0; i-- ) {
        if ( p[ i ] == end_sign ) {
            if ( count == nth ) {
                return p + i + 1;
            }
            count++;
        }
    }
    return NULL;
}

/**
 * @brief calculates the decimal digits of an integer (ignoring the symbol)
 *
 * @param num integer value to calculate (negative numbers allowed)
 * @return int Number digits (0 is counted as 1 digit, negative signs are not counted)
 *
 * @note Special value processing:
 *-Enter 0 and return 1
 *-Negative numbers automatically take absolute values ​​to calculate
 *
 * @code
 *count_digits(-314) //Return 3
 *count_digits(0) //Return 1
 *count_digits(12345) //Return 5
 * @endcode
 */
int count_digits( long num )
{
    int  count = 0;
    long n     = ( num < 0 ) ? -num : num;

    if ( n == 0 ) return 1;

    while ( n != 0 ) {
        n /= 10;
        count++;
    }
    return count;
}
/* =============================  fsc_bw236 network interface operations ============================= */

static int fsc_bw236_netdev_set_dns_server( struct netdev* netdev, uint8_t dns_num, ip_addr_t* dns_server );

static void fsc_bw236_get_netdev_info( struct rt_work* work, void* work_data )
{
#define AT_ADDR_LEN 32

    at_response_t     resp = RT_NULL;
    ip_addr_t         ip_addr;
    rt_uint32_t       mac_addr[ 6 ] = { 0 };
    rt_uint32_t       num           = 0;
    struct at_device* device        = ( struct at_device* )work_data;
    struct netdev*    netdev        = device->netdev;
    struct at_client* client        = device->client;

    const char* line = RT_NULL;
    if ( work != RT_NULL ) {
        rt_free( work );
    }

    resp = at_create_resp( 64, 4, rt_tick_from_millisecond( 300 ) );
    if ( resp == RT_NULL ) {
        LOG_E( "no memory for F:%s L:%d create.", __FUNCTION__, __LINE__ );
        return;
    }
    /* AT+LIP ->\r\n+LIP=0.0.0.0\r\n\r\nOK\r\n */
    if ( at_obj_exec_cmd( client, resp, "AT+LIP" ) < 0 ) {
        LOG_E( "%s device send \"AT+LIP\" cmd error.", device->name );
        goto __exit;
    }
    line = ( char* )at_resp_get_line( resp, 2 );
    if ( line ) {
        inet_aton( line + 5, &ip_addr );
        netdev_low_level_set_ipaddr( netdev, &ip_addr );
    }
    /* AT+GW ->\r\n+GW=192.168.1.1\r\n\r\nOK\r\n */
    if ( at_obj_exec_cmd( client, resp, "AT+GW" ) < 0 ) {
        LOG_E( "%s device send \"AT+GW\" cmd error.", device->name );
        goto __exit;
    }
    line = at_resp_get_line( resp, 2 );
    if ( line ) {
        inet_aton( line + 4, &ip_addr );
        netdev_low_level_set_gw( netdev, &ip_addr );
    }
    /* AT+MASK ->\r\n+MASK=255.255.255.0\r\n\r\nOK\r\n */
    if ( at_obj_exec_cmd( client, resp, "AT+MASK" ) < 0 ) {
        LOG_E( "%s device send \"AT+MASK\" cmd error.", device->name );
        goto __exit;
    }
    line = at_resp_get_line( resp, 2 );
    if ( line ) {
        inet_aton( line + 6, &ip_addr );
        netdev_low_level_set_netmask( netdev, &ip_addr );
    }
    /* AT+MAC ->\r\n+MAC=DC0D308F4490\r\n\r\nOK\r\n */
    if ( at_obj_exec_cmd( client, resp, "AT+MAC" ) < 0 ) {
        LOG_E( "%s device send \"AT+MAC\" cmd error.", device->name );
        goto __exit;
    }
    line = at_resp_get_line( resp, 2 );
    if ( line ) {
        rt_sscanf( line + 5,
                   "%2x%2x%2x%2x%2x%2x",
                   &mac_addr[ 0 ],
                   &mac_addr[ 1 ],
                   &mac_addr[ 2 ],
                   &mac_addr[ 3 ],
                   &mac_addr[ 4 ],
                   &mac_addr[ 5 ] );

        for ( num = 0; num < netdev->hwaddr_len; num++ ) {
            netdev->hwaddr[ num ] = mac_addr[ num ];
        }
    }
    /* AT+DNS ->\r\n+DNS=0.0.0.0\r\n\r\nOK\r\n */
    if ( at_obj_exec_cmd( device->client, resp, "AT+DNS" ) < 0 ) {
        LOG_W( "please check and update %s device firmware to support the \"AT+DNS\" cmd.", device->name );
        goto __exit;
    }
    line = at_resp_get_line( resp, 2 );
    if ( line ) {
        inet_aton( line + 5, &ip_addr );
        netdev_low_level_set_dns_server( netdev, 0, &ip_addr );
    }
    /* AT+DHCP ->\r\n+DHCP=1\r\n\r\nOK\r\n */
    if ( at_obj_exec_cmd( device->client, resp, "AT+DHCP" ) < 0 ) {
        LOG_W( "please check and update %s device firmware to support the \"AT+DHCP\" cmd.", device->name );
        goto __exit;
    }
    line = at_resp_get_line( resp, 2 );
    if ( line ) {
        netdev_low_level_set_dhcp_status( netdev, line[ 6 ] & 0x01 ? RT_TRUE : RT_FALSE );
    }

__exit:
    if ( resp ) {
        at_delete_resp( resp );
    }
}

static int fsc_bw236_net_init( struct at_device* device );

static int fsc_bw236_netdev_set_up( struct netdev* netdev )
{
    struct at_device* device = RT_NULL;

    device = at_device_get_by_name( AT_DEVICE_NAMETYPE_NETDEV, netdev->name );
    if ( device == RT_NULL ) {
        LOG_E( "get device(%s) failed.", netdev->name );
        return -RT_ERROR;
    }

    if ( device->is_init == RT_FALSE ) {
        fsc_bw236_net_init( device );
        netdev_low_level_set_status( netdev, RT_TRUE );
        LOG_D( "network interface device(%s) set up status", netdev->name );
    }

    return RT_EOK;
}

static int fsc_bw236_netdev_set_down( struct netdev* netdev )
{
    struct at_device* device = RT_NULL;

    device = at_device_get_by_name( AT_DEVICE_NAMETYPE_NETDEV, netdev->name );
    if ( device == RT_NULL ) {
        LOG_E( "get device by netdev(%s) failed.", netdev->name );
        return -RT_ERROR;
    }

    if ( device->is_init == RT_TRUE ) {
        device->is_init = RT_FALSE;
        netdev_low_level_set_status( netdev, RT_FALSE );
        LOG_D( "network interface device(%s) set down status", netdev->name );
    }

    return RT_EOK;
}

static int
fsc_bw236_netdev_set_addr_info( struct netdev* netdev, ip_addr_t* ip_addr, ip_addr_t* netmask, ip_addr_t* gw )
{
#define IPADDR_RESP_SIZE 128
#define IPADDR_SIZE      16

    int               result                     = RT_EOK;
    at_response_t     resp                       = RT_NULL;
    struct at_device* device                     = RT_NULL;
    char              ip_str[ IPADDR_SIZE ]      = { 0 };
    char              gw_str[ IPADDR_SIZE ]      = { 0 };
    char              netmask_str[ IPADDR_SIZE ] = { 0 };

    RT_ASSERT( netdev );
    RT_ASSERT( ip_addr || netmask || gw );

    device = at_device_get_by_name( AT_DEVICE_NAMETYPE_NETDEV, netdev->name );
    if ( device == RT_NULL ) {
        LOG_E( "get device(%s) failed.", netdev->name );
        return -RT_ERROR;
    }

    resp = at_create_resp( IPADDR_RESP_SIZE, 0, rt_tick_from_millisecond( 300 ) );
    if ( resp == RT_NULL ) {
        LOG_E( "no memory for resp create." );
        result = -RT_ENOMEM;
        goto __exit;
    }

    /* Convert numeric IP address into decimal dotted ASCII representation. */
    if ( ip_addr )
        rt_memcpy( ip_str, inet_ntoa( *ip_addr ), IPADDR_SIZE );
    else
        rt_memcpy( ip_str, inet_ntoa( netdev->ip_addr ), IPADDR_SIZE );

    if ( gw )
        rt_memcpy( gw_str, inet_ntoa( *gw ), IPADDR_SIZE );
    else
        rt_memcpy( gw_str, inet_ntoa( netdev->gw ), IPADDR_SIZE );

    if ( netmask )
        rt_memcpy( netmask_str, inet_ntoa( *netmask ), IPADDR_SIZE );
    else
        rt_memcpy( netmask_str, inet_ntoa( netdev->netmask ), IPADDR_SIZE );

    result = -RT_ERROR;
    do {
        if ( at_obj_exec_cmd( device->client, resp, "AT+DHCP=0" ) < 0 ) {
            break;
        }
        if ( at_obj_exec_cmd( device->client, resp, "AT+SIP=%s", ip_str ) < 0 ) {
            break;
        }
        if ( at_obj_exec_cmd( device->client, resp, "AT+GW=%s", gw_str ) < 0 ) {
            break;
        }
        if ( at_obj_exec_cmd( device->client, resp, "AT+MASK=%s", netmask_str ) < 0 ) {
            break;
        }
        result = RT_EOK;
    } while ( 0 );

    if ( result == -RT_ERROR ) {
        LOG_E( "%s device set address failed.", device->name );
    }
    else {
        /* Update netdev information */
        if ( ip_addr ) netdev_low_level_set_ipaddr( netdev, ip_addr );

        if ( gw ) netdev_low_level_set_gw( netdev, gw );

        if ( netmask ) netdev_low_level_set_netmask( netdev, netmask );

        LOG_D( "%s device set address success.", device->name );
    }

__exit:
    if ( resp ) {
        at_delete_resp( resp );
    }

    return result;
}

static int fsc_bw236_netdev_set_dns_server( struct netdev* netdev, uint8_t dns_num, ip_addr_t* dns_server )
{
#define DNS_RESP_SIZE 128

    int               result = RT_EOK;
    at_response_t     resp   = RT_NULL;
    struct at_device* device = RT_NULL;

    RT_ASSERT( netdev );
    RT_ASSERT( dns_server );

    device = at_device_get_by_name( AT_DEVICE_NAMETYPE_NETDEV, netdev->name );
    if ( device == RT_NULL ) {
        LOG_E( "get device by netdev(%s) failed.", netdev->name );
        return -RT_ERROR;
    }

    resp = at_create_resp( DNS_RESP_SIZE, 0, rt_tick_from_millisecond( 300 ) );
    if ( resp == RT_NULL ) {
        LOG_E( "no memory for resp create." );
        return -RT_ENOMEM;
    }
    result = -RT_ERROR;
    do {
        if ( at_obj_exec_cmd( device->client, resp, "AT+DHCP=0" ) < 0 ) {
            break;
        }
        if ( at_obj_exec_cmd( device->client, resp, "AT+DNS=%s", inet_ntoa( *dns_server ) ) < 0 ) {
            break;
        }
        result = RT_EOK;
    } while ( 0 );

    if ( result == -RT_ERROR ) {
        LOG_E( "%s device set DNS failed.", device->name );
    }
    else {
        netdev_low_level_set_dns_server( netdev, dns_num, dns_server );
        LOG_D( "%s device set DNS(%s) success.", device->name, inet_ntoa( *dns_server ) );
    }

    if ( resp ) {
        at_delete_resp( resp );
    }

    return result;
}

static int fsc_bw236_netdev_set_dhcp( struct netdev* netdev, rt_bool_t is_enabled )
{
#define RESP_SIZE 128
    int               result = RT_EOK;
    at_response_t     resp   = RT_NULL;
    struct at_device* device = RT_NULL;

    RT_ASSERT( netdev );

    device = at_device_get_by_name( AT_DEVICE_NAMETYPE_NETDEV, netdev->name );
    if ( device == RT_NULL ) {
        LOG_E( "get device by netdev(%s) failed.", netdev->name );
        return -RT_ERROR;
    }

    resp = at_create_resp( RESP_SIZE, 0, rt_tick_from_millisecond( 300 ) );
    if ( resp == RT_NULL ) {
        LOG_E( "no memory for resp struct." );
        return -RT_ENOMEM;
    }

    if ( at_obj_exec_cmd( device->client, resp, "AT+DHCP=%d", is_enabled ) < 0 ) {
        LOG_E( "%s device set DHCP status(%d) failed.", device->name, is_enabled );
        result = -RT_ERROR;
        goto __exit;
    }
    else {
        netdev_low_level_set_dhcp_status( netdev, is_enabled );
        LOG_D( "%s device set DHCP status(%d) ok.", device->name, is_enabled );
    }

__exit:
    if ( resp ) {
        at_delete_resp( resp );
    }

    return result;
}

static const struct netdev_ops fsc_bw236_netdev_ops = {
    fsc_bw236_netdev_set_up,        fsc_bw236_netdev_set_down,

    fsc_bw236_netdev_set_addr_info, fsc_bw236_netdev_set_dns_server, fsc_bw236_netdev_set_dhcp,

};

static struct netdev* fsc_bw236_netdev_add( const char* netdev_name )
{
#define ETHERNET_MTU 1500
#define HWADDR_LEN   6
    struct netdev* netdev = RT_NULL;

    RT_ASSERT( netdev_name );

    netdev = netdev_get_by_name( netdev_name );
    if ( netdev != RT_NULL ) {
        return ( netdev );
    }

    netdev = ( struct netdev* )rt_calloc( 1, sizeof( struct netdev ) );
    if ( netdev == RT_NULL ) {
        LOG_E( "no memory for netdev create." );
        return RT_NULL;
    }

    netdev->mtu        = ETHERNET_MTU;
    netdev->ops        = &fsc_bw236_netdev_ops;
    netdev->hwaddr_len = HWADDR_LEN;

#ifdef SAL_USING_AT
    extern int sal_at_netdev_set_pf_info( struct netdev * netdev );
    /* set the network interface socket/netdb operations */
    sal_at_netdev_set_pf_info( netdev );
#endif

    netdev_register( netdev, netdev_name, RT_NULL );

    return netdev;
}

/* =============================  fsc_bw236 device operations ============================= */

#define AT_SEND_CMD( client, resp, cmd )                              \
    do {                                                              \
        if ( at_obj_exec_cmd( ( client ), ( resp ), ( cmd ) ) < 0 ) { \
            result = -RT_ERROR;                                       \
            goto __exit;                                              \
        }                                                             \
    } while ( 0 )

static void fsc_bw236_netdev_start_delay_work( struct at_device* device )
{
    struct rt_work* net_work = RT_NULL;
    net_work                 = ( struct rt_work* )rt_calloc( 1, sizeof( struct rt_work ) );
    if ( net_work == RT_NULL ) {
        return;
    }

    rt_work_init( net_work, fsc_bw236_get_netdev_info, ( void* )device );
    rt_work_submit( net_work, RT_TICK_PER_SECOND );
}

int chk_connect_status( struct at_device* device, fsc_bw236_connect_type_t type, int8_t retry )
{
    int           res  = RT_ERROR;
    at_response_t resp = at_create_resp( 64, 2, 500 );
    const char*   line = RT_NULL;
    if ( !resp ) {
        res = -RT_ENOMEM;
        goto exit;
    }

    do {
        if ( RT_EOK != at_obj_exec_cmd( device->client, resp, "AT+STAT" ) ) {
            res = -RT_ETIMEOUT;
            break;
        }
        /**< \r\n+STAT=0,0,3,1,0,0,0\r\n\r\nOK\r\n */
        line = at_resp_get_line( resp, 2 );
        if ( line && ( line[ 6 + type * 2 ] - '0' ) == CONNECT_STATUS_CONNECTED ) {
            res = RT_EOK;
            break;
        }

    } while ( ( --retry > 0 ) ? ( rt_thread_delay( 1000 ), 1 ) : 0 );

exit:
    if ( resp ) {
        at_delete_resp( resp );
    }
    return res;
}


static int connect_wifi( struct at_device* device )
{
    struct at_device_fsc_bw236* fsc_bw236 = ( struct at_device_fsc_bw236* )device->user_data;

    at_client_t client = device->client;
    int         res    = 0;

    at_response_t resp = at_create_resp( 128, 0, 500 );
    if ( !resp ) {
        res = -RT_ENOMEM;
        LOG_E( "no memory for resp create." );
        goto exit;
    }

    if ( fsc_bw236->wifi_ssid ) {
        if ( fsc_bw236->wifi_password ) {
            if ( rt_strlen( fsc_bw236->wifi_password ) < 8 ) {
                LOG_E( "The pwd is less than 8 bytes long" );
                res = -RT_ERROR;
                goto exit;
            }
            res = at_obj_exec_cmd( client,
                                   resp,
                                   "AT+RAP=%.*s,%.*s",
                                   RT_WLAN_SSID_MAX_LENGTH,
                                   fsc_bw236->wifi_ssid,
                                   RT_WLAN_PASSWORD_MAX_LENGTH,
                                   fsc_bw236->wifi_password );
        }
        else {
            res =
                at_obj_exec_cmd( client, resp, "AT+RAP=%.*s", RT_WLAN_SSID_MAX_LENGTH, fsc_bw236->wifi_ssid );
        }
        if ( RT_EOK == res ) {
            res = chk_connect_status( device, FSC_CONNECT_TYPE_WIFI, 20 );
            if ( RT_EOK != res ) {
                LOG_E( "%s device wifi connect failed, check ssid(%s) and password(%s)",
                       device->name,
                       fsc_bw236->wifi_ssid,
                       fsc_bw236->wifi_password );
                at_obj_exec_cmd( client, resp, "AT+RESTORE" ); /* restore to factory setting */
                at_obj_exec_cmd( client, resp, "AT+TPMODE=0" );
            }
        }
        else {
            res = -RT_ETIMEOUT;
        }
    }
    else {
        LOG_W( "%s device wifi connect failed, ssid is null", device->name );
        res = -RT_EEMPTY;
    }

exit:
    if ( resp ) {
        at_delete_resp( resp );
    }
    return res;
}

static void fsc_bw236_init_thread_entry( void* parameter )
{
    struct at_device* device       = ( struct at_device* )parameter;
    struct at_client* client       = device->client;
    at_response_t     resp         = RT_NULL;
    rt_err_t          result       = RT_EOK;
    rt_size_t         retry_num    = 1;
    rt_bool_t         wifi_is_conn = RT_FALSE;
    const char*       version_info = RT_NULL;
    LOG_D( "%s device initialize start.", device->name );

    /* wait fsc_bw236 device startup finish */
    if ( at_client_obj_wait_connect( client, FSC_BW236_WAIT_CONNECT_TIME ) ) {
        return;
    }

    resp = at_create_resp( 128, 0, 5 * RT_TICK_PER_SECOND );
    if ( resp == RT_NULL ) {
        LOG_E( "no memory for resp create." );
        return;
    }

    while ( retry_num-- ) {

        AT_SEND_CMD( client, resp, "AT+TPMODE=0" );  // 关闭透传模式
        /* set current mode to Wi-Fi station */
        AT_SEND_CMD( client, resp, "AT+ROLE=1" );
        /* get module version */
        at_resp_set_info( resp, 64, 3, 1000 );
        AT_SEND_CMD( client, resp, "AT+VER" );
        /* get AT version*/
        version_info = at_resp_get_line( resp, 2 );
        if ( version_info ) {
            FSC_BW236_GMR_AT_VERSION = fsc_bw236_at_version_to_hex( version_info );
            /* show module version */
            LOG_D( "%s %X", version_info, FSC_BW236_GMR_AT_VERSION );
        }
        /* initialize successfully  */
        result = RT_EOK;
        break;

    __exit:
        if ( result != RT_EOK ) {
            rt_thread_mdelay( 1000 );
            LOG_I( "%s device initialize retry...", device->name );
        }
    }

    /* connect to WiFi AP */
    if ( RT_EOK == connect_wifi( device ) ) {
        wifi_is_conn = RT_TRUE;
    }

    if ( resp ) {
        at_delete_resp( resp );
    }

    if ( result != RT_EOK ) {
        netdev_low_level_set_status( device->netdev, RT_FALSE );
        LOG_E( "%s device initialize failed(%d).", device->name, result );
    }
    else {
        device->is_init = RT_TRUE;
        netdev_low_level_set_status( device->netdev, RT_TRUE );
        if ( wifi_is_conn ) {
            netdev_low_level_set_link_status( device->netdev, RT_TRUE );
            fsc_bw236_netdev_start_delay_work( device );
        }

        LOG_I( "%s device initialize successfully.", device->name );
    }
}

static int fsc_bw236_net_init( struct at_device* device )
{

#ifdef AT_DEVICE_FSC_BW236_INIT_ASYN
    rt_thread_t tid;

    tid = rt_thread_create( "fsc_net",
                            fsc_bw236_init_thread_entry,
                            ( void* )device,
                            FSC_BW236_THREAD_STACK_SIZE,
                            FSC_BW236_THREAD_PRIORITY,
                            20 );
    if ( tid ) {
        rt_thread_startup( tid );
    }
    else {
        LOG_E( "create %s device init thread failed.", device->name );
        return -RT_ERROR;
    }
#else
    fsc_bw236_init_thread_entry( device );
#endif /* AT_DEVICE_FSC_BW236_INIT_ASYN */

    return RT_EOK;
}

static void urc_func_err_callback( struct at_client* client, const char* data, rt_size_t size )
{
    extern const char* at_get_last_cmd( rt_size_t * cmd_size );
    const char*        last_cmd = at_get_last_cmd( &size );
    LOG_E( "ERR %s\nlast cmd: %.*s\n", data, size, last_cmd );
    client->resp_status = AT_RESP_ERROR;
    client->resp        = NULL;
    rt_sem_release( client->resp_notice );
}

static const struct at_urc urc_table[] = {
    { "ERR", "\r\n", urc_func_err_callback }
};

static int fsc_bw236_init( struct at_device* device )
{
    struct at_device_fsc_bw236* fsc_bw236 = ( struct at_device_fsc_bw236* )device->user_data;

    /* initialize AT client */
#if RT_VER_NUM >= 0x50100
    at_client_init( fsc_bw236->client_name, fsc_bw236->recv_line_num, fsc_bw236->recv_line_num );
#else
    at_client_init( fsc_bw236->client_name, fsc_bw236->recv_line_num );
#endif

    device->client = at_client_get( fsc_bw236->client_name );
    if ( device->client == RT_NULL ) {
        LOG_E( "get AT client(%s) failed.", fsc_bw236->client_name );
        return -RT_ERROR;
    }

    /* register URC data execution function  */
    at_obj_set_urc_table( device->client, urc_table, sizeof( urc_table ) / sizeof( urc_table[ 0 ] ) );

#ifdef AT_USING_SOCKET
    fsc_bw236_socket_init( device );
#endif

    /* add fsc_bw236 device to the netdev list */
    device->netdev = fsc_bw236_netdev_add( fsc_bw236->device_name );
    if ( device->netdev == RT_NULL ) {
        LOG_E( "add netdev(%s) failed.", fsc_bw236->device_name );
        return -RT_ERROR;
    }

    /* initialize fsc_bw236 device network */
    return fsc_bw236_netdev_set_up( device->netdev );
}

static int fsc_bw236_deinit( struct at_device* device )
{
    return fsc_bw236_netdev_set_down( device->netdev );
}

/* reset eap8266 device and initialize device network again */
static int fsc_bw236_reset( struct at_device* device )
{
    int               result = RT_EOK;
    struct at_client* client = device->client;

    result = at_obj_exec_cmd( client, RT_NULL, "AT+REBOOT" );
    rt_thread_mdelay( 1000 );

    device->is_init = RT_FALSE;
    if ( at_client_obj_wait_connect( client, FSC_BW236_WAIT_CONNECT_TIME ) ) {
        return -RT_ETIMEOUT;
    }

    /* initialize fsc_bw236 device network */
    fsc_bw236_net_init( device );

    device->is_init = RT_TRUE;

    return result;
}

/* change eap8266 wifi ssid and password information */
static int fsc_bw236_wifi_info_set( struct at_device* device, struct at_device_ssid_pwd* info )
{
    struct at_device_fsc_bw236* fsc_bw236 = ( struct at_device_fsc_bw236* )device->user_data;

    if ( info->ssid == RT_NULL || info->password == RT_NULL ) {
        LOG_E( "input wifi ssid(%s) and password(%s) error.", info->ssid, info->password );
        return -RT_ERROR;
    }
    fsc_bw236->wifi_ssid     = info->ssid;
    fsc_bw236->wifi_password = info->password;
    return connect_wifi( device );
}

static int fsc_bw236_control( struct at_device* device, int cmd, void* arg )
{
    int result = -RT_ERROR;

    RT_ASSERT( device );

    switch ( cmd ) {
    case AT_DEVICE_CTRL_POWER_ON:
    case AT_DEVICE_CTRL_POWER_OFF:
    case AT_DEVICE_CTRL_LOW_POWER:
    case AT_DEVICE_CTRL_SLEEP:
    case AT_DEVICE_CTRL_WAKEUP:
    case AT_DEVICE_CTRL_NET_CONN:
    case AT_DEVICE_CTRL_NET_DISCONN:
    case AT_DEVICE_CTRL_GET_SIGNAL:
    case AT_DEVICE_CTRL_GET_GPS:
    case AT_DEVICE_CTRL_GET_VER:
        LOG_W( "not support the control cmd(%d).", cmd );
        break;
    case AT_DEVICE_CTRL_RESET:
        result = fsc_bw236_reset( device );
        break;
    case AT_DEVICE_CTRL_SET_WIFI_INFO:
        result = fsc_bw236_wifi_info_set( device, ( struct at_device_ssid_pwd* )arg );
        break;
    default:
        LOG_E( "input error control cmd(%d).", cmd );
        break;
    }

    return result;
}

static const struct at_device_ops fsc_bw236_device_ops = {
    fsc_bw236_init,
    fsc_bw236_deinit,
    fsc_bw236_control,
};

static int fsc_bw236_device_class_register( void )
{
    struct at_device_class* class = RT_NULL;

    class = ( struct at_device_class* )rt_calloc( 1, sizeof( struct at_device_class ) );
    if ( class == RT_NULL ) {
        LOG_E( "no memory for class create." );
        return -RT_ENOMEM;
    }

    /* fill fsc_bw236 device class object */
#ifdef AT_USING_SOCKET
    fsc_bw236_socket_class_register( class );
#endif
    class->device_ops = &fsc_bw236_device_ops;

    return at_device_class_register( class, AT_DEVICE_CLASS_FSC_BW236 );
}
INIT_DEVICE_EXPORT( fsc_bw236_device_class_register );

/**
 * Convert the fsc_bw236 AT version string to hexadecimal
 *
 * @param string containing the AT version
 *
 * @return hex_number: Hexadecimal AT version number, example: 4.1.1 -> 0x040101
 */
unsigned int fsc_bw236_at_version_to_hex( const char* str )
{
    const char*  version_start;
    unsigned int numbers[ 4 ] = { 0 };
    unsigned int hex_number;

    if ( str == NULL || *str == '\0' ) {
        LOG_E( "Invalid AT version format\n" );
        return 0;
    }

    /* Get AT version string */
    version_start = str + 16; /**< +VER=FSC-BW236,V6.3.1 */
    if ( version_start ) {
        if ( rt_sscanf( version_start, "%d.%d.%d", &numbers[ 1 ], &numbers[ 2 ], &numbers[ 3 ] ) == 3 ) {
            hex_number = ( numbers[ 1 ] << 16 ) | ( numbers[ 2 ] << 8 ) | numbers[ 3 ];
        }
        else {
            LOG_W( "The AT instruction format is not standard\n" );
            return 0;
        }
    }
    else {
        LOG_W( "The AT+GMR instruction is not supported\n" );
        return 0;
    }

    return hex_number;
}

unsigned int fsc_bw236_get_at_version( void )
{
    return FSC_BW236_GMR_AT_VERSION;
}

#endif /* AT_DEVICE_USING_fsc_bw236 */

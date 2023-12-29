/*
 * Copyright (c) 2006-2023, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-12-16     Jonas        first version
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include <at_device_ml305.h>

//#define AT_DEBUG
#define LOG_TAG                    "at.dev.ml305"
#include <at_log.h>

#ifdef AT_DEVICE_USING_ML305
#define ML305_AT_DEFAULT_TIMEOUT        1000
#define ML305_WAIT_CONNECT_TIME         5000
#define ML305_THREAD_STACK_SIZE         2048
#define ML305_POWER_OFF                 RT_FALSE
#define ML305_POWER_ON                  RT_TRUE
#define ML305_POWER_ON_TIME             3
#define ML305_POWER_OFF_TIME            4
#define ML305_THREAD_PRIORITY           (RT_THREAD_PRIORITY_MAX/2)

/* AT+CSTT command default*/
static char *CSTT_CHINA_MOBILE  = "AT+CSTT=\"CMNET\"";
static char *CSTT_CHINA_UNICOM  = "AT+CSTT=\"UNINET\"";
static char *CSTT_CHINA_TELECOM = "AT+CSTT=\"CTNET\"";

static rt_bool_t ml305_get_power_state(struct at_device *device)
{
    struct at_device_ml305 *ml305 = RT_NULL;

    ml305 = (struct at_device_ml305 *) device->user_data;

    return (!rt_pin_read(ml305->power_status_pin)) ? ML305_POWER_ON : ML305_POWER_OFF;
}

static void ml305_power_on(struct at_device *device)
{
    struct at_device_ml305 *ml305 = RT_NULL;

    ml305 = (struct at_device_ml305 *) device->user_data;

    /* not nead to set pin configuration for ml305 device power on */
    if (ml305->power_pin == -1 || ml305->power_status_pin == -1)
    {
        return;
    }

    if(ml305_get_power_state(device) == ML305_POWER_ON)
    {
        return;
    }

    rt_pin_write(ml305->power_pin, PIN_HIGH);
    rt_thread_mdelay(ML305_POWER_ON_TIME * RT_TICK_PER_SECOND);
    rt_pin_write(ml305->power_pin, PIN_LOW);
}

static void ml305_power_off(struct at_device *device)
{
    struct at_device_ml305 *ml305 = RT_NULL;

    ml305 = (struct at_device_ml305 *) device->user_data;
    do{
        /* not nead to set pin configuration for ml305 device power on */
        if (ml305->power_pin == -1 || ml305->power_status_pin == -1)
        {
            return;
        }

        if(ml305_get_power_state(device) == ML305_POWER_OFF)
        {
            return;
        }

        rt_pin_write(ml305->power_pin, PIN_HIGH);
        rt_thread_mdelay(ML305_POWER_OFF_TIME * RT_TICK_PER_SECOND);
        rt_pin_write(ml305->power_pin, PIN_LOW);
        rt_thread_mdelay(100);
    }while(ml305_get_power_state(device) != ML305_POWER_OFF);
}

static int ml305_check_link_status(struct at_device *device)
{
    at_response_t resp = RT_NULL;
    struct at_device_ml305 *ml305 = RT_NULL;
    int result = -RT_ERROR;
    int link_stat = 0;

    RT_ASSERT(device);

    ml305 = (struct at_device_ml305 *)device->user_data;

    resp = at_create_resp(64, 0, rt_tick_from_millisecond(300));
    if (resp == RT_NULL)
    {
        LOG_E("no memory for resp create.");
        return -RT_ERROR;
    }

    if(at_obj_exec_cmd(device->client, resp, "AT+CGREG?") < 0)
    {
        result = -RT_ERROR;
        goto __exit;
    }

    if (at_resp_parse_line_args_by_kw(resp, "+CGREG:", "+CGREG: %*d,%d", &link_stat) > 0)
    {
        if (link_stat == 1 || link_stat == 5)
        {
            result = RT_EOK;
        }
    }

    if(at_obj_exec_cmd(device->client, resp, "AT+CGACT?") < 0)
    {
        result = -RT_ERROR;
        goto __exit;
    }
    //+CGACT: 1,1
    if (at_resp_parse_line_args_by_kw(resp, "+CGACT:", "+CGACT: %*d,%d", &link_stat) > 0)
    {
        result = link_stat;
    }

__exit:

    if(resp)
    {
        at_delete_resp(resp);
    }

    return(result);
}

/* =============================  ml305 network interface operations ============================= */

/* set ml305 network interface device status and address information */
static int ml305_netdev_set_info(struct netdev *netdev)
{
#define ML305_IMEI_RESP_SIZE      32
#define ML305_IPADDR_RESP_SIZE    64
#define ML305_DNS_RESP_SIZE       96
#define ML305_INFO_RESP_TIMO      rt_tick_from_millisecond(300)

    int result = RT_EOK;
    ip_addr_t addr;
    at_response_t resp = RT_NULL;
    struct at_device *device = RT_NULL;

    RT_ASSERT(netdev);

    device = at_device_get_by_name(AT_DEVICE_NAMETYPE_NETDEV, netdev->name);
    if (device == RT_NULL)
    {
        LOG_E("get device(%s) failed.", netdev->name);
        return -RT_ERROR;
    }

    /* set network interface device status */
    netdev_low_level_set_status(netdev, RT_TRUE);
    netdev_low_level_set_link_status(netdev, RT_TRUE);
    netdev_low_level_set_dhcp_status(netdev, RT_TRUE);

    resp = at_create_resp(ML305_IMEI_RESP_SIZE, 0, ML305_INFO_RESP_TIMO);
    if (resp == RT_NULL)
    {
        LOG_E("no memory for resp create.");
        result = -RT_ENOMEM;
        goto __exit;
    }

    /* set network interface device hardware address(IMEI) */
    {
        #define ML305_NETDEV_HWADDR_LEN   8
        #define ML305_IMEI_LEN            15

        char imei[ML305_IMEI_LEN] = {0};
        int i = 0, j = 0;

        /* send "AT+GSN" commond to get device IMEI */
        if (at_obj_exec_cmd(device->client, resp, "AT+GSN=1") < 0)
        {
            result = -RT_ERROR;
            goto __exit;
        }
        if(at_resp_parse_line_args_by_kw(resp, "+GSN:", "+GSN:%s", imei) <= 0)
        {
            LOG_E("ml305 device(%s) prase \"AT+GSN\" commands resposne data error.", device->name);
            result = -RT_ERROR;
            goto __exit;
        }

        LOG_D("ml305 device(%s) IMEI number: %s", device->name, imei);

        netdev->hwaddr_len = ML305_NETDEV_HWADDR_LEN;
        /* get hardware address by IMEI */
        for (i = 0, j = 0; i < ML305_NETDEV_HWADDR_LEN && j < ML305_IMEI_LEN; i++, j += 2)
        {
            if (j != ML305_IMEI_LEN - 1)
            {
                netdev->hwaddr[i] = (imei[j] - '0') * 10 + (imei[j + 1] - '0');
            }
            else
            {
                netdev->hwaddr[i] = (imei[j] - '0');
            }
        }
    }

    /* set network interface device IP address */
    {
        #define IP_ADDR_SIZE_MAX    16
        char ipaddr[IP_ADDR_SIZE_MAX] = {0};

        at_resp_set_info(resp, ML305_IPADDR_RESP_SIZE, 2, ML305_INFO_RESP_TIMO);

        /* send "AT+CIFSR" commond to get IP address */
        if (at_obj_exec_cmd(device->client, resp, "AT+CGPADDR=1") < 0)
        {
            result = -RT_ERROR;
            goto __exit;
        }

        if (at_resp_parse_line_args_by_kw(resp, "+CGPADDR:", "+CGPADDR: %*d,\"%[^\"]", ipaddr) <= 0)
        {
            LOG_E("ml305 device(%s) prase \"AT+CGPADDR=1\" commands resposne data error!", device->name);
            result = -RT_ERROR;
            goto __exit;
        }

        LOG_D("ml305 device(%s) IP address: %s", device->name, ipaddr);

        /* set network interface address information */
        inet_aton(ipaddr, &addr);
        netdev_low_level_set_ipaddr(netdev, &addr);
    }

    /* set network interface device dns server */
    {
        #define DNS_ADDR_SIZE_MAX   16
        char dns_server1[DNS_ADDR_SIZE_MAX] = {0}, dns_server2[DNS_ADDR_SIZE_MAX] = {0};

        at_resp_set_info(resp, ML305_DNS_RESP_SIZE, 0, ML305_INFO_RESP_TIMO);

        /* send "AT+CDNSCFG?" commond to get DNS servers address */
        if (at_obj_exec_cmd(device->client, resp, "AT+MDNSCFG?") < 0)
        {
            result = -RT_ERROR;
            goto __exit;
        }
        //+MDNSCFG: IPV4 DNS1:211.136.17.107 IPV4 DNS2:211.136.20.203
        const char *dns_str = at_resp_get_line_by_kw(resp, "DNS1:");
        const char *dns1_str = strstr(dns_str, "DNS1:");
        sscanf(dns1_str, "DNS1:%s", dns_server1);
        const char *dns2_str = strstr(dns_str, "DNS2:");
        sscanf(dns2_str, "DNS2:%s", dns_server2);

        LOG_D("ml305 device(%s) primary DNS server address: %s", device->name, dns_server1);
        LOG_D("ml305 device(%s) secondary DNS server address: %s", device->name, dns_server2);

        inet_aton(dns_server1, &addr);
        netdev_low_level_set_dns_server(netdev, 0, &addr);

        inet_aton(dns_server2, &addr);
        netdev_low_level_set_dns_server(netdev, 1, &addr);
    }

__exit:
    if (resp)
    {
        at_delete_resp(resp);
    }

    return result;
}

static void check_link_status_entry(void *parameter)
{
#define ML305_LINK_STATUS_OK   1
#define ML305_LINK_RESP_SIZE   128
#define ML305_LINK_RESP_TIMO   (3 * RT_TICK_PER_SECOND)
#define ML305_LINK_DELAY_TIME  (30 * RT_TICK_PER_SECOND)

    at_response_t resp = RT_NULL;
    int result_code, link_status;
    struct at_device *device = RT_NULL;
    struct netdev *netdev = (struct netdev *)parameter;

    device = at_device_get_by_name(AT_DEVICE_NAMETYPE_NETDEV, netdev->name);
    if (device == RT_NULL)
    {
        LOG_E("get device(%s) failed.", netdev->name);
        return;
    }

    resp = at_create_resp(ML305_LINK_RESP_SIZE, 0, ML305_LINK_RESP_TIMO);
    if (resp == RT_NULL)
    {
        LOG_E("no memory for response create.");
        return;
    }
    while(1)
    {
        link_status = ml305_check_link_status(device);
        if(link_status < 0)
        {
            rt_thread_mdelay(ML305_LINK_DELAY_TIME);
            continue;
        }
        /* check the network interface device link status  */
        if ((ML305_LINK_STATUS_OK == link_status) != netdev_is_link_up(netdev))
        {
            netdev_low_level_set_link_status(netdev, (ML305_LINK_STATUS_OK == link_status));
        }

        rt_thread_mdelay(ML305_LINK_DELAY_TIME);
    }
}

static int ml305_netdev_check_link_status(struct netdev *netdev)
{
#define ML305_LINK_THREAD_TICK           20
#define ML305_LINK_THREAD_STACK_SIZE     (1024 + 512)
#define ML305_LINK_THREAD_PRIORITY       (RT_THREAD_PRIORITY_MAX - 2)

    rt_thread_t tid;
    char tname[RT_NAME_MAX] = {0};

    if (netdev == RT_NULL)
    {
        LOG_E("input network interface device is NULL.\n");
        return -RT_ERROR;
    }

    rt_snprintf(tname, RT_NAME_MAX, "%s_link", netdev->name);

    tid = rt_thread_create(tname, check_link_status_entry, (void *) netdev,
            ML305_LINK_THREAD_STACK_SIZE, ML305_LINK_THREAD_PRIORITY, ML305_LINK_THREAD_TICK);
    if (tid)
    {
        rt_thread_startup(tid);
    }

    return RT_EOK;
}

static int ml305_net_init(struct at_device *device);

static int ml305_netdev_set_up(struct netdev *netdev)
{
    struct at_device *device = RT_NULL;

    device = at_device_get_by_name(AT_DEVICE_NAMETYPE_NETDEV, netdev->name);
    if (device == RT_NULL)
    {
        LOG_E("get ml305 device by netdev name(%s) failed.", netdev->name);
        return -RT_ERROR;
    }

    if (device->is_init == RT_FALSE)
    {
        device->is_init = RT_TRUE;
        ml305_net_init(device);

        netdev_low_level_set_status(netdev, RT_TRUE);
        LOG_D("the network interface device(%s) set up status.", netdev->name);
    }

    return RT_EOK;
}

static int ml305_netdev_set_down(struct netdev *netdev)
{
    struct at_device *device = RT_NULL;

    device = at_device_get_by_name(AT_DEVICE_NAMETYPE_NETDEV, netdev->name);
    if (device == RT_NULL)
    {
        LOG_E("get ml305 device by netdev name(%s) failed.", netdev->name);
        return -RT_ERROR;
    }

    if (device->is_init == RT_TRUE)
    {
        ml305_power_off(device);
        device->is_init = RT_FALSE;

        netdev_low_level_set_status(netdev, RT_FALSE);
        LOG_D("the network interface device(%s) set down status.", netdev->name);
    }

    return RT_EOK;
}

static int ml305_netdev_set_dns_server(struct netdev *netdev, uint8_t dns_num, ip_addr_t *dns_server)
{
#define ML305_DNS_RESP_LEN     8
#define ML305_DNS_RESP_TIMEO   rt_tick_from_millisecond(300)

    int result = RT_EOK;
    at_response_t resp = RT_NULL;
    struct at_device *device = RT_NULL;

    RT_ASSERT(netdev);
    RT_ASSERT(dns_server);

    device = at_device_get_by_name(AT_DEVICE_NAMETYPE_NETDEV, netdev->name);
    if (device == RT_NULL)
    {
        LOG_E("get ml305 device by netdev name(%s) failed.", netdev->name);
        return -RT_ERROR;
    }

    resp = at_create_resp(ML305_DNS_RESP_LEN, 0, ML305_DNS_RESP_TIMEO);
    if (resp == RT_NULL)
    {
        LOG_D("ml305 set dns server failed, no memory for response object.");
        result = -RT_ENOMEM;
        goto __exit;
    }

    /* send "AT+CDNSCFG=<pri_dns>[,<sec_dns>]" commond to set dns servers */
    if (at_obj_exec_cmd(device->client, resp, "AT+CDNSCFG=\"%s\"", inet_ntoa(*dns_server)) < 0)
    {
        result = -RT_ERROR;
        goto __exit;
    }

    netdev_low_level_set_dns_server(netdev, dns_num, dns_server);

__exit:
    if (resp)
    {
        at_delete_resp(resp);
    }

    return result;
}

static int ml305_ping_domain_resolve(struct at_device *device, const char *name, char ip[16])
{
    int result = RT_EOK;
    char recv_ip[16] = { 0 };
    at_response_t resp = RT_NULL;

    /* The maximum response time is 14 seconds, affected by network status */
    resp = at_create_resp(512, 4, 14 * RT_TICK_PER_SECOND);
    if (resp == RT_NULL)
    {
        LOG_E("no memory for ml305 device(%s) response structure.", device->name);
        return -RT_ENOMEM;
    }

    if (at_obj_exec_cmd(device->client, resp, "AT+CGACT=1,1") < 0)
    {
        result = -RT_ERROR;
        goto __exit;
    }

    if (at_obj_exec_cmd(device->client, resp, "AT+MDNSGIP=\"%s\"", name) < 0)
    {
        result = -RT_ERROR;
        goto __exit;
    }

    if (at_resp_parse_line_args_by_kw(resp, "+MDNSGIP:", "%*[^,],%*[^,],\"%[^\"]", recv_ip) < 0)
    {
        rt_thread_mdelay(100);
        /* resolve failed, maybe receive an URC CRLF */
    }

    if (rt_strlen(recv_ip) < 8)
    {
        rt_thread_mdelay(100);
        /* resolve failed, maybe receive an URC CRLF */
    }
    else
    {
        rt_strncpy(ip, recv_ip, 15);
        ip[15] = '\0';
    }

__exit:
    if (resp)
    {
        at_delete_resp(resp);
    }

    return result;
}

#ifdef NETDEV_USING_PING
#ifdef AT_DEVICE_USING_ML305
static int ml305_netdev_ping(struct netdev *netdev, const char *host,
        size_t data_len, uint32_t timeout, struct netdev_ping_resp *ping_resp)
{
    rt_kprintf("I don't have PING function!\r\n");
    return RT_EOK;
}
#endif
#endif /* NETDEV_USING_PING */

#ifdef NETDEV_USING_NETSTAT
void ml305_netdev_netstat(struct netdev *netdev)
{
    // TODO netstat support
}
#endif /* NETDEV_USING_NETSTAT */

const struct netdev_ops ml305_netdev_ops =
{
    ml305_netdev_set_up,
    ml305_netdev_set_down,

    RT_NULL, /* not support set ip, netmask, gatway address */
    ml305_netdev_set_dns_server,
    RT_NULL, /* not support set DHCP status */

#ifdef NETDEV_USING_PING
    ml305_netdev_ping,
#endif
#ifdef NETDEV_USING_NETSTAT
    ml305_netdev_netstat,
#endif
};

static struct netdev *ml305_netdev_add(const char *netdev_name)
{
#define ML305_NETDEV_MTU       1500
    struct netdev *netdev = RT_NULL;

    RT_ASSERT(netdev_name);

    netdev = netdev_get_by_name(netdev_name);
    if (netdev != RT_NULL)
    {
        return (netdev);
    }

    netdev = (struct netdev *) rt_calloc(1, sizeof(struct netdev));
    if (netdev == RT_NULL)
    {
        LOG_E("no memory for ml305 device(%s) netdev structure.", netdev_name);
        return RT_NULL;
    }

    netdev->mtu = ML305_NETDEV_MTU;
    netdev->ops = &ml305_netdev_ops;

#ifdef SAL_USING_AT
    extern int sal_at_netdev_set_pf_info(struct netdev *netdev);
    /* set the network interface socket/netdb operations */
    sal_at_netdev_set_pf_info(netdev);
#endif

    netdev_register(netdev, netdev_name, RT_NULL);

    return netdev;
}

/* =============================  ml305 device operations ============================= */

#define AT_SEND_CMD(client, resp, resp_line, timeout, cmd)                                         \
    do {                                                                                           \
        (resp) = at_resp_set_info((resp), 128, (resp_line), rt_tick_from_millisecond(timeout));    \
        if (at_obj_exec_cmd((client), (resp), (cmd)) < 0)                                          \
        {                                                                                          \
            result = -RT_ERROR;                                                                    \
            goto __exit;                                                                           \
        }                                                                                          \
    } while(0)                                                                                     \

/* init for ml305 */
static void ml305_init_thread_entry(void *parameter)
{
#define INIT_RETRY                      5
#define CPIN_RETRY                      10
#define CSQ_RETRY                       10
#define CREG_RETRY                      10
#define CGREG_RETRY                     20
#define IPADDR_RETRY                    10
#define CGATT_RETRY                     10
#define COMMON_RETRY                    10
    int i, qimux, retry_num = INIT_RETRY;
    char parsed_data[10] = {0};
    rt_err_t result = RT_EOK;
    at_response_t resp = RT_NULL;
    struct at_device *device = (struct at_device *)parameter;
    struct at_client *client = device->client;

    resp = at_create_resp(128, 0, rt_tick_from_millisecond(500));
    if (resp == RT_NULL)
    {
        LOG_E("no memory for ml305 device(%s) response structure.", device->name);
        return;
    }

    LOG_D("start initializing the device(%s)", device->name);
    ml305_power_off(device);
    while (retry_num--)
    {
        rt_memset(parsed_data, 0, sizeof(parsed_data));
        rt_thread_mdelay(500);
        ml305_power_on(device);
        rt_thread_mdelay(1000);

        /* wait ml305 startup finish */
        if (at_client_obj_wait_connect(client, ML305_WAIT_CONNECT_TIME))
        {
            result = -RT_ETIMEOUT;
            goto __exit;
        }

        /* disable echo */
        AT_SEND_CMD(client, resp, 0, ML305_AT_DEFAULT_TIMEOUT, "ATE0");
        /* get module version */
        AT_SEND_CMD(client, resp, 5, ML305_AT_DEFAULT_TIMEOUT, "ATI");
        /* show module version */
        for (i = 0; i < (int)resp->line_counts - 1; i++)
        {
            LOG_D("%s", at_resp_get_line(resp, i + 1));
        }
        /* check SIM card */
        for (i = 0; i < CPIN_RETRY; i++)
        {
            AT_SEND_CMD(client, resp, 2, 5 * RT_TICK_PER_SECOND, "AT+CPIN?");

            if (at_resp_get_line_by_kw(resp, "READY"))
            {
                LOG_I("ml305 device(%s) SIM card detection success.", device->name);
                break;
            }
            rt_thread_mdelay(500);
        }
        if (i == CPIN_RETRY)
        {
            LOG_E("ml305 device(%s) SIM card detection failed.", device->name);
            result = -RT_ERROR;
            goto __exit;
        }
        /* check SIM card */
        for (i = 0; i < CPIN_RETRY; i++)
        {
            AT_SEND_CMD(client, resp, 2, 10 * 1000, "AT+ICCID");
            if (at_resp_get_line_by_kw(resp, "+ICCID:"))
            {
                LOG_D("%s device SIM card detection success.", device->name);
                break;
            }
            rt_thread_mdelay(1000);
        }
        if (i == CPIN_RETRY)
        {
            LOG_E("%s device SIM card detection failed.", device->name);
            result = -RT_ERROR;
            goto __exit;
        }

        /* check signal strength */
        for (i = 0; i < CSQ_RETRY; i++)
        {
            AT_SEND_CMD(client, resp, 0, ML305_AT_DEFAULT_TIMEOUT, "AT+CSQ");
            at_resp_parse_line_args_by_kw(resp, "+CSQ:", "+CSQ: %s", &parsed_data);
            if (rt_strncmp(parsed_data, "99,99", sizeof(parsed_data)))
            {
                LOG_D("signal strength: %s", parsed_data);
                break;
            }
            rt_thread_mdelay(2000);
        }
        if(i == CSQ_RETRY)
        {
            LOG_E("%s device signal strength check failed(%s).", device->name, parsed_data);
            result = -RT_ERROR;
            goto __exit;
        }

        /* check the GSM network is registered */
        for (i = 0; i < CREG_RETRY; i++)
        {
            AT_SEND_CMD(client, resp, 0, ML305_AT_DEFAULT_TIMEOUT, "AT+CREG?");
            at_resp_parse_line_args_by_kw(resp, "+CREG:", "+CREG: %s", &parsed_data);
            if (!strncmp(parsed_data, "0,1", sizeof(parsed_data)) ||
                !strncmp(parsed_data, "0,5", sizeof(parsed_data)))
            {
                LOG_D("ml305 device(%s) GSM network is registered(%s),", device->name, parsed_data);
                break;
            }
            if(!strncmp(parsed_data, "0,3", sizeof(parsed_data)))
            {
                LOG_E("%s device GSM network is register failed(%s).", device->name, parsed_data);
                result = -RT_ERROR;
                goto __exit;
            }
            rt_thread_mdelay(1000 + 500 * (i + 1));
        }
        if (i == CREG_RETRY)
        {
            LOG_E("%s device GSM network is register failed(%s).", device->name, parsed_data);
            result = -RT_ERROR;
            goto __exit;
        }

        /* check packet domain attach or detach */
        for (i = 0; i < CGATT_RETRY; i++)
        {
            AT_SEND_CMD(client, resp, 0, ML305_AT_DEFAULT_TIMEOUT, "AT+CGATT?");
            at_resp_parse_line_args_by_kw(resp, "+CGATT:", "+CGATT: %s", &parsed_data);
            if (!rt_strncmp(parsed_data, "1", 1))
            {
                LOG_D("%s device Packet domain attach.", device->name);
                break;
            }

            rt_thread_mdelay(1000);
        }
        if (i == CGATT_RETRY)
        {
            LOG_E("%s device GPRS attach failed.", device->name);
            result = -RT_ERROR;
            goto __exit;
        }

        /* Define PDP Context */
        for (i = 0; i < COMMON_RETRY; i++)
        {
            if (at_obj_exec_cmd(client, resp, "AT+CGDCONT=1,\"IP\",\"CMIOT\"") == RT_EOK)
            {
                LOG_D("%s device Define PDP Context Success.", device->name);
                break;
            }
            rt_thread_mdelay(100);
        }
        if (i == COMMON_RETRY)
        {
            LOG_E("%s device Define PDP Context failed.", device->name);
            result = -RT_ERROR;
            goto __exit;
        }

        /* PDP Context Activate*/
        for (i = 0; i < COMMON_RETRY; i++)
        {
            if (at_obj_exec_cmd(client, resp, "AT+CGACT=1,1") == RT_EOK)
            {
                LOG_D("%s device PDP Context Activate Success.", device->name);
                break;
            }
            rt_thread_mdelay(500);
        }
        if (i == COMMON_RETRY)
        {
            LOG_E("%s device PDP Context Activate failed.", device->name);
            result = -RT_ERROR;
            goto __exit;
        }

#if defined (AT_DEBUG)
        /* check the GPRS network IP address */
        for (i = 0; i < IPADDR_RETRY; i++)
        {
            if (at_obj_exec_cmd(client, resp, "AT+CGPADDR=1") == RT_EOK)
            {
                #define IP_ADDR_SIZE_MAX    16
                char ipaddr[IP_ADDR_SIZE_MAX] = {0};

                /* parse response data "+CGPADDR: 1,<IP_address>" */
                if (at_resp_parse_line_args_by_kw(resp, "+CGPADDR:", "+CGPADDR: %*d,%s", ipaddr) > 0)
                {
                    LOG_D("%s device IP address: %s", device->name, ipaddr);
                    break;
                }
            }
            rt_thread_mdelay(1000);
        }
        if (i == IPADDR_RETRY)
        {
            LOG_E("%s device GPRS is get IP address failed", device->name);
            result = -RT_ERROR;
            goto __exit;
        }
#endif

        result = RT_EOK;

__exit:
        if (result == RT_EOK)
        {
            break;
        }
        else
        {
            /* power off the ml305 device */
            ml305_power_off(device);
            rt_thread_mdelay(1000);

            LOG_I("ml305 device(%s) initialize retry...", device->name);
        }
    }

    if (resp)
    {
        at_delete_resp(resp);
    }

    if (result == RT_EOK)
    {
        device->is_init = RT_TRUE;

        /* set network interface device status and address information */
        ml305_netdev_set_info(device->netdev);
        /*  */
        ml305_netdev_check_link_status(device->netdev);
        /*  */
        LOG_I("ml305 device(%s) network initialize success!", device->name);

    }
    else
    {
        device->is_init = RT_FALSE;

        netdev_low_level_set_status(device->netdev, RT_FALSE);
        LOG_E("ml305 device(%s) network initialize failed(%d)!", device->name, result);
    }
}

static int ml305_net_init(struct at_device *device)
{
#ifdef AT_DEVICE_ML305_INIT_ASYN
    rt_thread_t tid;

    tid = rt_thread_create("ml305_net_init", ml305_init_thread_entry, (void *)device,
                ML305_THREAD_STACK_SIZE, ML305_THREAD_PRIORITY, 20);
    if (tid)
    {
        rt_thread_startup(tid);
    }
    else
    {
        LOG_E("create ml305 device(%s) initialization thread failed.", device->name);
        return -RT_ERROR;
    }
#else
    ml305_init_thread_entry(device);
#endif /* AT_DEVICE_ML305_INIT_ASYN */

    return RT_EOK;
}

static void urc_func(struct at_client *client, const char *data, rt_size_t size)
{
    RT_ASSERT(data);

    LOG_I("URC data : %.*s", size, data);
}

/* ml305 device URC table for the device control */
static const struct at_urc urc_table[] =
{
        {"READY",         "\r\n",                 urc_func},
};

static int ml305_init(struct at_device *device)
{
    struct at_device_ml305 *ml305 = (struct at_device_ml305 *) device->user_data;

    struct serial_configure serial_config = RT_SERIAL_CONFIG_DEFAULT;

    rt_device_t serial = rt_device_find(ml305->client_name);

     if (serial == RT_NULL)
    {
        LOG_E("ml305 device(%s) initialize failed, get AT client(%s) failed.", ml305->device_name, ml305->client_name);
        return -RT_ERROR;
    }
    serial_config.bufsz = ml305->recv_buff_size;
    rt_device_control(serial, RT_DEVICE_CTRL_CONFIG, &serial_config);

    /* initialize AT client */
    at_client_init(ml305->client_name, ml305->recv_buff_size, ml305->recv_buff_size);

    device->client = at_client_get(ml305->client_name);
    if (device->client == RT_NULL)
    {
        LOG_E("ml305 device(%s) initialize failed, get AT client(%s) failed.", ml305->device_name, ml305->client_name);
        return -RT_ERROR;
    }

    /* register URC data execution function  */
    at_obj_set_urc_table(device->client, urc_table, sizeof(urc_table) / sizeof(urc_table[0]));

#ifdef AT_USING_SOCKET
    ml305_socket_init(device);
#endif

    /* add ml305 device to the netdev list */
    device->netdev = ml305_netdev_add(ml305->device_name);
    if (device->netdev == RT_NULL)
    {
        LOG_E("ml305 device(%s) initialize failed, get network interface device failed.", ml305->device_name);
        return -RT_ERROR;
    }

    /* initialize ml305 pin configuration */
    if (ml305->power_pin != -1)
    {
        rt_pin_mode(ml305->power_pin, PIN_MODE_OUTPUT);
    }

    if (ml305->power_status_pin != -1)
    {
        rt_pin_mode(ml305->power_status_pin, PIN_MODE_INPUT_PULLUP);
    }
    /* initialize ml305 device network */
    return ml305_netdev_set_up(device->netdev);
}

static int ml305_deinit(struct at_device *device)
{
    return ml305_netdev_set_down(device->netdev);
}

static int ml305_control(struct at_device *device, int cmd, void *arg)
{
    int result = -RT_ERROR;

    RT_ASSERT(device);

    switch (cmd)
    {
    case AT_DEVICE_CTRL_POWER_ON:
    case AT_DEVICE_CTRL_POWER_OFF:
    case AT_DEVICE_CTRL_RESET:
    case AT_DEVICE_CTRL_LOW_POWER:
    case AT_DEVICE_CTRL_SLEEP:
    case AT_DEVICE_CTRL_WAKEUP:
    case AT_DEVICE_CTRL_NET_CONN:
    case AT_DEVICE_CTRL_NET_DISCONN:
    case AT_DEVICE_CTRL_SET_WIFI_INFO:
    case AT_DEVICE_CTRL_GET_SIGNAL:
    case AT_DEVICE_CTRL_GET_GPS:
    case AT_DEVICE_CTRL_GET_VER:
        LOG_W("ml305 not support the control command(%d).", cmd);
        break;
    default:
        LOG_E("input error control command(%d).", cmd);
        break;
    }

    return result;
}

const struct at_device_ops ml305_device_ops =
{
    ml305_init,
    ml305_deinit,
    ml305_control,
};

static int ml305_device_class_register(void)
{
    struct at_device_class *class = RT_NULL;

    class = (struct at_device_class *) rt_calloc(1, sizeof(struct at_device_class));
    if (class == RT_NULL)
    {
        LOG_E("no memory for ml305 device class create.");
        return -RT_ENOMEM;
    }

    /* fill ml305 device class object */
#ifdef AT_USING_SOCKET
    ml305_socket_class_register(class);
#endif
    class->device_ops = &ml305_device_ops;

    return at_device_class_register(class, AT_DEVICE_CLASS_ML305);
}
INIT_DEVICE_EXPORT(ml305_device_class_register);

#endif /* AT_DEVICE_USING_ML305 */

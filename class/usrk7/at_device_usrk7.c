/*
 * File      : at_device_usrk7.c
 * This file is part of RT-Thread RTOS
 * COPYRIGHT (C) 2006 - 2020, RT-Thread Development Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Change Logs:
 * Date           Author       Notes
 * 2020-07-26     Wei Fu      first version
 */

/*
 * Note:
 * 回车: <CR> == "\r" == 0x0d，回车的作用只是移动光标至该行的起始位置；
 * 换行: <LF> == "\n" == 0x0a，换行至下一行行首起始位置；
 */

#include <stdio.h>
#include <string.h>

#include "at_device_usrk7.h"

#define LOG_TAG                        "at.dev.usrk7"

#include <at_log.h>

#ifdef AT_DEVICE_USING_USRK7
/**
 * This function replace the 'org' char with 'target' in the 'src' string.
 *
 * @param src: the pointer of the source string.
 * @param maxlen: the maximum length of the source string.
 * @param org: the original char which will be replaced.
 * @param target: the target char will be used in the string.
 *
 * @return void.
 */
void usrk7_str_ncrp(char *src, int maxlen, char org, char target)
{
   char *p = src;
    while (*p != '\0' && p <= (src + maxlen))
    {
         if (*p == org)
        {
            *p = target;
        }
        p++;
    }
    return;
}

/**
 * This function is a AT command wrapper for USR-K7 format:
 * cmd: AT+cmd<CR>
 * resp: <CR><LF>+OK[=resp]<CR><LF>
 *
 * @param device: the pointer of at_device struct.
 * @param cmd: command string
 * @param in: input string after '='. Set to NULL if you don't need it
 * @param out: output string after '+OK='. Set to NULL if you don't need resp.
 *
 * @return RT_EOK or -RT_ERROR/-RT_ENOMEM.
 */
int usrk7_at_cmd(struct at_device *device, char *cmd, char *in, char *out)
{
    char *pos;
    int result = RT_EOK;
    struct at_response *resp = RT_NULL;
    struct at_client *client = device->client;
    char cmd_str[USRK7_CMD_STR_DEFAULT_SIZE] = {0};

    RT_ASSERT(cmd);

    //rt_tick_from_millisecond(3000)
    resp = at_create_resp(USRK7_RESP_DEFAULT_SIZE, USRK7_RESP_DEFAULT_LINE,
                          USRK7_RESP_DEFAULT_TIMEOUT);
    if (!resp)
    {
        LOG_E("%s resp create failed.", cmd);
        return -RT_ENOMEM;
    }

    //set end sign as '<CR>'
    at_obj_set_end_sign(client, '\r');
    if (in)
    {
        rt_sprintf(cmd_str, "AT+%s=%s", cmd, in);
    }
    else
    {
        rt_sprintf(cmd_str, "AT+%s", cmd);
    }

    /* send "AT+cmd" to usrk7 device */
    if (at_obj_exec_cmd(client, resp, cmd_str) < 0)
    {
        result = -RT_ERROR;
//        goto __exit_ent;
    }

    pos = rt_strstr(at_resp_get_line(resp, USRK7_RESP_DEFAULT_LINE_NUM),
                                     USRK7_OK_RESP);
    if (!pos)
    {
        pos = rt_strstr(at_resp_get_line(resp, USRK7_RESP_DEFAULT_LINE_NUM),
                                         "ERR");
        if (pos)
        {
            LOG_E("Set %s failed on %s: %s.", cmd_str, device->name, pos);
            if (out)
            {
                rt_strncpy(out, pos, rt_strlen(pos));
            }
        }
        else
        {
            LOG_E("Set %s failed on %s: no response!", cmd_str, device->name);
        }
        result = -RT_ERROR;
    }
    else
    {
        if (out)
        {
            rt_strncpy(out, pos + 4, rt_strlen(pos) - 4);
        }
    }
    LOG_D("%s resp %s in %s",cmd_str ,pos, __func__);

__exit_ent:
    at_delete_resp(resp);
    return result;
}

/* enter usrk7 command mode */
int usrk7_cmdmode(struct at_device *device)
{
    int ret;
    struct at_client *client = device->client;

    rt_mutex_take(client->lock, RT_WAITING_FOREVER);

    // send +++ (without \t\r) to usrk7
    if (!at_client_obj_send(device->client,
                           USRK7_CMDMODE_TRIGGER,
                           USRK7_CMDMODE_TRIGGER_SIZE))
    {
        LOG_E("%s device send cmdmode trigger fail.", device->name);
        ret = -RT_ERROR;
        goto __exit_cmdmode;
    }

    // skipping to get response "a" (without \t\r) from usrk7
    rt_thread_mdelay(500);

    // send back 'response "a"' (without \t\r) to usrk7
    if (!at_client_obj_send(device->client,
                            USRK7_CMDMODE_ACK,
                            USRK7_CMDMODE_ACK_SIZE))
    {
        LOG_E("%s device send cmdmode ACK fail.", device->name);
        ret = -RT_ERROR;
        goto __exit_cmdmode;
    }

    // skipping to check the 'response +ok' (without \t\r) from usrk7

    //client->resp = RT_NULL;
__exit_cmdmode:

    rt_mutex_release(client->lock);
    return ret;
}

/* enter usrk7 transmission mode */
int usrk7_entmode(struct at_device *device)
{
    return usrk7_at_cmd(device, "ENTM", RT_NULL, RT_NULL);
}

/* reset usrk7 device and initialize device network again */
static int usrk7_reset(struct at_device *device)
{
    /* send "AT+Z" commonds to usrk7 device(ignore the feedback) */
    usrk7_at_cmd(device, "Z", RT_NULL, RT_NULL);
    device->is_init = RT_FALSE;
    /* waiting 2 second(you MUST wait for more than 2s) for usrk7 reset */
    rt_thread_mdelay(USRK7_RESET_MDELAY);
    usrk7_cmdmode(device);
    device->is_init = RT_TRUE;
    return RT_EOK;
}

/* change usrk7 firmware version information */
static int usrk7_get_version(struct at_device *device, char *version)
{
    RT_ASSERT(version);
    return usrk7_at_cmd(device, "VER", RT_NULL, version);
}

static int usrk7_get_mid(struct at_device *device, char *id)
{
    RT_ASSERT(id);
    return usrk7_at_cmd(device, "MID", RT_NULL, id);
}
/**
 * This function query the socket info.
 *
 * @param device: taget device.
 * @param interface: interface of the socket, actually is the AT command, like
 *                   SOCKA1, SOCKB1, and etc.
 * @param protocol: TCPC, TCPS, UDPC, UDPS.
 * @param ip: target ip of server
 * @param port: port number.
 *
 * @return void.
 */
void usrk7_at_sock_get(struct at_device *device, char *interface,
                              char *protocol, char *ip, int *port, char *status)
{
    char tmp_str[USRK7_CMD_STR_DEFAULT_SIZE] = {0};

    RT_ASSERT(interface && protocol && ip && port && status);

    if (usrk7_at_cmd(device, interface, RT_NULL, tmp_str))
    {
        LOG_E("%s device parse \"AT+%s\" cmd error.", device->name, interface);
        return;
    }

    // from "%s,%s,%d" to "%s %s %d" for sscanf
    usrk7_str_ncrp(tmp_str, USRK7_CMD_STR_DEFAULT_SIZE, ',', ' ');
    sscanf(tmp_str, "%s %s %d", protocol, ip, port);

    if (rt_strcmp(interface, "SOCKB1"))
    {
        usrk7_at_cmd(device, "SOCKLKA1", RT_NULL, status);
    }
    else
    {
        usrk7_at_cmd(device, "SOCKLKB1", RT_NULL, status);
    }

    return;
}

int usrk7_at_sock_set(struct at_device *device, char *interface,
                             char *protocol, char *ip, int *port)
{
    int ret;
    char tmp_str[USRK7_CMD_STR_DEFAULT_SIZE] = {0};

    RT_ASSERT(interface && protocol && ip && port);

    rt_sprintf(tmp_str, "%s,%s,%d", protocol, ip, *port);

    ret = usrk7_at_cmd(device, interface, tmp_str, RT_NULL);
    if (ret)
    {
        LOG_E("%s device parse \"AT+%s\" cmd error.", device->name, interface);
    }
    else
    {
        usrk7_reset(device);
    }

    return ret;
}

static void usrk7_at_mac_get(struct at_device *device, char *mac)
{
    RT_ASSERT(mac);
    usrk7_at_cmd(device, "MAC", RT_NULL, mac);
    return;
}

void usrk7_at_wann_get(struct at_device *device, char *mode, char *ip,
                              char *netmask, char *gateway)
{
    char tmp_str[USRK7_CMD_STR_DEFAULT_SIZE] = {0};

    RT_ASSERT(mode && ip && netmask && gateway);

    if (usrk7_at_cmd(device, "WANN", RT_NULL, tmp_str))
    {
        LOG_E("%s device parse \"AT+WANN\" cmd error.", device->name);
        return;
    }

    // from "%s,%s,%s,%s" to "%s %s %s %s" for sscanf
    usrk7_str_ncrp(tmp_str, USRK7_CMD_STR_DEFAULT_SIZE, ',', ' ');
    sscanf(tmp_str, "%s %s %s %s", mode, ip, netmask, gateway);

    return;
}

static int usrk7_at_wann_set(struct at_device *device, char *mode, char *ip,
                              char *netmask, char *gateway)
{
    int ret;
    char tmp_str[USRK7_CMD_STR_DEFAULT_SIZE] = {0};

    RT_ASSERT(mode && ip && netmask && gateway);

    rt_sprintf(tmp_str, "%s,%s,%s,%s", mode, ip, netmask, gateway);

    ret = usrk7_at_cmd(device, "WANN", tmp_str, RT_NULL);
    if (ret)
    {
        LOG_E("%s device parse \"AT+WANN\" cmd error.", device->name);
    }
    else
    {
        usrk7_reset(device);
    }

    return ret;
}

static void usrk7_at_dns_get(struct at_device *device, char *dns)
{
    RT_ASSERT(dns);
    usrk7_at_cmd(device, "DNS", RT_NULL, dns);
    return;
}

static int usrk7_at_dns_set(struct at_device *device, char *dns)
{
    int ret;

    RT_ASSERT(dns);
    ret = usrk7_at_cmd(device, "DNS", dns, RT_NULL);
    if (ret)
    {
        LOG_E("%s device parse \"AT+DNS\" cmd error.", device->name);
    }
    else
    {
        usrk7_reset(device);
    }

    return ret;
}
/* ==================  usrk7 network interface operations ================== */

static int usrk7_netdev_set_dns_server(struct netdev *netdev, uint8_t dns_num,
                                       ip_addr_t *dns_server);

static void usrk7_get_netdev_info(struct rt_work *work, void *work_data)
{

    ip_addr_t ip_addr;
    rt_uint32_t num = 0;
    rt_uint32_t dhcp_stat = 0;
    rt_uint32_t mac_addr[MAC_ADDR_LEN] = {0};

    char ip[AT_ADDR_STR_MAXLEN] = {0};
    char mac[AT_ADDR_STR_MAXLEN] = {0};
    char mode[AT_ADDR_STR_MAXLEN] = {0};
    char gateway[AT_ADDR_STR_MAXLEN] = {0};
    char netmask[AT_ADDR_STR_MAXLEN] = {0};
    char dns_server[AT_ADDR_STR_MAXLEN] = {0};

    struct rt_delayed_work *delay_work = (struct rt_delayed_work *)work;
    struct at_device *device = (struct at_device *)work_data;
    struct netdev *netdev = device->netdev;
    struct at_client *client = device->client;
    struct at_device_usrk7 *usrk7 = (struct at_device_usrk7 *)device->user_data;

    if (delay_work)
    {
        rt_free(delay_work);
    }

    usrk7_get_mid(device, usrk7->id);
    usrk7_get_version(device, usrk7->version);

    usrk7_at_mac_get(device, mac);
    usrk7_at_dns_get(device, dns_server);
    usrk7_at_wann_get(device, mode, ip, netmask, gateway);

    LOG_D("=== %s ===", __func__);
    LOG_D("ID:         %s", usrk7->id);
    LOG_D("Version:    %s", usrk7->version);
    LOG_D("MAC:        %s", mac);
    LOG_D("mode:       %s", mode);
    LOG_D("ip:         %s", ip);
    LOG_D("netmask:    %s", netmask);
    LOG_D("gateway:    %s", gateway);
    LOG_D("dns_server: %s", dns_server);
    LOG_D("===============================");

    /* set netdev info */
    inet_aton(gateway, &ip_addr);
    netdev_low_level_set_gw(netdev, &ip_addr);
    inet_aton(netmask, &ip_addr);
    netdev_low_level_set_netmask(netdev, &ip_addr);
    inet_aton(ip, &ip_addr);
    netdev_low_level_set_ipaddr(netdev, &ip_addr);

    sscanf(mac, "%02x%02x%02x%02x%02x%02x",
            &mac_addr[0], &mac_addr[1], &mac_addr[2],
            &mac_addr[3], &mac_addr[4], &mac_addr[5]);
    for (num = 0; num < netdev->hwaddr_len; num++)
    {
        netdev->hwaddr[num] = mac_addr[num];
    }

    /* set primary DNS server address */
    if (rt_strlen(dns_server) > 0 &&
            rt_strncmp(dns_server, USRK7_ERR_DNS_SERVER,
                       rt_strlen(USRK7_ERR_DNS_SERVER)) != 0)
    {
        inet_aton(dns_server, &ip_addr);
        netdev_low_level_set_dns_server(netdev, 0, &ip_addr);
    }
    else
    {
        inet_aton(USRK7_DEF_DNS_SERVER, &ip_addr);
        usrk7_netdev_set_dns_server(netdev, 0, &ip_addr);
    }
    if (rt_strlen(mode) > 0)
    {
        dhcp_stat = rt_strncmp(mode, "DHCP", rt_strlen("DHCP"));
        /* 0 - DHCP, 1 - static ip */
        netdev_low_level_set_dhcp_status(netdev,
                                         dhcp_stat ? RT_FALSE : RT_TRUE);
    }
    else
    {
        LOG_E("%s device dhcp status error.", device->name);
    }
__exit:
    return;
}

static int usrk7_net_init(struct at_device *device);

static int usrk7_netdev_set_up(struct netdev *netdev)
{
    struct at_device *device = RT_NULL;
    device = at_device_get_by_name(AT_DEVICE_NAMETYPE_NETDEV, netdev->name);
    if (!device)
    {
        LOG_E("get device(%s) failed.", netdev->name);
        return -RT_ERROR;
    }

    if (device->is_init == RT_FALSE)
    {
        usrk7_net_init(device);
        netdev_low_level_set_status(netdev, RT_TRUE);
        LOG_D("network interface device(%s) set up status", netdev->name);
    }

    return RT_EOK;
}

static int usrk7_netdev_set_down(struct netdev *netdev)
{
    struct at_device *device = RT_NULL;

    device = at_device_get_by_name(AT_DEVICE_NAMETYPE_NETDEV, netdev->name);
    if (!device)
    {
        LOG_E("get device by netdev(%s) failed.", netdev->name);
        return -RT_ERROR;
    }

    if (device->is_init == RT_TRUE)
    {
        device->is_init = RT_FALSE;
        netdev_low_level_set_status(netdev, RT_FALSE);
        LOG_D("network interface device(%s) set down status", netdev->name);
    }

    return RT_EOK;
}

static int usrk7_netdev_set_addr_info(struct netdev *netdev,
                                      ip_addr_t *ip_addr,
                                      ip_addr_t *netmask,
                                      ip_addr_t *gw)
{
    int result = RT_EOK;
    struct at_device *device = RT_NULL;

    char ip_str[AT_ADDR_STR_MAXLEN] = {0};
    char gw_str[AT_ADDR_STR_MAXLEN] = {0};
    char dhcp_str[AT_ADDR_STR_MAXLEN] = {0};
    char netmask_str[AT_ADDR_STR_MAXLEN] = {0};

    RT_ASSERT(netdev);
    RT_ASSERT(ip_addr || netmask || gw);

    device = at_device_get_by_name(AT_DEVICE_NAMETYPE_NETDEV, netdev->name);
    if (!device)
    {
        LOG_E("get device(%s) failed.", netdev->name);
        return -RT_ERROR;
    }
    usrk7_at_wann_get(device, dhcp_str, ip_str, netmask_str, gw_str);

    /* Convert numeric IP address into decimal dotted ASCII representation. */
    if (ip_addr)
        rt_memcpy(ip_str, inet_ntoa(*ip_addr), AT_ADDR_STR_MAXLEN);
    else
        rt_memcpy(ip_str, inet_ntoa(netdev->ip_addr), AT_ADDR_STR_MAXLEN);

    if (gw)
        rt_memcpy(gw_str, inet_ntoa(*gw), AT_ADDR_STR_MAXLEN);
    else
        rt_memcpy(gw_str, inet_ntoa(netdev->gw), AT_ADDR_STR_MAXLEN);

    if (netmask)
        rt_memcpy(netmask_str, inet_ntoa(*netmask), AT_ADDR_STR_MAXLEN);
    else
        rt_memcpy(netmask_str, inet_ntoa(netdev->netmask), AT_ADDR_STR_MAXLEN);

    /* if the original setting is DHCP, then print a warnning */
    if (!rt_strncmp(dhcp_str, "DHCP", rt_strlen("DHCP"))) {
        LOG_W("Setting %s device network from DHCP to STATIC.", device->name);
    }

    /*
     * send addr info set commond "AT+WANN=STATIC,ip,gateway,netmask"
     * The mode will be set to static by using this api.
     */
    LOG_D("set AT+WANN=%s,%s,%s,%s in %s",
          dhcp_str, ip_str, netmask_str, gw_str, __func__);

    if (usrk7_at_wann_set(device, "STATIC", ip_str, netmask_str, gw_str) < 0)
    {
        LOG_E("%s device set address failed.", device->name);
        result = -RT_ERROR;
    }
    else
    {
        /* Update netdev information */
        if (ip_addr)
            netdev_low_level_set_ipaddr(netdev, ip_addr);

        if (gw)
            netdev_low_level_set_gw(netdev, gw);

        if (netmask)
            netdev_low_level_set_netmask(netdev, netmask);
        LOG_D("%s device set address success.", device->name);
    }
    return result;
}

static int usrk7_netdev_set_dns_server(struct netdev *netdev,
                                       uint8_t dns_num,
                                       ip_addr_t *dns_server)
{
    int result = RT_EOK;
    struct at_device *device = RT_NULL;
    char dns_str[AT_ADDR_STR_MAXLEN] = {0};

    RT_ASSERT(netdev);
    RT_ASSERT(dns_server);

    device = at_device_get_by_name(AT_DEVICE_NAMETYPE_NETDEV, netdev->name);
    if (!device)
    {
        LOG_E("get device by netdev(%s) failed.", netdev->name);
        return -RT_ERROR;
    }

    if (dns_server)
        rt_memcpy(dns_str, inet_ntoa(*dns_server), AT_ADDR_STR_MAXLEN);
    else
        rt_memcpy(dns_str, inet_ntoa(netdev->dns_servers), AT_ADDR_STR_MAXLEN);


    /* send addr info set commond "AT+DNS=dnsserver" and wait response */
    if (usrk7_at_dns_set(device, dns_str) < 0)
    {
        LOG_E("%s device set DNS failed.", device->name);
        result = -RT_ERROR;
    }
    else
    {
        netdev_low_level_set_dns_server(netdev, dns_num, dns_server);
        LOG_D("%s device set DNS(%s) success.",
              device->name, inet_ntoa(*dns_server));
    }
    usrk7_reset(device);
    return result;
}

static int usrk7_netdev_set_dhcp(struct netdev *netdev, rt_bool_t is_enabled)
{
    ip_addr_t ip_addr;
    int result = RT_EOK;
    struct at_device *device;

    char ip_str[AT_ADDR_STR_MAXLEN] = {0};
    char gw_str[AT_ADDR_STR_MAXLEN] = {0};
    char dhcp_str[AT_ADDR_STR_MAXLEN] = {0};
    char netmask_str[AT_ADDR_STR_MAXLEN] = {0};

    RT_ASSERT(netdev);

    device = at_device_get_by_name(AT_DEVICE_NAMETYPE_NETDEV, netdev->name);
    if (!device)
    {
        LOG_E("get device by netdev(%s) failed.", netdev->name);
        return -RT_ERROR;
    }

    usrk7_at_wann_get(device, dhcp_str, ip_str, netmask_str, gw_str);

    if (is_enabled)
    {
        LOG_I("Setting %s device network to DHCP.", device->name);
        if (usrk7_at_wann_set(device, "DHCP", ip_str, netmask_str, gw_str) < 0)
        {
            result = -RT_ERROR;
        }
    }
    else
    {
        LOG_I("Setting %s device network to STATIC.", device->name);
        if (usrk7_at_wann_set(device, "STATIC", ip_str, netmask_str, gw_str) < 0)
        {
            result = -RT_ERROR;
        }
    }

    /* send addr info set commond "AT+WANN=mode,ip,gateway,netmask", wait ret*/
    if ( result < 0)
    {
        LOG_E("%s device set DHCP status(%d) failed.",
              device->name, is_enabled);
        goto __exit;
    }
    else
    {
        netdev_low_level_set_dhcp_status(netdev, is_enabled);
        LOG_D("%s device set DHCP status(%d) ok.", device->name, is_enabled);
    }
    usrk7_reset(device);

    usrk7_at_wann_get(device, dhcp_str, ip_str, netmask_str, gw_str);
    LOG_D("=== %s ===", __func__);
    LOG_D("mode:       %s", dhcp_str);
    LOG_D("ip:         %s", ip_str);
    LOG_D("netmask:    %s", netmask_str);
    LOG_D("gateway:    %s", gw_str);
    LOG_D("===============================");

    /* set netdev info */
    inet_aton(gw_str, &ip_addr);
    netdev_low_level_set_gw(netdev, &ip_addr);
    inet_aton(netmask_str, &ip_addr);
    netdev_low_level_set_netmask(netdev, &ip_addr);
    inet_aton(ip_str, &ip_addr);
    netdev_low_level_set_ipaddr(netdev, &ip_addr);
__exit:
    return result;
}

#ifdef NETDEV_USING_PING
static int usrk7_netdev_ping(struct netdev *netdev, const char *host,
                             size_t data_len, uint32_t timeout,
                             struct netdev_ping_resp *ping_resp)
{
    char *pos;
    int ret = RT_EOK;
    struct at_device *device;
    struct at_client *client;
    struct at_response *resp = RT_NULL;
    //char ping_result[AT_ADDR_STR_MAXLEN] = {0};

    RT_ASSERT(netdev && host && ping_resp);

    device = at_device_get_by_name(AT_DEVICE_NAMETYPE_NETDEV, netdev->name);
    if (!device)
    {
        LOG_E("get device(%s) failed.", netdev->name);
        return -RT_ERROR;
    }

    /*
     * resp parameters are different from other commands,
     * so don't use usrk7_at_cmd
     */
    //ret = usrk7_at_cmd(device, "PING1", (char *) host, ping_result);

    client = device->client;
    resp = at_create_resp(USRK7_RESP_DEFAULT_SIZE, 1,
                          USRK7_RESP_DEFAULT_TIMEOUT);
    if (!resp)
    {
        LOG_E("AT+PING1 resp create failed.");
        return -RT_ENOMEM;
    }
    //set end sign as '<CR>'
    at_obj_set_end_sign(client, '\r');
    /* send "AT+PING1" to usrk7 device */
    if (at_obj_exec_cmd(client, resp, "AT+PING1=%s", host) < 0)
    {
        ret = -RT_ERROR;
        goto __exit_ping;
    }

    pos = rt_strstr(at_resp_get_line(resp, 1), USRK7_OK_RESP);
    if (!pos)
    {
        pos = rt_strstr(at_resp_get_line(resp, 1), "ERR");
        if (pos)
        {
            LOG_E("AT+PING1 failed on %s: %s.", device->name, pos);
        }
        else
        {
            LOG_E("AT+PING1 failed on %s: no response!", device->name);
        }
        ret = -RT_ERROR;
        goto __exit_ping;
    }
    LOG_D("AT+PING1=%s resp %s in %s", host, pos, __func__);
    /*
     * can NOT get IP from domain name in USR-K7,
     * if host is NOT IP addr, we just set IP as 255.255.255.255
     */
    if (!inet_aton(host, &(ping_resp->ip_addr)))
    {
        ping_resp->ip_addr.addr = IPADDR_NONE;
    }
    ping_resp->data_len = data_len;
    //can NOT get this info from USR-K7
    ping_resp->ttl = 0;
    ping_resp->ticks = 0;

__exit_ping:
    at_delete_resp(resp);
    return ret;
}
#endif /* NETDEV_USING_PING */

#ifdef NETDEV_USING_NETSTAT
void usrk7_netdev_netstat(struct netdev *netdev)
{
    int port;
    ip_addr_t ip_addr;
    rt_uint32_t dhcp_stat;
    struct at_device *device;

    char ip_str[AT_ADDR_STR_MAXLEN] = {0};
    char gw_str[AT_ADDR_STR_MAXLEN] = {0};
    char dhcp_str[AT_ADDR_STR_MAXLEN] = {0};
    char protocol[AT_ADDR_STR_MAXLEN] = {0};
    char netmask_str[AT_ADDR_STR_MAXLEN] = {0};
    char socket_ip_str[AT_ADDR_STR_MAXLEN] = {0};
    char socket_status[AT_ADDR_STR_MAXLEN] = {0};

    RT_ASSERT(netdev);

    device = at_device_get_by_name(AT_DEVICE_NAMETYPE_NETDEV, netdev->name);
    if (!device)
    {
        LOG_E("get device(%s) failed.", netdev->name);
        return;
    }

    usrk7_at_wann_get(device, dhcp_str, ip_str, netmask_str, gw_str);
    LOG_I("=== %s ===", __func__);
    LOG_I("mode:       %s", dhcp_str);
    LOG_I("ip:         %s", ip_str);
    LOG_I("netmask:    %s", netmask_str);
    LOG_I("gateway:    %s", gw_str);

    usrk7_at_sock_get(device, "SOCKA1",
                      protocol, socket_ip_str, &port, socket_status);
    LOG_I("sockA1:     %s %s %d status: %s",
                      protocol, socket_ip_str, port, socket_status);

    rt_memset(protocol,'\0', AT_ADDR_STR_MAXLEN);
    rt_memset(socket_ip_str,'\0', AT_ADDR_STR_MAXLEN);
    rt_memset(socket_status,'\0', AT_ADDR_STR_MAXLEN);
    usrk7_at_sock_get(device, "SOCKB1",
                      protocol, socket_ip_str, &port, socket_status);
    LOG_I("sockB1:     %s %s %d status: %s",
                      protocol, socket_ip_str, port, socket_status);

    LOG_I("===============================");

    /* set netdev info */
    inet_aton(gw_str, &ip_addr);
    netdev_low_level_set_gw(netdev, &ip_addr);
    inet_aton(netmask_str, &ip_addr);
    netdev_low_level_set_netmask(netdev, &ip_addr);
    inet_aton(ip_str, &ip_addr);
    netdev_low_level_set_ipaddr(netdev, &ip_addr);
    dhcp_stat = rt_strncmp(dhcp_str, "DHCP", rt_strlen("DHCP"));
    /* 0 - DHCP, 1 - static ip */
    netdev_low_level_set_dhcp_status(netdev,
                                     dhcp_stat ? RT_FALSE : RT_TRUE);
    return;
}
#endif /* NETDEV_USING_NETSTAT */

static const struct netdev_ops usrk7_netdev_ops =
{
    usrk7_netdev_set_up,
    usrk7_netdev_set_down,
    usrk7_netdev_set_addr_info,
    usrk7_netdev_set_dns_server,
    usrk7_netdev_set_dhcp,
#ifdef NETDEV_USING_PING
    usrk7_netdev_ping,
#endif
#ifdef NETDEV_USING_NETSTAT
    usrk7_netdev_netstat,
#endif
};

static struct netdev *usrk7_netdev_add(const char *netdev_name)
{
    struct netdev *netdev = RT_NULL;

    RT_ASSERT(netdev_name);

    netdev = (struct netdev *) rt_calloc(1, sizeof(struct netdev));
    if (!netdev)
    {
        LOG_E("no memory for netdev create.");
        return RT_NULL;
    }

    netdev->ops = &usrk7_netdev_ops;
    netdev->mtu = USRK7_ETHERNET_MTU;
    netdev->hwaddr_len = MAC_ADDR_LEN;

 #ifdef SAL_USING_AT
     extern int sal_at_netdev_set_pf_info(struct netdev *netdev);
     /* set the network interface socket/netdb operations */
     sal_at_netdev_set_pf_info(netdev);
 #endif

    netdev_register(netdev, netdev_name, RT_NULL);

    return netdev;
}

/* ========================  usrk7 device operations ======================== */
static void usrk7_netdev_start_delay_work(struct at_device *device)
{
    struct rt_delayed_work *net_work =
        (struct rt_delayed_work *)rt_calloc(1, sizeof(struct rt_delayed_work));
    if (net_work)
    {
        rt_delayed_work_init(net_work, usrk7_get_netdev_info, (void *)device);
        rt_work_submit(&(net_work->work), RT_TICK_PER_SECOND);
    }
    return;
}

static void usrk7_init_thread_entry(void *parameter)
{
    int ret;
    rt_size_t retry_num = USRK7_INIT_RETRY;

    struct at_device *device = (struct at_device *) parameter;
    struct at_client *client = device->client;
    struct at_device_usrk7 *usrk7 = (struct at_device_usrk7 *)device->user_data;

    LOG_D("%s device initialize start.", device->name);
    while (retry_num--)
    {
        /* reset module */
        ret = usrk7_reset(device);
        if (!ret)
        {
            break;
        }

        /* initialize successfully  */
        rt_thread_mdelay(USRK7_DEFAULT_MDELAY);
        LOG_I("%s device initialize retry...", device->name);
    }

    if (ret)
    {
        netdev_low_level_set_status(device->netdev, RT_FALSE);
        LOG_E("%s device network initialize failed(%d).", device->name, ret);
        return;
    }

    device->is_init = RT_TRUE;
    netdev_low_level_set_status(device->netdev, RT_TRUE);
    netdev_low_level_set_link_status(device->netdev, RT_TRUE);
    usrk7_netdev_start_delay_work(device);
    LOG_I("%s device network initialize successfully.", device->name);
    return;
}

static int usrk7_net_init(struct at_device *device)
{
#ifdef AT_DEVICE_USRK7_INIT_ASYN
    rt_thread_t tid;
    tid = rt_thread_create("usrk7_net",
                           usrk7_init_thread_entry, (void *) device,
                           USRK7_THREAD_STACK_SIZE, USRK7_THREAD_PRIORITY, 20);
    if (tid)
    {
        rt_thread_startup(tid);
    }
    else
    {
        LOG_E("create %s device init thread failed.", device->name);
        return -RT_ERROR;
    }
#else
    usrk7_init_thread_entry(device);
#endif /* AT_DEVICE_USRK7_INIT_ASYN */

    return RT_EOK;
}

static int usrk7_init(struct at_device *device)
{
    int ret;
    struct at_device_usrk7 *usrk7 = (struct at_device_usrk7 *)device->user_data;

    /* initialize AT client */
    LOG_I("AT client(%s) init...", usrk7->client_name);
    ret = at_client_init(usrk7->client_name, usrk7->recv_line_num);
    if (ret)
    {
        LOG_E("AT client(%s) init failed. %d", usrk7->client_name, ret);
        return -RT_ERROR;
    }

    device->client = at_client_get(usrk7->client_name);
    if (!device->client)
    {
        LOG_E("get AT client(%s) failed.", usrk7->client_name);
        return -RT_ERROR;
    }

 #ifdef AT_USING_SOCKET
     usrk7_socket_init(device);
 #endif

    /* add usrk7 device to the netdev list */
    device->netdev = usrk7_netdev_add(usrk7->device_name);
    if (!device->netdev)
    {
        LOG_E("add netdev(%s) failed.", usrk7->device_name);
        return -RT_ERROR;
    }

    /* initialize usrk7 device network */
    return usrk7_netdev_set_up(device->netdev);
}

static int usrk7_deinit(struct at_device *device)
{
    return usrk7_netdev_set_down(device->netdev);
}

static int usrk7_control(struct at_device *device, int cmd, void *arg)
{
    int result = -RT_ERROR;

    RT_ASSERT(device);

    switch (cmd)
    {
    case AT_DEVICE_CTRL_POWER_ON:
    case AT_DEVICE_CTRL_POWER_OFF:
    case AT_DEVICE_CTRL_LOW_POWER:
    case AT_DEVICE_CTRL_SLEEP:
    case AT_DEVICE_CTRL_WAKEUP:
    case AT_DEVICE_CTRL_SET_WIFI_INFO:
    case AT_DEVICE_CTRL_NET_CONN:
    case AT_DEVICE_CTRL_NET_DISCONN:
    case AT_DEVICE_CTRL_GET_SIGNAL:
    case AT_DEVICE_CTRL_GET_GPS:
        LOG_W("not support the control cmd(%d).", cmd);
        break;
    case AT_DEVICE_CTRL_RESET:
        result = usrk7_reset(device);
        break;
    case AT_DEVICE_CTRL_GET_VER:
        result = usrk7_get_version(device, (char *) arg);
        break;
    default:
        LOG_E("input error control cmd(%d).", cmd);
        break;
    }

    return result;
}

static const struct at_device_ops usrk7_device_ops =
{
    usrk7_init,
    usrk7_deinit,
    usrk7_control,
};

static int usrk7_device_class_register(void)
{
    struct at_device_class *class =
        (struct at_device_class *) rt_calloc(1, sizeof(struct at_device_class));
    if (!class)
    {
        LOG_E("no memory for class create.");
        return -RT_ENOMEM;
    }

    /* fill USRK7 device class object */
 #ifdef AT_USING_SOCKET
     usrk7_socket_class_register(class);
 #endif
    class->device_ops = &usrk7_device_ops;

    return at_device_class_register(class, AT_DEVICE_CLASS_USRK7);
}
INIT_DEVICE_EXPORT(usrk7_device_class_register);

#endif /* AT_DEVICE_USING_USRK7 */

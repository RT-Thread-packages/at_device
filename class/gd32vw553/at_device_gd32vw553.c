/*
 * Copyright (c) 2006-2026, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-09-22     CYFS         add GD32VW553 Wi-Fi device support
 */

#include <stdio.h>
#include <string.h>

#include <at_device_gd32vw553.h>
#include <drivers/dev_pin.h>
#include <drivers/dev_serial.h>

#define LOG_TAG                         "at.dev.gd32vw553"
#include <at_log.h>

#if defined(AT_DEVICE_USING_GD32VW553)

#define GD32VW553_WAIT_CONNECT_TIME     8000
#define GD32VW553_THREAD_STACK_SIZE     2048
#define GD32VW553_THREAD_PRIORITY       (RT_THREAD_PRIORITY_MAX / 2)
#define GD32VW553_ADDR_LEN              32
#define GD32VW553_UART_RX_BUFSZ         1024
#define GD32VW553_RESET_PULSE_MS        20
#define GD32VW553_RESET_BOOT_MS         500
#define GD32VW553_NET_WAIT_MS           10000
#define GD32VW553_NET_POLL_MS           500

static int gd32vw553_net_init(struct at_device *device);

static int gd32vw553_hardware_reset(struct at_device_gd32vw553 *gd32vw553)
{
    rt_uint8_t inactive_level;

    if (gd32vw553->use_reset_pin == RT_FALSE || gd32vw553->reset_pin < 0)
    {
        return -RT_ENOSYS;
    }

    inactive_level = gd32vw553->reset_active_level == PIN_LOW ? PIN_HIGH : PIN_LOW;
    rt_pin_write(gd32vw553->reset_pin, inactive_level);
    rt_pin_mode(gd32vw553->reset_pin, PIN_MODE_OUTPUT);
    rt_thread_mdelay(1);
    rt_pin_write(gd32vw553->reset_pin, gd32vw553->reset_active_level);
    rt_thread_mdelay(GD32VW553_RESET_PULSE_MS);
    rt_pin_write(gd32vw553->reset_pin, inactive_level);
    rt_thread_mdelay(GD32VW553_RESET_BOOT_MS);

    return RT_EOK;
}

static int gd32vw553_configure_at(struct at_device *device)
{
    at_response_t resp;
    int result;

    resp = at_create_resp(128, 0, 5 * RT_TICK_PER_SECOND);
    if (resp == RT_NULL)
    {
        return -RT_ENOMEM;
    }

    result = at_obj_exec_cmd(device->client, resp, "AT+CWMODE_CUR=1");
    if (result == RT_EOK)
    {
        result = at_obj_exec_cmd(device->client, resp, "AT+CIPMODE=0");
    }
    if (result == RT_EOK)
    {
        result = at_obj_exec_cmd(device->client, resp, "AT+CIPMUX=0");
    }

    at_delete_resp(resp);
    return result;
}

static void gd32vw553_copy_value(char *dst, size_t dst_size, const char *src)
{
    const char *start;
    const char *end;
    size_t length;

    if (dst == RT_NULL || dst_size == 0)
    {
        return;
    }

    dst[0] = '\0';
    if (src == RT_NULL)
    {
        return;
    }

    start = src;
    while (*start == ' ' || *start == ':' || *start == ',' || *start == '<')
    {
        start++;
    }
    if (*start == '"')
    {
        start++;
    }

    end = start;
    while (*end != '\0' && *end != '"' && *end != '>' &&
           *end != ',' && *end != '\r' && *end != '\n')
    {
        end++;
    }

    length = (size_t)(end - start);
    while (length > 0 && start[length - 1] == ' ')
    {
        length--;
    }
    if (length >= dst_size)
    {
        length = dst_size - 1;
    }

    rt_memcpy(dst, start, length);
    dst[length] = '\0';
}

static rt_bool_t gd32vw553_inet_aton(const char *text, ip_addr_t *address)
{
    if (text == RT_NULL || address == RT_NULL || inet_aton(text, address) == 0)
    {
        return RT_FALSE;
    }

    return address->addr != 0 ? RT_TRUE : RT_FALSE;
}

static rt_bool_t gd32vw553_parse_value_by_key(at_response_t resp, const char *key,
                                               char *value, size_t value_size)
{
    rt_size_t line_number;

    for (line_number = 1; line_number <= resp->line_counts; line_number++)
    {
        const char *line = at_resp_get_line(resp, line_number);
        const char *position;

        if (line == RT_NULL || (position = rt_strstr(line, key)) == RT_NULL)
        {
            continue;
        }

        position += rt_strlen(key);
        gd32vw553_copy_value(value, value_size, position);
        if (value[0] != '\0')
        {
            return RT_TRUE;
        }
    }

    return RT_FALSE;
}

static rt_bool_t gd32vw553_parse_value_by_line(at_response_t resp, int line_number,
                                                char *value, size_t value_size)
{
    const char *line = at_resp_get_line(resp, line_number);
    const char *position;

    if (line == RT_NULL)
    {
        return RT_FALSE;
    }

    position = strchr(line, ':');
    gd32vw553_copy_value(value, value_size, position ? position + 1 : line);
    return value[0] != '\0' ? RT_TRUE : RT_FALSE;
}

static int gd32vw553_join_ap(struct at_device *device, const char *ssid, const char *password)
{
    at_response_t resp;
    int result;

    if (ssid == RT_NULL || password == RT_NULL || ssid[0] == '\0')
    {
        return RT_EOK;
    }

    resp = at_create_resp(256, 0, 20 * RT_TICK_PER_SECOND);
    if (resp == RT_NULL)
    {
        return -RT_ENOMEM;
    }

    result = at_obj_exec_cmd(device->client, resp,
                             "AT+CWJAP_CUR=\"%s\",\"%s\"", ssid, password);
    if (result != RT_EOK)
    {
        result = at_obj_exec_cmd(device->client, resp,
                                 "AT+CWJAP=\"%s\",\"%s\"", ssid, password);
    }
    if (result != RT_EOK)
    {
        LOG_W("%s device Wi-Fi connect failed.", device->name);
    }

    at_delete_resp(resp);
    return result;
}

static rt_bool_t gd32vw553_sync_netdev_info(struct at_device *device)
{
    at_response_t resp;
    struct netdev *netdev = device->netdev;
    char ip[GD32VW553_ADDR_LEN] = {0};
    char gateway[GD32VW553_ADDR_LEN] = {0};
    char netmask[GD32VW553_ADDR_LEN] = {0};
    char mac[GD32VW553_ADDR_LEN] = {0};
    ip_addr_t address;
    rt_uint32_t mac_addr[6] = {0};
    rt_bool_t link_up = RT_FALSE;
    int index;

    resp = at_create_resp(512, 0, 5 * RT_TICK_PER_SECOND);
    if (resp == RT_NULL)
    {
        return RT_FALSE;
    }

    if (at_obj_exec_cmd(device->client, resp, "AT+CIPSTA?") == RT_EOK)
    {
        if ((gd32vw553_parse_value_by_key(resp, "ip", ip, sizeof(ip)) ||
             gd32vw553_parse_value_by_line(resp, 1, ip, sizeof(ip))) &&
            gd32vw553_inet_aton(ip, &address))
        {
            netdev_low_level_set_ipaddr(netdev, &address);
            netdev_low_level_set_dhcp_status(netdev, RT_TRUE);
            link_up = RT_TRUE;
        }

        if ((gd32vw553_parse_value_by_key(resp, "gateway", gateway, sizeof(gateway)) ||
             gd32vw553_parse_value_by_line(resp, 3, gateway, sizeof(gateway))) &&
            gd32vw553_inet_aton(gateway, &address))
        {
            netdev_low_level_set_gw(netdev, &address);
        }

        if ((gd32vw553_parse_value_by_key(resp, "netmask", netmask, sizeof(netmask)) ||
             gd32vw553_parse_value_by_line(resp, 2, netmask, sizeof(netmask))) &&
            gd32vw553_inet_aton(netmask, &address))
        {
            netdev_low_level_set_netmask(netdev, &address);
        }
    }

    if (at_obj_exec_cmd(device->client, resp, "AT+CIFSR") == RT_EOK)
    {
        if (gd32vw553_parse_value_by_key(resp, "STAIP", ip, sizeof(ip)) &&
            gd32vw553_inet_aton(ip, &address))
        {
            netdev_low_level_set_ipaddr(netdev, &address);
            netdev_low_level_set_dhcp_status(netdev, RT_TRUE);
            link_up = RT_TRUE;
        }

        if (gd32vw553_parse_value_by_key(resp, "STAMAC", mac, sizeof(mac)) &&
            rt_sscanf(mac, "%x:%x:%x:%x:%x:%x",
                      &mac_addr[0], &mac_addr[1], &mac_addr[2],
                      &mac_addr[3], &mac_addr[4], &mac_addr[5]) == 6)
        {
            for (index = 0; index < netdev->hwaddr_len; index++)
            {
                netdev->hwaddr[index] = (rt_uint8_t)mac_addr[index];
            }
        }
    }

    if (netdev->netmask.addr == 0 && gd32vw553_inet_aton("255.255.255.0", &address))
    {
        netdev_low_level_set_netmask(netdev, &address);
    }

    if (netdev->gw.addr == 0 && link_up)
    {
        unsigned int octet[4];

        if (rt_sscanf(ip, "%u.%u.%u.%u", &octet[0], &octet[1], &octet[2], &octet[3]) == 4)
        {
            rt_snprintf(gateway, sizeof(gateway), "%u.%u.%u.1", octet[0], octet[1], octet[2]);
            if (gd32vw553_inet_aton(gateway, &address))
            {
                netdev_low_level_set_gw(netdev, &address);
            }
        }
    }

    if (netdev->dns_servers[0].addr == 0 && gd32vw553_inet_aton("114.114.114.114", &address))
    {
        netdev_low_level_set_dns_server(netdev, 0, &address);
    }
    if (netdev->dns_servers[1].addr == 0 && gd32vw553_inet_aton("8.8.8.8", &address))
    {
        netdev_low_level_set_dns_server(netdev, 1, &address);
    }

    netdev_low_level_set_link_status(netdev, link_up);
    at_delete_resp(resp);
    return link_up;
}

static rt_bool_t gd32vw553_wait_netdev_ready(struct at_device *device)
{
    rt_uint32_t waited;

    for (waited = 0; waited < GD32VW553_NET_WAIT_MS; waited += GD32VW553_NET_POLL_MS)
    {
        if (gd32vw553_sync_netdev_info(device))
        {
            return RT_TRUE;
        }
        rt_thread_mdelay(GD32VW553_NET_POLL_MS);
    }

    return RT_FALSE;
}

static void gd32vw553_get_netdev_info(struct rt_work *work, void *work_data)
{
    if (work != RT_NULL)
    {
        rt_free(work);
    }
    gd32vw553_sync_netdev_info((struct at_device *)work_data);
}

static void gd32vw553_netdev_start_delay_work(struct at_device *device)
{
    struct rt_work *work = (struct rt_work *)rt_calloc(1, sizeof(struct rt_work));

    if (work != RT_NULL)
    {
        rt_work_init(work, gd32vw553_get_netdev_info, device);
        if (rt_work_submit(work, RT_TICK_PER_SECOND) != RT_EOK)
        {
            rt_free(work);
        }
    }
}

static void gd32vw553_urc_wifi(struct at_client *client, const char *data, rt_size_t size)
{
    struct at_device *device;
    const char *client_name;

    RT_UNUSED(size);

    client_name = client->device->parent.name;
    device = at_device_get_by_name(AT_DEVICE_NAMETYPE_CLIENT, client_name);
    if (device == RT_NULL || device->netdev == RT_NULL)
    {
        return;
    }

    if (rt_strstr(data, "WIFI DISCONNECT") != RT_NULL)
    {
        netdev_low_level_set_link_status(device->netdev, RT_FALSE);
    }
    else if (device->is_init && rt_strstr(data, "WIFI CONNECTED") != RT_NULL)
    {
        gd32vw553_netdev_start_delay_work(device);
    }
}

static const struct at_urc gd32vw553_urc_table[] =
{
    {"WIFI CONNECTED",  "\r\n", gd32vw553_urc_wifi},
    {"WIFI DISCONNECT", "\r\n", gd32vw553_urc_wifi},
};

static int gd32vw553_netdev_set_up(struct netdev *netdev)
{
    struct at_device *device;

    device = at_device_get_by_name(AT_DEVICE_NAMETYPE_NETDEV, netdev->name);
    if (device == RT_NULL)
    {
        return -RT_ERROR;
    }

    if (device->is_init == RT_FALSE)
    {
        return gd32vw553_net_init(device);
    }

    netdev_low_level_set_status(netdev, RT_TRUE);
    return RT_EOK;
}

static int gd32vw553_netdev_set_down(struct netdev *netdev)
{
    struct at_device *device;

    device = at_device_get_by_name(AT_DEVICE_NAMETYPE_NETDEV, netdev->name);
    if (device != RT_NULL)
    {
        device->is_init = RT_FALSE;
    }
    netdev_low_level_set_link_status(netdev, RT_FALSE);
    netdev_low_level_set_status(netdev, RT_FALSE);
    return RT_EOK;
}

static int gd32vw553_netdev_set_addr_info(struct netdev *netdev, ip_addr_t *ip_addr,
                                           ip_addr_t *netmask, ip_addr_t *gw)
{
    RT_UNUSED(netdev);
    RT_UNUSED(ip_addr);
    RT_UNUSED(netmask);
    RT_UNUSED(gw);
    return -RT_ENOSYS;
}

static int gd32vw553_netdev_set_dns_server(struct netdev *netdev, uint8_t dns_num,
                                            ip_addr_t *dns_server)
{
    RT_UNUSED(netdev);
    RT_UNUSED(dns_num);
    RT_UNUSED(dns_server);
    return -RT_ENOSYS;
}

static int gd32vw553_netdev_set_dhcp(struct netdev *netdev, rt_bool_t is_enabled)
{
    RT_UNUSED(netdev);
    RT_UNUSED(is_enabled);
    return -RT_ENOSYS;
}

#ifdef NETDEV_USING_PING
static rt_bool_t gd32vw553_parse_ping_time(at_response_t resp, int *time_ms)
{
    rt_size_t line_number;

    for (line_number = 1; line_number <= resp->line_counts; line_number++)
    {
        const char *line = at_resp_get_line(resp, line_number);

        if (line != RT_NULL &&
            (rt_sscanf(line, "+PING:%d", time_ms) == 1 ||
             rt_sscanf(line, "+%d", time_ms) == 1 ||
             rt_sscanf(line, "%d", time_ms) == 1))
        {
            return RT_TRUE;
        }
    }

    return RT_FALSE;
}

static int gd32vw553_netdev_ping(struct netdev *netdev, const char *host,
                                  size_t data_len, uint32_t timeout,
                                  struct netdev_ping_resp *ping_resp
#if RT_VER_NUM >= 0x50100
                                  , rt_bool_t is_bind
#endif
                                  )
{
    struct at_device *device;
    at_response_t resp;
    int time_ms = 0;
    int result = -RT_ERROR;

#if RT_VER_NUM >= 0x50100
    RT_UNUSED(is_bind);
#endif

    if (netdev == RT_NULL || host == RT_NULL || ping_resp == RT_NULL)
    {
        return -RT_ERROR;
    }

    device = at_device_get_by_name(AT_DEVICE_NAMETYPE_NETDEV, netdev->name);
    if (device == RT_NULL)
    {
        return -RT_ERROR;
    }

    timeout = timeout < 10000 ? 10000 : timeout;
    resp = at_create_resp(128, 0, timeout);
    if (resp == RT_NULL)
    {
        return -RT_ENOMEM;
    }

    gd32vw553_inet_aton(host, &(ping_resp->ip_addr));
    if (at_obj_exec_cmd(device->client, resp, "AT+PING=\"%s\"", host) == RT_EOK &&
        gd32vw553_parse_ping_time(resp, &time_ms))
    {
        ping_resp->data_len = data_len;
        ping_resp->ttl = 0;
        ping_resp->ticks = time_ms;
        result = RT_EOK;
    }

    at_delete_resp(resp);
    return result;
}
#endif /* NETDEV_USING_PING */

static const struct netdev_ops gd32vw553_netdev_ops =
{
    .set_up = gd32vw553_netdev_set_up,
    .set_down = gd32vw553_netdev_set_down,
    .set_addr_info = gd32vw553_netdev_set_addr_info,
    .set_dns_server = gd32vw553_netdev_set_dns_server,
    .set_dhcp = gd32vw553_netdev_set_dhcp,
#ifdef NETDEV_USING_PING
    .ping = gd32vw553_netdev_ping,
#endif
};

static struct netdev *gd32vw553_netdev_add(const char *netdev_name)
{
    struct netdev *netdev = netdev_get_by_name(netdev_name);

    if (netdev != RT_NULL)
    {
        return netdev;
    }

    netdev = (struct netdev *)rt_calloc(1, sizeof(struct netdev));
    if (netdev == RT_NULL)
    {
        return RT_NULL;
    }

    netdev->mtu = 1500;
    netdev->ops = &gd32vw553_netdev_ops;
    netdev->hwaddr_len = 6;

#ifdef SAL_USING_AT
    {
        extern int sal_at_netdev_set_pf_info(struct netdev *netdev);
        sal_at_netdev_set_pf_info(netdev);
    }
#endif

    netdev_register(netdev, netdev_name, RT_NULL);
    return netdev;
}

static int gd32vw553_network_init(struct at_device *device)
{
    struct at_device_gd32vw553 *gd32vw553 =
        (struct at_device_gd32vw553 *)device->user_data;
    rt_bool_t link_up = RT_FALSE;
    int result = RT_EOK;

    if (at_client_obj_wait_connect(device->client, GD32VW553_WAIT_CONNECT_TIME) != RT_EOK)
    {
        result = -RT_ETIMEOUT;
    }
    else if (gd32vw553_configure_at(device) != RT_EOK)
    {
        result = -RT_ERROR;
    }
    else
    {
        link_up = gd32vw553_sync_netdev_info(device);
        if (link_up == RT_FALSE && gd32vw553->wifi_ssid != RT_NULL &&
            gd32vw553->wifi_ssid[0] != '\0')
        {
            if (gd32vw553_join_ap(device, gd32vw553->wifi_ssid,
                                  gd32vw553->wifi_password) != RT_EOK)
            {
                result = -RT_ERROR;
            }
            else if (gd32vw553_wait_netdev_ready(device) == RT_FALSE)
            {
                result = -RT_ETIMEOUT;
            }
            else
            {
                link_up = RT_TRUE;
            }
        }
    }

    if (result == RT_EOK)
    {
        device->is_init = RT_TRUE;
        netdev_low_level_set_status(device->netdev, RT_TRUE);
        netdev_low_level_set_link_status(device->netdev, link_up);
        if (link_up == RT_FALSE)
        {
            gd32vw553_netdev_start_delay_work(device);
        }
        LOG_I("%s device network initialize successfully.", device->name);
    }
    else
    {
        device->is_init = RT_FALSE;
        netdev_low_level_set_link_status(device->netdev, RT_FALSE);
        netdev_low_level_set_status(device->netdev, RT_FALSE);
        LOG_E("%s device network initialize failed(%d).", device->name, result);
    }

    return result;
}

static void gd32vw553_init_thread_entry(void *parameter)
{
    gd32vw553_network_init((struct at_device *)parameter);
}

static int gd32vw553_net_init(struct at_device *device)
{
#ifdef AT_DEVICE_GD32VW553_INIT_ASYN
    rt_thread_t thread;

    thread = rt_thread_create("gd32net", gd32vw553_init_thread_entry, device,
                              GD32VW553_THREAD_STACK_SIZE,
                              GD32VW553_THREAD_PRIORITY, 20);
    if (thread == RT_NULL)
    {
        return -RT_ERROR;
    }
    return rt_thread_startup(thread);
#else
    return gd32vw553_network_init(device);
#endif
}

static int gd32vw553_config_uart(const char *client_name, rt_uint32_t baudrate)
{
    struct serial_configure config = RT_SERIAL_CONFIG_DEFAULT;
    rt_device_t serial = rt_device_find(client_name);

    if (serial == RT_NULL)
    {
        return -RT_ERROR;
    }

    config.baud_rate = baudrate;
    config.bufsz = GD32VW553_UART_RX_BUFSZ;
    return rt_device_control(serial, RT_DEVICE_CTRL_CONFIG, &config);
}

static int gd32vw553_init(struct at_device *device)
{
    struct at_device_gd32vw553 *gd32vw553 =
        (struct at_device_gd32vw553 *)device->user_data;
    int result;

    result = gd32vw553_config_uart(gd32vw553->client_name, gd32vw553->baudrate);
    if (result != RT_EOK)
    {
        return result;
    }

    if (gd32vw553->use_reset_pin && gd32vw553->reset_pin >= 0)
    {
        result = gd32vw553_hardware_reset(gd32vw553);
        if (result != RT_EOK)
        {
            return result;
        }
    }

#if RT_VER_NUM >= 0x50100
    result = at_client_init(gd32vw553->client_name, gd32vw553->recv_line_num,
                            gd32vw553->recv_line_num);
#else
    result = at_client_init(gd32vw553->client_name, gd32vw553->recv_line_num);
#endif
    if (result != RT_EOK)
    {
        return result;
    }

    device->client = at_client_get(gd32vw553->client_name);
    if (device->client == RT_NULL)
    {
        return -RT_ERROR;
    }

    result = at_obj_set_urc_table(device->client, gd32vw553_urc_table,
                                  sizeof(gd32vw553_urc_table) /
                                  sizeof(gd32vw553_urc_table[0]));
    if (result != RT_EOK)
    {
        return result;
    }

#ifdef AT_USING_SOCKET
    result = gd32vw553_socket_init(device);
    if (result != RT_EOK)
    {
        return result;
    }
#endif

    device->netdev = gd32vw553_netdev_add(gd32vw553->device_name);
    if (device->netdev == RT_NULL)
    {
        return -RT_ERROR;
    }

    return gd32vw553_netdev_set_up(device->netdev);
}

static int gd32vw553_deinit(struct at_device *device)
{
    return gd32vw553_netdev_set_down(device->netdev);
}

static int gd32vw553_reset(struct at_device *device)
{
    struct at_device_gd32vw553 *gd32vw553 =
        (struct at_device_gd32vw553 *)device->user_data;
    at_response_t resp;
    int result;

    device->is_init = RT_FALSE;
    netdev_low_level_set_link_status(device->netdev, RT_FALSE);
    netdev_low_level_set_status(device->netdev, RT_FALSE);

    if (gd32vw553->use_reset_pin && gd32vw553->reset_pin >= 0)
    {
        result = gd32vw553_hardware_reset(gd32vw553);
    }
    else
    {
        resp = at_create_resp(256, 0, 3 * RT_TICK_PER_SECOND);
        if (resp == RT_NULL)
        {
            return -RT_ENOMEM;
        }
        result = at_obj_exec_cmd(device->client, resp, "AT+RST");
        at_delete_resp(resp);
        rt_thread_mdelay(GD32VW553_RESET_BOOT_MS);
    }

    if (result != RT_EOK)
    {
        return result;
    }

    return gd32vw553_net_init(device);
}

static int gd32vw553_wifi_info_set(struct at_device *device,
                                    struct at_device_ssid_pwd *info)
{
    int result;

    if (info == RT_NULL || info->ssid == RT_NULL || info->password == RT_NULL)
    {
        return -RT_ERROR;
    }

    result = gd32vw553_join_ap(device, info->ssid, info->password);
    if (result == RT_EOK && gd32vw553_wait_netdev_ready(device) == RT_FALSE)
    {
        result = -RT_ETIMEOUT;
    }

    return result;
}

static int gd32vw553_control(struct at_device *device, int cmd, void *arg)
{
    switch (cmd)
    {
    case AT_DEVICE_CTRL_RESET:
        return gd32vw553_reset(device);
    case AT_DEVICE_CTRL_SET_WIFI_INFO:
        return gd32vw553_wifi_info_set(device, (struct at_device_ssid_pwd *)arg);
    default:
        LOG_W("not support the control cmd(%d).", cmd);
        return -RT_ENOSYS;
    }
}

static const struct at_device_ops gd32vw553_device_ops =
{
    gd32vw553_init,
    gd32vw553_deinit,
    gd32vw553_control,
};

static int gd32vw553_device_class_register(void)
{
    struct at_device_class *class;

    class = (struct at_device_class *)rt_calloc(1, sizeof(struct at_device_class));
    if (class == RT_NULL)
    {
        return -RT_ENOMEM;
    }

#ifdef AT_USING_SOCKET
    gd32vw553_socket_class_register(class);
#endif
    class->device_ops = &gd32vw553_device_ops;

    return at_device_class_register(class, AT_DEVICE_CLASS_GD32VW553);
}
INIT_DEVICE_EXPORT(gd32vw553_device_class_register);

#endif /* AT_DEVICE_USING_GD32VW553 */

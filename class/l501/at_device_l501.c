/*
 * Copyright (c) 2006-2023, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-08-25     moneng       first version for L501
 */

#include <stdio.h>
#include <string.h>
#include <rtthread.h>
#include <rtdevice.h>
#include <netdev.h>
#include <at_device_l501.h>

#define LOG_TAG                         "at.dev.l501"
#include <at_log.h>

#ifdef AT_DEVICE_USING_L501

#define L501_WAIT_CONNECT_TIME          10000
#define L501_THREAD_STACK_SIZE          2048
#define L501_THREAD_PRIORITY            (RT_THREAD_PRIORITY_MAX / 2)

/* ---------- 电源控制（保留原 ec200x 逻辑，但适配 L501 时序） ---------- */
static int l501_power_on(struct at_device *device)
{
    struct at_device_l501 *l501 = (struct at_device_l501 *)device->user_data;

    /* 使用用户提供的硬件控制函数 */
    l501->fun_power(PIN_LOW);
    l501->fun_onoff(PIN_LOW);
    l501->fun_reset(PIN_LOW);
    rt_thread_mdelay(1000);
    l501->fun_power(PIN_HIGH);
    rt_thread_mdelay(100);
    l501->fun_onoff(PIN_HIGH);
    rt_thread_mdelay(1111);
    l501->fun_onoff(PIN_LOW);
    rt_thread_mdelay(100);
    //l501->fun_reset(PIN_HIGH);
    //rt_thread_mdelay(1111);
    l501->fun_reset(PIN_LOW);
    rt_thread_mdelay(3000);

    l501->power_status = RT_TRUE;
    return RT_EOK;
}

static int l501_power_off(struct at_device *device)
{
    struct at_device_l501 *l501 = (struct at_device_l501 *)device->user_data;

    /* 使用用户提供的硬件控制函数 */
    l501->fun_power(PIN_LOW);
    l501->fun_reset(PIN_LOW);
    l501->fun_onoff(PIN_LOW);
    rt_thread_mdelay(500);

    l501->power_status = RT_FALSE;
    return RT_EOK;
}

/* L501 暂不支持睡眠，留空或返回成功 */
static int l501_sleep(struct at_device *device)
{
    LOG_W("L501 sleep not supported.");
    return RT_EOK;
}

static int l501_wakeup(struct at_device *device)
{
    LOG_W("L501 wakeup not supported.");
    return RT_EOK;
}

/* ---------- 链路检查 ---------- */
static int l501_check_link_status(struct at_device *device)
{
    at_response_t resp = RT_NULL;
    struct at_device_l501 *l501 = (struct at_device_l501 *)device->user_data;
    int result = -RT_ERROR;

    if (!l501->power_status) {
        LOG_D("power is off.");
        return -RT_ERROR;
    }

    resp = at_create_resp(64, 0, rt_tick_from_millisecond(300));
    if (resp == RT_NULL) {
        LOG_E("no memory for resp.");
        return -RT_ERROR;
    }

    /* 使用 AT+CEREG? 检查 EPS 注册状态 */
    if (at_obj_exec_cmd(device->client, resp, "AT+CEREG?") == RT_EOK) {
        int link_stat = 0;
        if (at_resp_parse_line_args_by_kw(resp, "+CEREG:", "+CEREG: %*d,%d", &link_stat) > 0) {
            /* 1=注册，5=已注册并漫游 */
            if (link_stat == 1 || link_stat == 5) {
                result = RT_EOK;
            }
        }
    }

    at_delete_resp(resp);
    return result;
}

static int l501_read_rssi(struct at_device *device)
{
    int result = -RT_ERROR;
    at_response_t resp = at_create_resp(64, 0, rt_tick_from_millisecond(300));
    if (resp == RT_NULL) {
        LOG_E("no memory for resp.");
        return result;
    }

    if (at_obj_exec_cmd(device->client, resp, "AT+CSQ") == RT_EOK) {
        int rssi = 0;
        if (at_resp_parse_line_args_by_kw(resp, "+CSQ:", "+CSQ: %d", &rssi) > 0) {
            struct at_device_l501 *l501 = (struct at_device_l501 *)device->user_data;
            if (rssi < 99) {
                l501->rssi = rssi * 2 - 113;   // 转换为 dBm
            } else {
                l501->rssi = -113;             // 无效值
            }
            result = RT_EOK;
        }
    }

    at_delete_resp(resp);
    return result;
}

/* ---------- netdev 操作 ---------- */
static int l501_netdev_set_info(struct netdev *netdev)
{
#define L501_INFO_RESP_SIZE      256
#define L501_INFO_RESP_TIMO      rt_tick_from_millisecond(1000)

    int result = RT_EOK;
    ip_addr_t addr;
    at_response_t resp = RT_NULL;
    struct at_device *device = RT_NULL;

    RT_ASSERT(netdev);

    device = at_device_get_by_name(AT_DEVICE_NAMETYPE_NETDEV, netdev->name);
    if (device == RT_NULL) {
        LOG_E("get device(%s) failed.", netdev->name);
        return -RT_ERROR;
    }

    netdev_low_level_set_status(netdev, RT_TRUE);
    netdev_low_level_set_link_status(netdev, RT_TRUE);
    netdev_low_level_set_dhcp_status(netdev, RT_TRUE);

    resp = at_create_resp(L501_INFO_RESP_SIZE, 0, L501_INFO_RESP_TIMO);
    if (resp == RT_NULL) {
        LOG_E("no memory for resp.");
        return -RT_ENOMEM;
    }

    /* 1. 获取 IMEI (AT+GSN) */
    {
        char imei[16] = {0};
        if (at_obj_exec_cmd(device->client, resp, "AT+GSN") == RT_EOK) {
            at_resp_parse_line_args(resp, 2, "%s", imei);
            LOG_D("IMEI: %s", imei);
            /* 将 IMEI 转为 MAC 地址（简略） */
            netdev->hwaddr_len = 8;
            for (int i = 0, j = 0; i < netdev->hwaddr_len && j < 15; i++, j += 2) {
                netdev->hwaddr[i] = (imei[j] - '0') * 10 + (imei[j+1] - '0');
            }
        }
    }

    /* 2. 获取 ICCID (AT+CCID) */
    {
        if (at_obj_exec_cmd(device->client, resp, "AT+CCID") == RT_EOK) {
            char ccid[32] = {0};
            if (at_resp_parse_line_args_by_kw(resp, "+CCID:", "+CCID:%s", ccid) > 0) {
                LOG_D("CCID: %s", ccid);
            }
        }
    }

    #if 0
    /* 3. 获取 IP 地址 (AT+CIPADDR?) 或从 NETOPEN 响应中获取，这里使用 AT+CIPADDR? */
    {
        char ipaddr[16] = {0};
        if (at_obj_exec_cmd(device->client, resp, "AT+CIPADDR?") == RT_EOK) {
            /* 响应格式: +CIPADDR: <IP> */
            if (at_resp_parse_line_args_by_kw(resp, "+CIPADDR:", "+CIPADDR: %s", ipaddr) > 0) {
                LOG_D("IP: %s", ipaddr);
                inet_aton(ipaddr, &addr);
                netdev_low_level_set_ipaddr(netdev, &addr);
            }
        }
    }
    #else
    /* 3. 获取 IP 地址 (AT+CGPADDR) */
    {
        char ipaddr[16] = {0};
        /* 发送 AT+CGPADDR=1，查询 cid=1 的 IP 地址 */
        if (at_obj_exec_cmd(device->client, resp, "AT+CGPADDR=1") == RT_EOK) {
            /* 响应格式: +CGPADDR: 1,"<IP_address>" */
            //if (at_resp_parse_line_args_by_kw(resp, "+CGPADDR:", "+CGPADDR: %*d,\"%[^\"]\"", ipaddr) > 0) {
            if (at_resp_parse_line_args_by_kw(resp, "+CGPADDR:", "+CGPADDR: %*d, \"%[^\"]\"", ipaddr) > 0) {
                LOG_D("IP: %s", ipaddr);
                inet_aton(ipaddr, &addr);
                netdev_low_level_set_ipaddr(netdev, &addr);
            } else {
                LOG_W("Parsing IP address from CGPADDR response failed.");
            }
        } else {
            LOG_W("AT+CGPADDR=1 command failed, maybe PDP context not active yet.");
        }
    }
    #endif

    /* 4. 获取 DNS (可能通过 AT+NETDNS? 或解析 NETOPEN 返回，我们暂且不设置) */
    /* 如有需要可添加 AT+NETDNS 查询 */
    {
        char dns1[16] = {0}, dns2[16] = {0};
        if (at_obj_exec_cmd(device->client, resp, "AT+NETDNS?") == RT_EOK) {
            /* 假设格式: +NETDNS: <pri_dns>,<sec_dns> 或 +NETDNS: <pri_dns> */
            if (at_resp_parse_line_args_by_kw(resp, "+NETDNS:", "+NETDNS: %[^,],%s", dns1, dns2) > 0) {
                inet_aton(dns1, &addr);
                netdev_low_level_set_dns_server(netdev, 0, &addr);
                if (strlen(dns2) > 0) {
                    inet_aton(dns2, &addr);
                    netdev_low_level_set_dns_server(netdev, 1, &addr);
                }
                LOG_D("DNS primary: %s, secondary: %s", dns1, dns2);
            } else if (at_resp_parse_line_args_by_kw(resp, "+NETDNS:", "+NETDNS: %s", dns1) > 0) {
                inet_aton(dns1, &addr);
                netdev_low_level_set_dns_server(netdev, 0, &addr);
                LOG_D("DNS primary: %s", dns1);
            } else {
                LOG_W("Parsing DNS from NETDNS? response failed.");
            }
        } else {
            LOG_W("AT+NETDNS? command failed.");
        }
    }

__exit:
    if (resp) at_delete_resp(resp);
    return result;
}

static void l501_check_link_entry(void *parameter)
{
#define L501_LINK_DELAY_TIME    (60 * RT_TICK_PER_SECOND)

    struct netdev *netdev = (struct netdev *)parameter;
    struct at_device *device = at_device_get_by_name(AT_DEVICE_NAMETYPE_NETDEV, netdev->name);

    while (1) {
        l501_read_rssi(device);
        rt_thread_delay(L501_LINK_DELAY_TIME);
        rt_bool_t up = (l501_check_link_status(device) == RT_EOK);
        netdev_low_level_set_link_status(netdev, up);
    }
}

static int l501_netdev_check_link_status(struct netdev *netdev)
{
    rt_thread_t tid;
    char tname[RT_NAME_MAX] = {0};
    rt_snprintf(tname, RT_NAME_MAX, "%s", netdev->name);
    tid = rt_thread_create(tname, l501_check_link_entry, (void *)netdev,
                           L501_THREAD_STACK_SIZE, L501_THREAD_PRIORITY, 20);
    if (tid) rt_thread_startup(tid);
    return RT_EOK;
}

static int l501_net_init(struct at_device *device);

static int l501_netdev_set_up(struct netdev *netdev)
{
    struct at_device *device = at_device_get_by_name(AT_DEVICE_NAMETYPE_NETDEV, netdev->name);
    if (device == RT_NULL) return -RT_ERROR;

    if (!device->is_init) {
        l501_net_init(device);
        device->is_init = RT_TRUE;
        netdev_low_level_set_status(netdev, RT_TRUE);
    }
    return RT_EOK;
}

static int l501_netdev_set_down(struct netdev *netdev)
{
    struct at_device *device = at_device_get_by_name(AT_DEVICE_NAMETYPE_NETDEV, netdev->name);
    if (device == RT_NULL) return -RT_ERROR;

    if (device->is_init) {
        l501_power_off(device);
        device->is_init = RT_FALSE;
        netdev_low_level_set_status(netdev, RT_FALSE);
    }
    return RT_EOK;
}

static int l501_netdev_set_dns_server(struct netdev *netdev, uint8_t dns_num, ip_addr_t *dns_server)
{
    /* L501 可能支持 AT+CDNSCFG，若支持则实现，这里简单返回成功 */
    LOG_W("DNS setting not implemented for L501.");
    netdev_low_level_set_dns_server(netdev, dns_num, dns_server);
    return RT_EOK;
}

#ifdef NETDEV_USING_PING
static int l501_netdev_ping(struct netdev *netdev, const char *host,
        size_t data_len, uint32_t timeout, struct netdev_ping_resp *ping_resp)
{
#define L501_PING_RESP_SIZE       128
#define L501_PING_IP_SIZE         16

    int result = RT_EOK;
    int response = -1, ping_time = 0;
    char ip_addr[L501_PING_IP_SIZE] = {0};
    at_response_t resp = RT_NULL;
    struct at_device *device = RT_NULL;

    RT_ASSERT(netdev);
    RT_ASSERT(host);
    RT_ASSERT(ping_resp);

    device = at_device_get_by_name(AT_DEVICE_NAMETYPE_NETDEV, netdev->name);
    if (device == RT_NULL) {
        LOG_E("get device(%s) failed.", netdev->name);
        return -RT_ERROR;
    }

    /* 超时时间转换为秒，建议使用 timeout / RT_TICK_PER_SECOND，若 timeout 以 ms 传入 */
    int timeout_sec = timeout / RT_TICK_PER_SECOND;
    if (timeout_sec < 1) timeout_sec = 1;

    resp = at_create_resp(L501_PING_RESP_SIZE, 4, timeout);
    if (resp == RT_NULL) {
        LOG_E("no memory for resp");
        return -RT_ENOMEM;
    }

    /* 发送 AT+MPING="<host>",<timeout_sec> */
    if (at_obj_exec_cmd(device->client, resp, "AT+MPING=\"%s\",%d", host, timeout_sec) < 0) {
        result = -RT_ERROR;
        goto __exit;
    }

    /* 解析响应，常见格式：+MPING: 0,"8.8.8.8",<time_ms> */
    /* 尝试带引号格式 */
    if (at_resp_parse_line_args_by_kw(resp, "+MPING:", "+MPING: %d,\"%[^\"]\",%d", &response, ip_addr, &ping_time) > 0) {
        if (response == 0) {
            inet_aton(ip_addr, &(ping_resp->ip_addr));
            ping_resp->data_len = data_len;   // 实际无法获取，保留传入值
            ping_resp->ticks = ping_time;     // 毫秒
            ping_resp->ttl = 0;               // L501 可能不返回 TTL
            result = RT_EOK;
        } else {
            result = -RT_ETIMEOUT;
        }
    }
    /* 尝试无引号格式：+MPING: 0,8.8.8.8,<time_ms> */
    else if (at_resp_parse_line_args_by_kw(resp, "+MPING:", "+MPING: %d,%[^,],%d", &response, ip_addr, &ping_time) > 0) {
        if (response == 0) {
            inet_aton(ip_addr, &(ping_resp->ip_addr));
            ping_resp->data_len = data_len;
            ping_resp->ticks = ping_time;
            ping_resp->ttl = 0;
            result = RT_EOK;
        } else {
            result = -RT_ETIMEOUT;
        }
    } else {
        /* 可能超时或其他错误，尝试检查是否包含 "ERROR" 或 "TIMEOUT" */
        if (strstr(resp->buf, "ERROR") || strstr(resp->buf, "TIMEOUT")) {
            result = -RT_ETIMEOUT;
        } else {
            result = -RT_ERROR;
        }
    }

__exit:
    if (resp) at_delete_resp(resp);
    return result;
}
#endif /* NETDEV_USING_PING */

#ifdef NETDEV_USING_NETSTAT
static void l501_netdev_netstat(struct netdev *netdev)
{
    /* 可通过 AT+CIPSTATUS? 查询，但暂不实现 */
    LOG_W("Netstat not implemented for L501.");
}
#endif

static const struct netdev_ops l501_netdev_ops = {
    l501_netdev_set_up,
    l501_netdev_set_down,
    RT_NULL,
    l501_netdev_set_dns_server,
    RT_NULL,
#ifdef NETDEV_USING_PING
    l501_netdev_ping,
#endif
#ifdef NETDEV_USING_NETSTAT
    l501_netdev_netstat,
#endif
};

static struct netdev *l501_netdev_add(const char *netdev_name)
{
    struct netdev *netdev = netdev_get_by_name(netdev_name);
    if (netdev) return netdev;

    netdev = (struct netdev *)rt_calloc(1, sizeof(struct netdev));
    if (!netdev) {
        LOG_E("no memory for netdev.");
        return RT_NULL;
    }
    netdev->mtu = 1500;
    netdev->ops = &l501_netdev_ops;
    netdev->hwaddr_len = 8;

#ifdef SAL_USING_AT
    extern int sal_at_netdev_set_pf_info(struct netdev *netdev);
    sal_at_netdev_set_pf_info(netdev);
#endif

    netdev_register(netdev, netdev_name, RT_NULL);
    return netdev;
}

/* ---------- 设备初始化线程 ---------- */
static void l501_init_thread_entry(void *parameter)
{
#define RESP_SIZE         128
#define CPIN_RETRY        10
#define CSQ_RETRY         20
#define CEREG_RETRY       50
#define NETOPEN_RETRY     10

    int i, retry = 5;
    rt_err_t result = RT_EOK;
    at_response_t resp = RT_NULL;
    struct at_device *device = (struct at_device *)parameter;
    struct at_client *client = device->client;
    struct at_device_l501 *l501 = (struct at_device_l501 *)device->user_data;

    resp = at_create_resp(RESP_SIZE, 0, rt_tick_from_millisecond(300));
    if (!resp) {
        LOG_E("no memory for resp.");
        return;
    }

    LOG_D("start init %s device.", device->name);

    while (retry--) {
        l501_power_on(device);
        rt_thread_mdelay(1000);

        if (at_client_obj_wait_connect(client, L501_WAIT_CONNECT_TIME)) {
            result = -RT_ETIMEOUT;
            goto __exit;
        }

        /* 关闭回显 */
        if (at_obj_exec_cmd(device->client, resp, "ATE0") != RT_EOK) {
            result = -RT_ERROR;
            goto __exit;
        }

        /* 查询波特率 */
        if (at_obj_exec_cmd(device->client, resp, "AT+IPR?") == RT_EOK) {
            int br = 0;
            at_resp_parse_line_args_by_kw(resp, "+IPR:", "+IPR: %d", &br);
            LOG_D("baudrate: %d", br);
        }

        /* 查询模块信息 */
        if (at_obj_exec_cmd(device->client, resp, "ATI") == RT_EOK) {
            for (i = 0; i < (int)resp->line_counts - 1; i++) {
                LOG_D("%s", at_resp_get_line(resp, i + 1));
            }
        }

        /* 检查 SIM 卡 */
        for (i = 0; i < CPIN_RETRY; i++) {
            rt_thread_mdelay(1000);
            if (at_obj_exec_cmd(device->client, resp, "AT+CPIN?") == RT_EOK) {
                if (at_resp_get_line_by_kw(resp, "READY") != RT_NULL)
                    break;
            }
        }
        if (i == CPIN_RETRY) {
            LOG_E("SIM card detection failed.");
            result = -RT_ERROR;
            goto __exit;
        }

        /* 检查信号强度 */
        for (i = 0; i < CSQ_RETRY; i++) {
            rt_thread_mdelay(1000);
            if (at_obj_exec_cmd(device->client, resp, "AT+CSQ") == RT_EOK) {
                int rssi, ber;
                if (at_resp_parse_line_args_by_kw(resp, "+CSQ:", "+CSQ: %d,%d", &rssi, &ber) > 0) {
                    if (rssi != 99 && rssi != 0) {
                        LOG_D("CSQ: %d, BER: %d", rssi, ber);
                        break;
                    }
                }
            }
        }
        if (i == CSQ_RETRY) {
            LOG_E("CSQ check failed.");
            result = -RT_ERROR;
            goto __exit;
        }

        /* 检查 EPS 注册 (CEREG) */
        for (i = 0; i < CEREG_RETRY; i++) {
            rt_thread_mdelay(1000);
            if (at_obj_exec_cmd(device->client, resp, "AT+CEREG?") == RT_EOK) {
                int stat;
                if (at_resp_parse_line_args_by_kw(resp, "+CEREG:", "+CEREG: %*d,%d", &stat) > 0) {
                    if (stat == 1 || stat == 5) {
                        LOG_D("CEREG registered.");
                        break;
                    }
                }
            }
        }
        if (i == CEREG_RETRY) {
            LOG_E("CEREG register failed.");
            result = -RT_ERROR;
            goto __exit;
        }

        /* 设置 PDP 上下文：AT+QICSGP=1,<APN>,,,0 */
        /* 这里 APN 需要从配置获取，此处用空字符串，用户可修改 */
        if (at_obj_exec_cmd(device->client, resp, "AT+QICSGP=1,1,\"cmnbiot\",\"\",\"\"") != RT_EOK) {
            LOG_E("set PDP context failed.");
            result = -RT_ERROR;
            goto __exit;
        }

        /* 打开网络：AT+NETOPEN */
        resp = at_resp_set_info(resp, RESP_SIZE, 0, rt_tick_from_millisecond(150 * 1000));
        if (at_obj_exec_cmd(device->client, resp, "AT+NETOPEN") != RT_EOK) {
            LOG_E("NETOPEN failed.");
            result = -RT_ERROR;
            goto __exit;
        }

        /* 成功 */
        result = RT_EOK;
        break;

    __exit:
        if (result != RT_EOK) {
            l501_power_off(device);
            rt_thread_mdelay(3000);
            LOG_I("%s init retry...", device->name);
        }
    }

    if (resp) at_delete_resp(resp);

    if (result == RT_EOK) {
        l501_netdev_set_info(device->netdev);
        if (rt_thread_find(device->netdev->name) == RT_NULL) {
            l501_netdev_check_link_status(device->netdev);
        }
        LOG_I("%s network init success.", device->name);
    } else {
        LOG_E("%s network init failed(%d).", device->name, result);
    }
}

static int l501_net_init(struct at_device *device)
{
#ifdef AT_DEVICE_L501_INIT_ASYN
    rt_thread_t tid = rt_thread_create("l501_net", l501_init_thread_entry, (void *)device,
                                       L501_THREAD_STACK_SIZE, L501_THREAD_PRIORITY, 20);
    if (tid) rt_thread_startup(tid);
    else LOG_E("create init thread failed.");
#else
    l501_init_thread_entry(device);
#endif
    return RT_EOK;
}

/* ---------- 设备 ops ---------- */
static int l501_init(struct at_device *device)
{
    struct at_device_l501 *l501 = (struct at_device_l501 *)device->user_data;
    l501->power_status = RT_FALSE;
    l501->sleep_status = RT_FALSE;

    at_client_init(l501->client_name, l501->recv_line_num);
    device->client = at_client_get(l501->client_name);
    if (!device->client) {
        LOG_E("get client(%s) failed.", l501->client_name);
        return -RT_ERROR;
    }

#ifdef AT_USING_SOCKET
    l501_socket_init(device);
#endif

    device->netdev = l501_netdev_add(l501->device_name);
    if (!device->netdev) {
        LOG_E("add netdev failed.");
        return -RT_ERROR;
    }

    /* 引脚初始化（若使用引脚控制） */
    if (l501->power_pin != -1) {
        rt_pin_write(l501->power_pin, PIN_LOW);
        rt_pin_mode(l501->power_pin, PIN_MODE_OUTPUT);
    }
    if (l501->power_status_pin != -1) {
        rt_pin_mode(l501->power_status_pin, PIN_MODE_INPUT);
    }
    if (l501->wakeup_pin != -1) {
        rt_pin_write(l501->wakeup_pin, PIN_LOW);
        rt_pin_mode(l501->wakeup_pin, PIN_MODE_OUTPUT);
    }

    return l501_netdev_set_up(device->netdev);
}

static int l501_deinit(struct at_device *device)
{
    return l501_netdev_set_down(device->netdev);
}

static int l501_control(struct at_device *device, int cmd, void *arg)
{
    switch (cmd) {
    case AT_DEVICE_CTRL_SLEEP:   return l501_sleep(device);
    case AT_DEVICE_CTRL_WAKEUP:  return l501_wakeup(device);
    default:
        LOG_W("ctrl cmd %d not supported.", cmd);
        return -RT_ERROR;
    }
}

static const struct at_device_ops l501_device_ops = {
    l501_init,
    l501_deinit,
    l501_control,
};

/* 注册设备类 */
static int l501_device_class_register(void)
{
    struct at_device_class *class = (struct at_device_class *)rt_calloc(1, sizeof(struct at_device_class));
    if (!class) {
        LOG_E("no memory for class.");
        return -RT_ENOMEM;
    }

#ifdef AT_USING_SOCKET
    l501_socket_class_register(class);
#endif
    class->device_ops = &l501_device_ops;

    return at_device_class_register(class, AT_DEVICE_CLASS_L501);
}
INIT_DEVICE_EXPORT(l501_device_class_register);

#endif /* AT_DEVICE_USING_L501 */

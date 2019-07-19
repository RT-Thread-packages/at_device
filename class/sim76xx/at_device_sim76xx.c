/*
 * File      : at_socket_sim76xx.c
 * This file is part of RT-Thread RTOS
 * COPYRIGHT (C) 2006 - 2018, RT-Thread Development Team
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
 * 2018-12-22    thomasonegd  first version
 * 2019-03-06    thomasonegd  fix udp connection.
 * 2019-03-08    thomasonegd  add power_on & power_off api
 * 2019-05-14    chenyong     multi AT socket client support
 */

#include <stdio.h>
#include <string.h>

#include <at_device_sim76xx.h>

#define LOG_TAG                        "at.dev"
#include <at_log.h>

#ifdef AT_DEVICE_USING_SIM76XX

#define SIM76XX_WAIT_CONNECT_TIME      5000
#define SIM76XX_THREAD_STACK_SIZE      1024
#define SIM76XX_THREAD_PRIORITY        (RT_THREAD_PRIORITY_MAX / 2)

/**
 * power up sim76xx modem
 */
static void sim76xx_power_on(struct at_device *device)
{
    struct at_device_sim76xx *sim76xx = RT_NULL;

    sim76xx = (struct at_device_sim76xx *) device->user_data;

    /* not nead to set pin configuration for m26 device power on */
    if (sim76xx->power_pin == -1 || sim76xx->power_status_pin == -1)
    {
        return;
    }

    if (rt_pin_read(sim76xx->power_status_pin) == PIN_HIGH)
    {
        return;
    }
    rt_pin_write(sim76xx->power_pin, PIN_HIGH);

    while (rt_pin_read(sim76xx->power_status_pin) == PIN_LOW)
    {
        rt_thread_mdelay(10);
    }
    rt_pin_write(sim76xx->power_pin, PIN_LOW);
}

static void sim76xx_power_off(struct at_device *device)
{
    struct at_device_sim76xx *sim76xx = RT_NULL;

    sim76xx = (struct at_device_sim76xx *) device->user_data;

    /* not nead to set pin configuration for m26 device power on */
    if (sim76xx->power_pin == -1 || sim76xx->power_status_pin == -1)
    {
        return;
    }

    if (rt_pin_read(sim76xx->power_status_pin) == PIN_LOW)
    {
        return;
    }
    rt_pin_write(sim76xx->power_pin, PIN_HIGH);

    while (rt_pin_read(sim76xx->power_status_pin) == PIN_HIGH)
    {
        rt_thread_mdelay(10);
    }
    rt_pin_write(sim76xx->power_pin, PIN_LOW);
}

/* =============================  sim76xx network interface operations ============================= */

static struct netdev *sim76xx_netdev_add(const char *netdev_name)
{
#define ETHERNET_MTU 1500
#define HWADDR_LEN 8
    struct netdev *netdev = RT_NULL;

    RT_ASSERT(netdev_name);

    netdev = (struct netdev *)rt_calloc(1, sizeof(struct netdev));
    if (netdev == RT_NULL)
    {
        LOG_E("no memory for sim76xx device(%s) netdev structure.", netdev_name);
        return RT_NULL;
    }

    netdev->mtu = ETHERNET_MTU;
    netdev->hwaddr_len = HWADDR_LEN;
    netdev->ops = RT_NULL;

#ifdef SAL_USING_AT
    extern int sal_at_netdev_set_pf_info(struct netdev * netdev);
    /* set the network interface socket/netdb operations */
    sal_at_netdev_set_pf_info(netdev);
#endif

    netdev_register(netdev, netdev_name, RT_NULL);

    /*TODO: improve netdev adaptation */
    netdev_low_level_set_status(netdev, RT_TRUE);
    netdev_low_level_set_link_status(netdev, RT_TRUE);
    netdev_low_level_set_dhcp_status(netdev, RT_TRUE);
    netdev->flags |= NETDEV_FLAG_INTERNET_UP;

    return netdev;
}

/* =============================  sim76xx device operations ============================= */

#define AT_SEND_CMD(client, resp, cmd)                                          \
    do {                                                                        \
        (resp) = at_resp_set_info((resp), 256, 0, 5 * RT_TICK_PER_SECOND);      \
        if (at_obj_exec_cmd((client), (resp), (cmd)) < 0)                       \
        {                                                                       \
            result = -RT_ERROR;                                                 \
            goto __exit;                                                        \
        }                                                                       \
    } while(0)                                                                  \


static void sim76xx_init_thread_entry(void *parameter)
{
#define INIT_RETRY                     5
#define CSQ_RETRY                      20
#define CREG_RETRY                     10
#define CGREG_RETRY                    20
#define CGATT_RETRY                    10
#define CCLK_RETRY                     10

    at_response_t resp = RT_NULL;
    rt_err_t result = RT_EOK;
    rt_size_t i, qi_arg[3] = {0};
    int retry_num = INIT_RETRY;
    char parsed_data[20] = {0};
    struct at_device *device = (struct at_device *)parameter;
    struct at_client *client = device->client;

    resp = at_create_resp(128, 0, rt_tick_from_millisecond(300));
    if (resp == RT_NULL)
    {
        LOG_E("no memory for sim76xx device(%s) response structure.", device->name);
        return;
    }

    LOG_D("start initializing the sim76xx device(%s).", device->name);

    while (retry_num--)
    {
        /* power-up sim76xx */
        sim76xx_power_on(device);
        rt_thread_mdelay(1000);

        /* wait SIM76XX startup finish, Send AT every 5s, if receive OK, SYNC success*/
        if (at_client_wait_connect(SIM76XX_WAIT_CONNECT_TIME))
        {
            result = -RT_ETIMEOUT;
            goto __exit;
        }

        /* disable echo */
        AT_SEND_CMD(client, resp, "ATE0");

        /* get module version */
        AT_SEND_CMD(client, resp, "ATI");

        /* show module version */
        for (i = 0; i < (int)resp->line_counts - 1; i++)
        {
            LOG_D("%s", at_resp_get_line(resp, i + 1));
        }
        /* check SIM card */

        AT_SEND_CMD(client, resp, "AT+CPIN?");
        if (!at_resp_get_line_by_kw(resp, "READY"))
        {
            LOG_E("sim76xx device(%s) SIM card detection failed.", device->name);
            result = -RT_ERROR;
            goto __exit;
        }

        /* waiting for dirty data to be digested */
        rt_thread_mdelay(10);
        /* check signal strength */
        for (i = 0; i < CSQ_RETRY; i++)
        {
            AT_SEND_CMD(client, resp, "AT+CSQ");
            at_resp_parse_line_args_by_kw(resp, "+CSQ:", "+CSQ: %d,%d", &qi_arg[0], &qi_arg[1]);
            if (qi_arg[0] != 99)
            {
                LOG_D("sim76xx device(%s) signal strength: %d  Channel bit error rate: %d", device->name, qi_arg[0], qi_arg[1]);
                break;
            }
            rt_thread_mdelay(1000);
        }

        if (i == CSQ_RETRY)
        {
            LOG_E("sim76xx device(%s) signal strength check failed (%s).", device->name, parsed_data);
            result = -RT_ERROR;
            goto __exit;
        }

        /* do not show the prompt when receiving data */
        AT_SEND_CMD(client, resp, "AT+CIPSRIP=0");

        /* check the GSM network is registered */
        for (i = 0; i < CREG_RETRY; i++)
        {
            AT_SEND_CMD(client, resp, "AT+CREG?");
            at_resp_parse_line_args_by_kw(resp, "+CREG:", "+CREG: %s", &parsed_data);
            if (!strncmp(parsed_data, "0,1", sizeof(parsed_data)) || !strncmp(parsed_data, "0,5", sizeof(parsed_data)))
            {
                LOG_D("sim76xx device(%s) GSM network is registered(%s).", device->name, parsed_data);
                break;
            }
            rt_thread_mdelay(1000);
        }
        if (i == CREG_RETRY)
        {
            LOG_E("sim76xx device(%s) GSM network is register failed(%s).", device->name, parsed_data);
            result = -RT_ERROR;
            goto __exit;
        }
        /* check the GPRS network is registered */
        for (i = 0; i < CGREG_RETRY; i++)
        {
            AT_SEND_CMD(client, resp, "AT+CGREG?");
            at_resp_parse_line_args_by_kw(resp, "+CGREG:", "+CGREG: %s", &parsed_data);
            if (!strncmp(parsed_data, "0,1", sizeof(parsed_data)) || !strncmp(parsed_data, "0,5", sizeof(parsed_data)))
            {
                LOG_D("sim76xx device(%s) GPRS network is registered(%s).", device->name, parsed_data);
                break;
            }
            rt_thread_mdelay(1000);
        }

        if (i == CGREG_RETRY)
        {
            LOG_E("sim76xx device(%s) GPRS network is register failed(%s).", device->name, parsed_data);
            result = -RT_ERROR;
            goto __exit;
        }

        /* check packet domain attach or detach */
        for (i = 0; i < CGATT_RETRY; i++)
        {
            AT_SEND_CMD(client, resp, "AT+CGATT?");
            at_resp_parse_line_args_by_kw(resp, "+CGATT:", "+CGATT: %s", &parsed_data);
            if (!strncmp(parsed_data, "1", 1))
            {
                LOG_I("sim76xx device(%s) Packet domain attach.", device->name);
                break;
            }

            rt_thread_mdelay(1000);
        }

        if (i == CGATT_RETRY)
        {
            LOG_E("sim76xx device(%s) GPRS network attach failed.", device->name);
            result = -RT_ERROR;
            goto __exit;
        }

        /* get real time */
        int year, month, day, hour, min, sec;

        for (i = 0; i < CCLK_RETRY; i++)
        {
            if (at_obj_exec_cmd(device->client, at_resp_set_info(resp, 256, 0, 5 * RT_TICK_PER_SECOND), "AT+CCLK?") < 0)
            {
                rt_thread_mdelay(500);
                continue;
            }

            /* +CCLK: "18/12/22,18:33:12+32" */
            if (at_resp_parse_line_args_by_kw(resp, "+CCLK:", "+CCLK: \"%d/%d/%d,%d:%d:%d", &year, &month, &day, &hour, &min, &sec) < 0)
            {
                rt_thread_mdelay(500);
                continue;
            }

            set_date(year + 2000, month, day);
            set_time(hour, min, sec);

            break;
        }

        if (i == CCLK_RETRY)
        {
            LOG_E("sim76xx device(%s) GPRS network attach failed.", device->name);
            result = -RT_ERROR;
            goto __exit;
        }

        /* set active PDP context's profile number */
        AT_SEND_CMD(client, resp, "AT+CSOCKSETPN=1");

    __exit:
        if (result == RT_EOK)
        {
            break;
        }
        else
        {
            /* power off the sim76xx device */
            sim76xx_power_off(device);
            rt_thread_mdelay(1000);

            LOG_I("sim76xx device(%s) initialize retry...", device->name);
        }
    }

    if (resp)
    {
        at_delete_resp(resp);
    }

    if (result == RT_EOK)
    {
        LOG_I("sim76xx devuce(%s) network initialize success!", device->name);
    }
    else
    {
        LOG_E("sim76xx device(%s) network initialize failed(%d)!", device->name, result);
    }
}

int sim76xx_net_init(struct at_device *device)
{
#ifdef AT_DEVICE_SIM76XX_INIT_ASYN
    rt_thread_t tid;

    tid = rt_thread_create("sim76xx_net_init", sim76xx_init_thread_entry, (void *)device,
                           SIM76XX_THREAD_STACK_SIZE, SIM76XX_THREAD_PRIORITY, 20);
    if (tid)
    {
        rt_thread_startup(tid);
    }
    else
    {
        LOG_E("create sim76xx device(%s) initialization thread failed.", device->name);
        return -RT_ERROR;
    }
#else
    sim76xx_init_thread_entry(device);
#endif /* AT_DEVICE_SIM76XX_INIT_ASYN */

    return RT_EOK;
}

int sim76xx_ping(int argc, char **argv)
{
    at_response_t resp = RT_NULL;
    struct at_device *device = RT_NULL;

    if (argc != 2)
    {
        rt_kprintf("Please input: at_ping <host address>\n");
        return -RT_ERROR;
    }

    device = at_device_get_first_initialized();
    if (device == RT_NULL)
    {
        rt_kprintf("get first initialized sim76xx device failed.\n");
        return -RT_ERROR;
    }

    resp = at_create_resp(64, 0, 5 * RT_TICK_PER_SECOND);
    if (resp == RT_NULL)
    {
        rt_kprintf("no memory for sim76xx device(%s) response structure.\n", device->name);
        return -RT_ENOMEM;
    }

    if (at_obj_exec_cmd(device->client, resp, "AT+CPING=\"%s\",1,4,64,1000,10000,255", argv[1]) < 0)
    {
        if (resp)
        {
            at_delete_resp(resp);
        }
        rt_kprintf("sim76xx device(%s) send ping commands error.\n", device->name);
        return -RT_ERROR;
    }

    if (resp)
    {
        at_delete_resp(resp);
    }

    return RT_EOK;
}

int sim76xx_ifconfig(void)
{
    at_response_t resp = RT_NULL;
    char resp_arg[AT_CMD_MAX_LEN] = {0};
    rt_err_t result = RT_EOK;
    struct at_device *device = RT_NULL;

    device = at_device_get_first_initialized();
    if (device == RT_NULL)
    {
        rt_kprintf("get first initialized sim76xx device failed.\n");
        return -RT_ERROR;
    }

    resp = at_create_resp(128, 2, rt_tick_from_millisecond(300));
    if (resp == RT_NULL)
    {
        rt_kprintf("no memory for sim76xx device(%s) response structure.\n", device->name);
        return -RT_ENOMEM;
    }

    /* Show PDP address */
    AT_SEND_CMD(device->client, resp, "AT+CGPADDR");
    at_resp_parse_line_args_by_kw(resp, "+CGPADDR:", "+CGPADDR: %s", &resp_arg);
    rt_kprintf("IP adress : %s\n", resp_arg);

__exit:
    if (resp)
    {
        at_delete_resp(resp);
    }

    return result;
}

#ifdef FINSH_USING_MSH
#include <finsh.h>
MSH_CMD_EXPORT_ALIAS(sim76xx_net_init, at_net_init, initialize AT network);
MSH_CMD_EXPORT_ALIAS(sim76xx_ping, at_ping, AT ping network host);
MSH_CMD_EXPORT_ALIAS(sim76xx_ifconfig, at_ifconfig, list the information of network interfaces);
#endif

static void urc_ping_func(struct at_client *client, const char *data, rt_size_t size)
{
    static int icmp_seq = 0;
    int i, j = 0;
    int result, recv_len, time, ttl;
    int sent, rcvd, lost, min, max, avg;
    char dst_ip[16] = {0};

    RT_ASSERT(data);

    for (i = 0; i < size; i++)
    {
        if (*(data + i) == '.')
            j++;
    }
    if (j != 0)
    {
        sscanf(data, "+CPING: %d,%[^,],%d,%d,%d", &result, dst_ip, &recv_len, &time, &ttl);
        if (result == 1)
            LOG_I("%d bytes from %s icmp_seq=%d ttl=%d time=%d ms", recv_len, dst_ip, icmp_seq++, ttl, time);
    }
    else
    {
        sscanf(data, "+CPING: %d,%d,%d,%d,%d,%d,%d", &result, &sent, &rcvd, &lost, &min, &max, &avg);
        if (result == 3)
            LOG_I("%d sent %d received %d lost, min=%dms max=%dms average=%dms", sent, rcvd, lost, min, max, avg);
        if (result == 2)
            LOG_I("ping requst timeout");
    }
}

/* sim76xx device URC table for the device control */
static struct at_urc urc_table[] = 
{
        {"+CPING:",        "\r\n",           urc_ping_func},
};

static int sim76xx_init(struct at_device *device)
{
    struct at_device_sim76xx *sim76xx = (struct at_device_sim76xx *) device->user_data;

    /* initialize AT client */
    at_client_init(sim76xx->client_name, sim76xx->recv_line_num);

    device->client = at_client_get(sim76xx->client_name);
    if (device->client == RT_NULL)
    {
        LOG_E("sim76xx device(%s) initialize failed, get AT client(%s) failed.", sim76xx->device_name, sim76xx->client_name);
        return -RT_ERROR;
    }

    /* register URC data execution function  */
    at_obj_set_urc_table(device->client, urc_table, sizeof(urc_table) / sizeof(urc_table[0]));

#ifdef AT_USING_SOCKET
    sim76xx_socket_init(device);
#endif

    /* add sim76xx device to the netdev list */
    device->netdev = sim76xx_netdev_add(sim76xx->device_name);
    if (device->netdev == RT_NULL)
    {
        LOG_E("sim76xx device(%s) initialize failed, get network interface device failed.", sim76xx->device_name);
        return -RT_ERROR;
    }

    /* initialize sim76xx pin configuration */
    if (sim76xx->power_pin != -1 && sim76xx->power_status_pin != -1)
    {
        rt_pin_mode(sim76xx->power_pin, PIN_MODE_OUTPUT);
        rt_pin_mode(sim76xx->power_status_pin, PIN_MODE_INPUT);
    }

    /* initialize sim76xx device network */
    sim76xx_net_init(device);

    return RT_EOK;
}

static int sim76xx_deinit(struct at_device *device)
{
    // TODO netdev operation
    device->is_init = RT_FALSE;
    return RT_EOK;
}

static int sim76xx_control(struct at_device *device, int cmd, void *arg)
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
        LOG_W("sim76xx not support the control command(%d).", cmd);
        break;
    default:
        LOG_E("input error control command(%d).", cmd);
        break;
    }

    return result;
}

const struct at_device_ops sim76xx_device_ops =
{
    sim76xx_init,
    sim76xx_deinit,
    sim76xx_control,
};

static int sim76xx_device_class_register(void)
{
    struct at_device_class *class = RT_NULL;

    class = (struct at_device_class *)rt_calloc(1, sizeof(struct at_device_class));
    if (class == RT_NULL)
    {
        LOG_E("no memory for sim76xx device class create.");
        return -RT_ENOMEM;
    }

    /* fill sim76xx device class object */
#ifdef AT_USING_SOCKET
    sim76xx_socket_class_register(class);
#endif
    class->device_ops = &sim76xx_device_ops;

    return at_device_class_register(class, AT_DEVICE_CLASS_SIM76XX);
}
INIT_DEVICE_EXPORT(sim76xx_device_class_register);

#endif /* AT_DEVICE_SIM76XX */

/*
 * File      : at_socket_a9g.c
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
 * 2020-09-30     DENGs        first version
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include <at_device_probe.h>

#define LOG_TAG                    "at.dev.probe"
#include <at_log.h>
 
#ifdef AT_DEVICE_USING_PROBE

/* The global list of at device probe kw */
static rt_slist_t at_device_probe_kw_list = RT_SLIST_OBJECT_INIT(at_device_probe_kw_list);

static int at_device_class_probe_kw_register(rt_slist_t *slist, struct at_device_probe_kw *kw)
{
    rt_base_t level;
    rt_slist_t *node = RT_NULL;
    struct at_device_probe_kw *kw_iter = RT_NULL;

    level = rt_hw_interrupt_disable();

    if (kw->user_func == RT_NULL)
    {
        rt_slist_for_each(node, slist)
        {
            kw_iter = rt_slist_entry(node, struct at_device_probe_kw, list);
            if (kw_iter && \
                rt_strcmp(kw_iter->kw, kw->kw) == 0 && \
                rt_strcmp(kw_iter->kw, kw->fmt) == 0)
            {
                rt_hw_interrupt_enable(level);
                return RT_EOK;
            }
        }
    }

    rt_slist_init(&(kw->list));
    rt_slist_append(slist, &(kw->list));

    rt_hw_interrupt_enable(level);

    return RT_EOK;
}

int at_device_class_probe_kw_ati_register(struct at_device_probe_kw *kw)
{
    RT_ASSERT(kw->kw);
    RT_ASSERT(kw->fmt);
    RT_ASSERT(kw->user_func == RT_NULL);

    return at_device_class_probe_kw_register(&at_device_probe_kw_list, kw);
}

int at_device_class_probe_kw_user_func_register(struct at_device_probe_kw *kw)
{
    RT_ASSERT(kw->kw == RT_NULL);
    RT_ASSERT(kw->fmt == RT_NULL);
    RT_ASSERT(kw->user_func);

    return at_device_class_probe_kw_register(&at_device_probe_kw_list, kw);
}

/* AT device class probe thread */
static void at_device_class_probe_thread_entry(void *parameter)
{
    struct at_device *device = (struct at_device *)parameter;
    struct at_device_probe_param *probe = (struct at_device_probe_param *)device->user_data;
    at_response_t resp = RT_NULL;

    /* Power init */
    probe->control(device, AT_DEVICE_PROBE_POWER_INIT, RT_NULL);

    resp = at_create_resp(256, 0, rt_tick_from_millisecond(1000));
    if (!resp)
    {
        LOG_E("No memory for response structure!");
        return;
    }

    device->class = RT_NULL;

    while (1)
    {
        /* Power on */
        probe->control(device, AT_DEVICE_CTRL_POWER_ON, RT_NULL);
        rt_thread_mdelay(5000);
        
        /* Wait AT startup finish */
        if (at_client_obj_wait_connect(device->client, 30 * 1000))
        {
            goto __next;
        }

        /* Disable echo */
        if (at_obj_exec_cmd(device->client, at_resp_set_info(resp, 256, 0, rt_tick_from_millisecond(1000)), "ATE0") < 0)
        {
            goto __next;
        }

        /* Try get at device model */
        {
            rt_slist_t *node = RT_NULL;
            struct at_device_probe_kw *kw_iter = RT_NULL;
            char parsed_data[32];
            int i;

            rt_strncpy(parsed_data, "?", sizeof(parsed_data));

            if (at_obj_exec_cmd(device->client, at_resp_set_info(resp, 256, 0, rt_tick_from_millisecond(1000)), "ATI") < 0)
            {
                /* user func, eg: esp8266 */
                rt_slist_for_each(node, &at_device_probe_kw_list)
                {
                    kw_iter = rt_slist_entry(node, struct at_device_probe_kw, list);
                    if (kw_iter && \
                        kw_iter->user_func && \
                        kw_iter->user_func(device->client, resp, parsed_data) == 0)
                    {
                        break;
                    }
                }
            }
            else
            {
                /* ati response */
                rt_slist_for_each(node, &at_device_probe_kw_list)
                {
                    kw_iter = rt_slist_entry(node, struct at_device_probe_kw, list);
                    if (kw_iter && \
                        !kw_iter->user_func && \
                        at_resp_parse_line_args_by_kw(resp, kw_iter->kw, kw_iter->fmt, parsed_data) == 1)
                    {
                        break;
                    }
                }
            }

            if (parsed_data[0] != '?')
            {
                device->class = at_device_class_get_by_name(parsed_data);
            }

            /* show latest at response */
            for (i = 0; i < (int) resp->line_counts - 1; i++)
            {
                LOG_I("%s", at_resp_get_line(resp, i + 1));
            }
            if (device->class == RT_NULL)
            {
                LOG_E("client(%s) no class match '%s'", probe->client_name, parsed_data);
                rt_thread_mdelay(3000);
                goto __next;
            }
        }

    __next:
        if (device->class != RT_NULL)
        {
            break;
        }

        /* Power off */
        probe->control(device, AT_DEVICE_CTRL_POWER_OFF, RT_NULL);
        rt_thread_mdelay(3000);
    }

    at_delete_resp(resp);

    if (device->class != RT_NULL)
    {
        LOG_I("client(%s) match class 0x%04x", probe->client_name, device->class->class_id);
        
        /* User register AT device with specific class id */
        probe->control(device, AT_DEVICE_PROBE_REGISTER, &device->class->class_id);
    }

    rt_free(device);
}

static int probe_init(struct at_device *device)
{
    struct at_device_probe_param *probe = (struct at_device_probe_param *)device->user_data;

    /* Initialize AT client */
    at_client_init(probe->client_name, probe->recv_line_num);

    device->client = at_client_get(probe->client_name);
    if (device->client == RT_NULL)
    {
        LOG_E("initialize failed, get AT client(%s) failed.", probe->client_name);
        return -RT_ERROR;
    }

    /* Asyn probe AT class */
    {
        rt_thread_t tid;
        
        tid = rt_thread_create("at_clsp", at_device_class_probe_thread_entry, (void *)device, 2048, RT_THREAD_PRIORITY_MAX / 2, 20);
        if (tid)
        {
            rt_thread_startup(tid);
        }
        else
        {
            LOG_E("create client(%s) at class probe thread failed.", probe->client_name);
            rt_free(device);
            return -RT_ERROR;
        }
    }

    return RT_EOK;
}

static int probe_deinit(struct at_device *device)
{
    return RT_EOK;
}

static int probe_control(struct at_device *device, int cmd, void *arg)
{
    return RT_EOK;
}

const struct at_device_ops probe_device_ops = 
{
    probe_init,
    probe_deinit,
    probe_control,
};

static int probe_device_class_register(void)
{
    struct at_device_class *class = RT_NULL;

    class = (struct at_device_class *) rt_calloc(1, sizeof(struct at_device_class));
    if (class == RT_NULL)
    {
        LOG_E("no memory for probe device class create.");
        return -RT_ENOMEM;
    }

    /* fill probe device class object */
    class->device_ops = &probe_device_ops;

    return at_device_class_register(class, AT_DEVICE_CLASS_PROBE);
}
INIT_DEVICE_EXPORT(probe_device_class_register);

#endif /* AT_DEVICE_USING_PROBE */

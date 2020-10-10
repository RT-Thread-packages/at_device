/*
 * File      : at_device_probe.h
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
 
#ifndef __AT_DEVICE_PROBE_H__
#define __AT_DEVICE_PROBE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>

#include <at_device.h>

/* Options and Commands for AT device probe opreations */
#define AT_DEVICE_PROBE_POWER_INIT     0x00L
#define AT_DEVICE_PROBE_POWER_ON       0x01L
#define AT_DEVICE_PROBE_POWER_OFF      0x02L
#define AT_DEVICE_PROBE_REGISTER       0x03L

struct at_device_probe_kw
{
    const char *kw;
    const char *fmt;
    int (*user_func)(at_client_t client, at_response_t resp, char model[32]);
    rt_slist_t list;
};

struct at_device_probe_param
{
    char *client_name;
    size_t recv_line_num;

    int (*control)(struct at_device *device, int cmd, void *arg);
    void *user_data;
};

int at_device_class_probe_kw_ati_register(struct at_device_probe_kw *kw);
int at_device_class_probe_kw_user_func_register(struct at_device_probe_kw *kw);

#ifdef __cplusplus
}
#endif

#endif  /* __AT_DEVICE_PROBE_H__ */

/*
 * File      : at_sample_probe.c
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

#include <board.h>
#include "at_device_probe.h"
#include "at_device_esp8266.h"
#include "at_device_air720.h"
#include "at_device_ec200x.h"

#define LOG_TAG                        "at.sample.probe"
#include <at_log.h>

static int at_device_probe_callback(struct at_device *device, int cmd, void *arg)
{
    uint16_t class_id;
    int power_pin = GET_PIN(C, 8);

    (void)(device);

    switch (cmd)
    {
    case AT_DEVICE_PROBE_POWER_INIT:
        LOG_I("power init");
        rt_pin_mode(power_pin, PIN_MODE_OUTPUT);
        break;

    case AT_DEVICE_PROBE_POWER_ON:
        LOG_I("power on");
        rt_pin_write(power_pin, PIN_HIGH);
        break;

    case AT_DEVICE_PROBE_POWER_OFF:
        LOG_I("power off");
        rt_pin_write(power_pin, PIN_LOW);
        break;

    case AT_DEVICE_PROBE_REGISTER:
        class_id = *(uint16_t *)arg;
        if (class_id == AT_DEVICE_CLASS_AIR720)
        {
            struct at_device_air720 *air720 = rt_calloc(1, sizeof(struct at_device_air720));
            if (air720 != RT_NULL)
            {
                air720->device_name = "e0";
                air720->client_name = "uart1";
                air720->power_pin = power_pin;
                air720->power_status_pin = -1;
                air720->recv_line_num = 256; /* unused */
                at_device_register(&air720->device, air720->device_name, air720->client_name, class_id, air720);
            }
        }
        else if (class_id == AT_DEVICE_CLASS_EC200X)
        {
            struct at_device_ec200x *ec200x = rt_calloc(1, sizeof(struct at_device_ec200x));
            if (ec200x != RT_NULL)
            {
                ec200x->device_name = "e0";
                ec200x->client_name = "uart1";
                ec200x->power_pin = power_pin;
                ec200x->power_status_pin = -1;
                ec200x->wakeup_pin = -1;
                ec200x->recv_line_num = 256; /* unused */
                at_device_register(&ec200x->device, ec200x->device_name, ec200x->client_name, class_id, ec200x);
            }
        }
        else if (class_id == AT_DEVICE_CLASS_ESP8266)
        {
            struct at_device_esp8266 *esp8266 = rt_calloc(1, sizeof(struct at_device_esp8266));
            if (esp8266 != RT_NULL)
            {
                esp8266->device_name = "e0";
                esp8266->client_name = "uart1";
                esp8266->wifi_ssid = "ssid";
                esp8266->wifi_password = "password";
                esp8266->recv_line_num = 256; /* unused */
                at_device_register(&esp8266->device, esp8266->device_name, esp8266->client_name, class_id, esp8266);
            }
        }
        else
        {
            LOG_E("unknown class 0x%04x", class_id);
        }
        break;

    default:
        break;
    }

    return 0;
}

static int sample_device_register(void)
{
    static struct at_device_probe_param probe =
    {
        "uart1",
        512,
        at_device_probe_callback,
    };

    at_device_probe(&probe);
    
    return 0;
}
INIT_COMPONENT_EXPORT(sample_device_register);

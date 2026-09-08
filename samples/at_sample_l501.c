/*
 * Copyright (c) 2006-2023, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-08-25     moneng       sample for L501
 */
#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>
#include <at_device_l501.h>

#define LOG_TAG                        "at.sample.l501"
#include <at_log.h>

#define L501_SAMPLE_DEIVCE_NAME        "l501"

//PCB工程中RESET和ONOFF命名混乱，程序中统一按照模组对应的引脚功能进行命名
//PCB工程中RESET和ONOFF都通过NPN三极管驱动，所以注意电平反转问题
#define L501_POWER_PIN      GET_PIN(E, 13) //高电平供电
#define L501_ONOFF_PIN      GET_PIN(C, 2)  //高电平使能
#define L501_RESET_PIN      GET_PIN(E, 11) //高电平使能

void l501_power(rt_base_t value)
{
    rt_pin_mode(L501_POWER_PIN, PIN_MODE_OUTPUT);
    rt_pin_write(L501_POWER_PIN, value);
    extern void ic_74hc273_ctrl(void);
    ic_74hc273_ctrl();
}
void l501_onoff(rt_base_t value)
{
    rt_pin_mode(L501_ONOFF_PIN, PIN_MODE_OUTPUT);
    rt_pin_write(L501_ONOFF_PIN, value);
    extern void ic_74hc273_ctrl(void);
    ic_74hc273_ctrl();
}
void l501_reset(rt_base_t value)
{
    rt_pin_mode(L501_RESET_PIN, PIN_MODE_OUTPUT);
    rt_pin_write(L501_RESET_PIN, value);
    extern void ic_74hc273_ctrl(void);
    ic_74hc273_ctrl();
}

/*static*/ struct at_device_l501 _dev_l501 = {
    .device_name    = L501_SAMPLE_DEIVCE_NAME,
    .client_name    = L501_SAMPLE_CLIENT_NAME,
    .fun_power      = l501_power,
    .fun_onoff      = l501_onoff,
    .fun_reset      = l501_reset,
    .power_pin      = L501_SAMPLE_POWER_PIN,      /* 若使用 fun_xxx 可设为 -1 */
    .power_status_pin = L501_SAMPLE_STATUS_PIN,
    .wakeup_pin     = L501_SAMPLE_WAKEUP_PIN,
    .recv_line_num  = L501_SAMPLE_RECV_BUFF_LEN,
};

/*static*/ int l501_device_register(void)
{
    struct at_device_l501 *l501 = &_dev_l501;
    return at_device_register(&(l501->device),
                              l501->device_name,
                              l501->client_name,
                              AT_DEVICE_CLASS_L501,
                              (void *)l501);
}
INIT_APP_EXPORT(l501_device_register);

int l501_get_rssi(void)
{
    struct at_device_l501 *l501 = &_dev_l501;
    return l501->rssi;
}

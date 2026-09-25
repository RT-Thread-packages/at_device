/*
 * Copyright (c) 2006-2023, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-08-25     moneng       first version for L501
 */

#ifndef __AT_DEVICE_L501_H__
#define __AT_DEVICE_L501_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <at_device.h>

/* 最大支持的 socket 数量，L501 通常支持 5 个 */
#define AT_DEVICE_L501_SOCKETS_NUM  5

struct at_device_l501
{
    char *device_name;
    char *client_name;

    /* 硬件控制函数（由用户实现） */
    void (*fun_power)(rt_base_t value);
    void (*fun_onoff)(rt_base_t value);
    void (*fun_reset)(rt_base_t value);

    /* 引脚号（若使用直接引脚控制，否则可设为 -1） */
    int power_pin;
    int power_status_pin;
    int wakeup_pin;          /* L501 可能不支持睡眠，保留但不使用 */
    size_t recv_line_num;

    struct at_device device;
    
    int socket_type[AT_DEVICE_L501_SOCKETS_NUM];   // 0: TCP, 1: UDP
    char remote_ip[AT_DEVICE_L501_SOCKETS_NUM][16];
    int remote_port[AT_DEVICE_L501_SOCKETS_NUM];
    rt_bool_t socket_opened[AT_DEVICE_L501_SOCKETS_NUM];

    /* 内部状态 */
    void *socket_data;       /* 用于域名解析的临时指针 */
    void *user_data;         /* 通用指针，可用于传递 socket 编号等 */

    rt_bool_t power_status;
    rt_bool_t sleep_status;  /* L501 暂不支持睡眠，置 FALSE */
    int rssi;
};

#ifdef AT_USING_SOCKET

/* socket 初始化 */
int l501_socket_init(struct at_device *device);

/* 注册 socket 类操作 */
int l501_socket_class_register(struct at_device_class *class);

#endif /* AT_USING_SOCKET */

#ifdef __cplusplus
}
#endif

#endif /* __AT_DEVICE_L501_H__ */

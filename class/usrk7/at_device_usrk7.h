/*
 * File      : at_device_usrk7.h
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

#ifndef __AT_DEVICE_USRK7_H__
#define __AT_DEVICE_USRK7_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stdio.h>

#include <at_device.h>

/* The maximum number of sockets supported by the usrk7 device */
#define AT_DEVICE_USRK7_SOCKETS_NUM	1
#define USRK7_CMDMODE_TRIGGER	"+++"
#define USRK7_CMDMODE_TRIGGER_SIZE	3
#define USRK7_CMDMODE_ACK		"a"
#define USRK7_CMDMODE_ACK_SIZE	1
#define USRK7_CMDMODE_OK		"+ok"
#define USRK7_CMDMODE_OK_SIZE	3

#define USRK7_ERR1_RESP		    "ERR1"
#define USRK7_ERR1_RESP_SIZE	4

#define USRK7_OK_RESP			"+OK"
#define USRK7_STR_RESP			"+OK=%s"
#define USRK7_INT_RESP			"+OK=%d"
#define USRK7_STR_ERR_RESP		"+ERR=%s"

#define USRK7_ECHO_CMD			"AT+E"
#define USRK7_ECHO_RESP		    USRK7_STR_RESP
#define USRK7_REBOOT_CMD		"AT+Z"
#define USRK7_REBOOT_RESP		USRK7_OK_RESP
#define USRK7_VERSION_CMD		"AT+VER"
#define USRK7_VERSION_RESP		USRK7_STR_RESP
#define USRK7_PDTIME_CMD		"AT+PDTIME"
#define USRK7_PDTIME_RESP		USRK7_STR_RESP
#define USRK7_MID_CMD			"AT+MID"
#define USRK7_MID_RESP			USRK7_STR_RESP
#define USRK7_MAC_CMD			"AT+MAC"
#define USRK7_MAC_RESP			USRK7_STR_RESP
#define USRK7_SETMAC_CMD		"AT+USERMAC"
#define USRK7_SETMAC_RESP		USRK7_OK_RESP
#define USRK7_DEFAULT_CMD		"AT+RELD"
#define USRK7_DEFAULT_RESP		USRK7_OK_RESP
#define USRK7_WANN_CMD			"AT+WANN"
#define USRK7_WANN_RESP		    "+OK=%s %s %s %s"
#define USRK7_DNS_CMD			"AT+DNS"
#define USRK7_DNS_RESP			USRK7_STR_RESP

#define USRK7_WEBU_CMD			"AT+WEBU"
#define USRK7_WEBU_RESP		    "+OK=%s %s"
#define USRK7_WEBPORT_CMD		"AT+WEBPORT"
#define USRK7_WEBPORT_RESP		USRK7_INT_RESP
#define USRK7_SEARCH_CMD		"AT+SEARCH"
#define USRK7_SEARCH_RESP		"+OK=%d %s"
#define USRK7_LANG_CMD			"AT+PLANG"
#define USRK7_LANG_RESP		    USRK7_STR_RESP

#define USRK7_UART_CMD			"AT+UART1"
#define USRK7_UART_RESP		    "+OK=%d %d %d %s %s"
#define USRK7_UARTTL_CMD		"AT+UARTTLN1"
#define USRK7_UARTTL_RESP		"+OK=%d %d"

#define USRK7_SOCKA1_CMD		"AT+UARTA1"
#define USRK7_SOCKA1_RESP		"+OK=%s %s %d"
#define USRK7_SOCKB1_CMD		"AT+UARTB1"
#define USRK7_SOCKB1_RESP		"+OK=%s %s %d"

#define USRK7_SOCKLKA1_CMD		"AT+UARTLKA1"
#define USRK7_SOCKLKA1_RESP		"+OK=%s %d) "
#define USRK7_SOCKLKB1_CMD		"AT+UARTLKB1"
#define USRK7_SOCKLKB1_RESP		USRK7_STR_RESP

#define USRK7_WEBSOCKPORT1_CMD	"AT+WEBSOCKPORT1"
#define USRK7_WEBSOCKPORT1_RESP	USRK7_INT_RESP

#define USRK7_REGEN1_CMD		"AT+REGEN1"
#define USRK7_REGEN1_RESP		USRK7_STR_RESP
#define USRK7_REGTCP1_CMD		"AT+REGTCP1"
#define USRK7_REGTCP1_RESP		USRK7_STR_RESP
#define USRK7_REGUSR1_CMD		"AT+REGUSR1"
#define USRK7_REGUSR1_RESP		USRK7_STR_RESP
#define USRK7_REGCLOUD1_CMD		"AT+REGCLOUD1"
#define USRK7_REGCLOUD1_RESP	"+OK=%s %s"
#define USRK7_REGUSER1_CMD		"AT+REGUSER1"
#define USRK7_REGUSER1_RESP		"+OK=%s %s"

#define USRK7_HTPTP1_CMD		"AT+HTPTP1"
#define USRK7_HTPTP1_RESP		USRK7_STR_RESP
#define USRK7_HTPURL1_CMD		"AT+HTPURL1"
#define USRK7_HTPURL1_RESP		USRK7_STR_RESP
#define USRK7_HTPHEAD1_CMD		"AT+HTPHEAD1"
#define USRK7_HTPHEAD1_RESP		USRK7_STR_RESP
#define USRK7_HTPCHD1_CMD		"AT+HTPCHD1"
#define USRK7_HTPCHD1_RESP		USRK7_STR_RESP

#define USRK7_HEARTEN1_CMD		"AT+HEARTEN1"
#define USRK7_HEARTEN1_RESP		USRK7_STR_RESP
#define USRK7_HEARTTP1_CMD		"AT+HEARTTP1"
#define USRK7_HEARTTP1_RESP		USRK7_STR_RESP
#define USRK7_HEARTTM1_CMD		"AT+HEARTTM1"
#define USRK7_HEARTTM1_RESP		USRK7_INT_RESP
#define USRK7_HEARTDT1_CMD		"AT+HEARTDT1"
#define USRK7_HEARTDT1_RESP		USRK7_STR_RESP
#define USRK7_HEARTUSER1_CMD	"AT+HEARTUSER1"
#define USRK7_HEARTUSER1_RESP	"+OK=%s %s"

#define USRK7_RFCEN1_CMD		"AT+RFCEN1"
#define USRK7_RFCEN1_RESP		USRK7_STR_RESP

#define USRK7_SOCKSL1_CMD		"AT+SOCKSL1"
#define USRK7_SOCKSL1_RESP		USRK7_STR_RESP
#define USRK7_SHORTO1_CMD		"AT+SHORTO1"
#define USRK7_SHORTO1_RESP		USRK7_INT_RESP

#define USRK7_RSTIM_CMD		    "AT+RSTIM"
#define USRK7_RSTIM_RESP		USRK7_INT_RESP
#define USRK7_UARTCLBUF_CMD		"AT+UARTCLBUF"
#define USRK7_UARTCLBUF_RESP	USRK7_STR_RESP
#define USRK7_SOCKTON1_CMD		"AT+SOCKTON1"
#define USRK7_SOCKTON1_RESP		USRK7_INT_RESP

#define USRK7_MODTCP1_CMD		"AT+MODTCP1"
#define USRK7_MODTCP1_RESP		USRK7_STR_RESP
#define USRK7_MODPOLL1_CMD		"AT+MODPOLL1"
#define USRK7_MODPOLL1_RESP		USRK7_STR_RESP
#define USRK7_MODTO1_CMD		"AT+MODTO1"
#define USRK7_MODTO1_RESP		USRK7_INT_RESP

#define USRK7_NETPR1_CMD		"AT+NETPR1"
#define USRK7_NETPR1_RESP		USRK7_STR_RESP
#define USRK7_UDPON1_CMD		"AT+UDPON1"
#define USRK7_UDPON1_RESP		USRK7_STR_RESP

#define USRK7_PING1_CMD		    "AT+PING1"
#define USRK7_PING1_RESP		USRK7_STR_RESP

#define USRK7_CFGTF_CMD		    "AT+CFGTF"
#define USRK7_CFGTF_RESP		USRK7_STR_RESP

#define USRK7_ENTMODE_CMD		"AT+ENTM"
#define USRK7_ENTMODE_RESP		USRK7_OK_RESP

#define USRK7_INIT_RETRY			5

#define USRK7_DEFAULT_MDELAY		1000
#define USRK7_RESET_MDELAY			2000

#define USRK7_CMD_STR_DEFAULT_SIZE	128
#define USRK7_RESP_DEFAULT_SIZE		128
#define USRK7_RESP_DEFAULT_LINE		2
#define USRK7_RESP_DEFAULT_LINE_NUM 2
#define USRK7_RESP_MINI_SIZE		16
#define USRK7_RESP_DEFAULT_TIMEOUT	500

#define USRK7_THREAD_STACK_SIZE		2048
#define USRK7_THREAD_PRIORITY		(RT_THREAD_PRIORITY_MAX / 2)

#define USRK7_UART_BAUDRATE			115200
#define USRK7_UART_DATE				8
#define USRK7_UART_STOP				1
#define USRK7_UART_PARITY			"NONE"
#define USRK7_UART_FLOWCTRL			"NFC"

#define AT_ADDR_STR_MAXLEN			16
#define MAC_ADDR_LEN				6
#define USRK7_ETHERNET_MTU			1500

#define USRK7_ERR_DNS_SERVER		"0.0.0.0"
#define USRK7_DEF_DNS_SERVER		"114.114.114.114"

#define AT_CLIENT_PASS_THROUGH_BUF_SIZE	512
#define AT_CLIENT_PASS_THROUGH_TIMEOUT	100

struct at_device_usrk7
{
    char *device_name;
    char *client_name;

    size_t recv_line_num;
    struct at_device device;

    void *user_data;
    int  device_socket;
    char version[USRK7_RESP_MINI_SIZE];
    char id[USRK7_RESP_MINI_SIZE];
};

#ifdef AT_USING_SOCKET

/* usrk7 device socket initialize */
int usrk7_socket_init(struct at_device *device);

/* usrk7 device class socket register */
int usrk7_socket_class_register(struct at_device_class *class);

#endif /* AT_USING_SOCKET */

/* enter usrk7 command mode */
int usrk7_cmdmode(struct at_device *device);

/* enter usrk7 transmission mode */
int usrk7_entmode(struct at_device *device);

void usrk7_str_ncrp(char *src, int maxlen, char org, char target);

int usrk7_at_sock_set(struct at_device *device, char *interface,
                      char *protocol, char *ip, int *port);
void usrk7_at_sock_get(struct at_device *device, char *interface,
                       char *protocol, char *ip, int *port, char *status);
void usrk7_at_wann_get(struct at_device *device, char *mode, char *ip,
                       char *netmask, char *gateway);
#ifdef __cplusplus
}
#endif

#endif /* __AT_DEVICE_USRK7_H__ */

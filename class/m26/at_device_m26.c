/*
 * File      : at_device_m26.c
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
 * 2018-06-12     chenyong     first version
 * 2019-05-12     chenyong     multi AT socket client support
 */

#include <stdio.h>
#include <string.h>

#include <at_device_m26.h>

#define LOG_TAG                        "at.dev"
#include <at_log.h>

#ifdef AT_DEVICE_USING_M26

#define M26_WAIT_CONNECT_TIME          5000
#define M26_THREAD_STACK_SIZE          1024
#define M26_THREAD_PRIORITY            (RT_THREAD_PRIORITY_MAX/2)

//从buf里面得到第cx个逗号所在的位置
//返回值:0~0XFE,代表逗号所在位置的偏移.
//       0XFF,代表不存在第cx个逗号							  
static unsigned char NMEA_Comma_Pos(unsigned char *buf,unsigned char cx)
{	 		    
	unsigned char *p=buf;
	while(cx)
	{		 
		if(*buf=='*'||*buf<' '||*buf>'z')return 0XFF;//遇到'*'或者非法字符,则不存在第cx个逗号
		if(*buf==',')cx--;
		buf++;
	}
	return buf-p;	 
}

//m^n函数
//返回值:m^n次方.
static uint32_t NMEA_Pow(uint8_t m,uint8_t n)
{
	uint32_t result=1;	 
	while(n--)result*=m;    
	return result;
}

//str转换为数字,以','或者'*'结束
//buf:数字存储区
//dx:小数点位数,返回给调用函数
//返回值:转换后的数值
static int NMEA_Str2num(uint8_t *buf,uint8_t*dx)
{
	uint8_t *p=buf;
	uint32_t ires=0,fres=0;
	uint8_t ilen=0,flen=0,i;
	uint8_t mask=0;
	int res;
	while(1) //得到整数和小数的长度
	{
		if(*p=='-'){mask|=0X02;p++;}//是负数
		if(*p==','||(*p=='*'))break;//遇到结束了
		if(*p=='.'){mask|=0X01;p++;}//遇到小数点了
		else if(*p>'9'||(*p<'0'))	//有非法字符
		{	
			ilen=0;
			flen=0;
			break;
		}	
		if(mask&0X01)flen++;
		else ilen++;
		p++;
	}
	if(mask&0X02)buf++;	//去掉负号
	for(i=0;i<ilen;i++)	//得到整数部分数据
	{  
		ires+=NMEA_Pow(10,ilen-1-i)*(buf[i]-'0');
	}
	if(flen>5)flen=5;	//最多取5位小数
	*dx=flen;	 		//小数点位数
	for(i=0;i<flen;i++)	//得到小数部分数据
	{  
		fres+=NMEA_Pow(10,flen-1-i)*(buf[ilen+1+i]-'0');
	} 
	res=ires*NMEA_Pow(10,flen)+fres;
	if(mask&0X02)res=-res;		   
	return res;
}	  							 

//分析GPGSV信息
//gpsx:nmea信息结构体
//buf:接收到的GPS数据缓冲区首地址
void NMEA_GPGSV_Analysis(nmea_msg *gpsx,uint8_t *buf)
{
	uint8_t *p,*p1,dx;
	uint8_t len,i,j,slx=0;
	uint8_t posx;   	 
	p=buf;
	p1=(uint8_t*)strstr((const char *)p,"$GPGSV");
	len=p1[7]-'0';								//得到GPGSV的条数
	posx=NMEA_Comma_Pos(p1,3); 					//得到可见卫星总数
	if(posx!=0XFF)gpsx->svnum=NMEA_Str2num(p1+posx,&dx);
	for(i=0;i<len;i++)
	{	 
		p1=(uint8_t*)strstr((const char *)p,"$GPGSV");  
		for(j=0;j<4;j++)
		{	  
			posx=NMEA_Comma_Pos(p1,4+j*4);
			if(posx!=0XFF)gpsx->slmsg[slx].num=NMEA_Str2num(p1+posx,&dx);	//得到卫星编号
			else break; 
			posx=NMEA_Comma_Pos(p1,5+j*4);
			if(posx!=0XFF)gpsx->slmsg[slx].eledeg=NMEA_Str2num(p1+posx,&dx);//得到卫星仰角 
			else break;
			posx=NMEA_Comma_Pos(p1,6+j*4);
			if(posx!=0XFF)gpsx->slmsg[slx].azideg=NMEA_Str2num(p1+posx,&dx);//得到卫星方位角
			else break; 
			posx=NMEA_Comma_Pos(p1,7+j*4);
			if(posx!=0XFF)gpsx->slmsg[slx].sn=NMEA_Str2num(p1+posx,&dx);	//得到卫星信噪比
			else break;
			slx++;	   
		}   
 		p=p1+1;//切换到下一个GPGSV信息
	}   
}

//分析GNVTG信息
//gpsx:nmea信息结构体
//buf:接收到的GPS数据缓冲区首地址
void NMEA_GNVTG_Analysis(nmea_msg *gpsx,uint8_t *buf)
{
	uint8_t *p1,dx;			 
	uint8_t posx;    
	p1=(uint8_t*)strstr((const char *)buf,"$GNVTG");							 
	posx=NMEA_Comma_Pos(p1,7);								//得到地面速率
	if(posx!=0XFF)
	{
		gpsx->speed=NMEA_Str2num(p1+posx,&dx);
		if(dx<3)gpsx->speed*=NMEA_Pow(10,3-dx);	 	 		//确保扩大1000倍
	}
}

//分析BDGSV信息
//gpsx:nmea信息结构体
//buf:接收到的GPS数据缓冲区首地址
void NMEA_BDGSV_Analysis(nmea_msg *gpsx,uint8_t *buf)
{
	uint8_t *p,*p1,dx;
	uint8_t len,i,j,slx=0;
	uint8_t posx;   	 
	p=buf;
	p1=(uint8_t*)strstr((const char *)p,"$BDGSV");
	len=p1[7]-'0';								//得到BDGSV的条数
	posx=NMEA_Comma_Pos(p1,3); 					//得到可见北斗卫星总数
	if(posx!=0XFF)gpsx->beidou_svnum=NMEA_Str2num(p1+posx,&dx);
	for(i=0;i<len;i++)
	{	 
		p1=(uint8_t*)strstr((const char *)p,"$BDGSV");  
		for(j=0;j<4;j++)
		{	  
			posx=NMEA_Comma_Pos(p1,4+j*4);
			if(posx!=0XFF)gpsx->beidou_slmsg[slx].beidou_num=NMEA_Str2num(p1+posx,&dx);	//得到卫星编号
			else break; 
			posx=NMEA_Comma_Pos(p1,5+j*4);
			if(posx!=0XFF)gpsx->beidou_slmsg[slx].beidou_eledeg=NMEA_Str2num(p1+posx,&dx);//得到卫星仰角 
			else break;
			posx=NMEA_Comma_Pos(p1,6+j*4);
			if(posx!=0XFF)gpsx->beidou_slmsg[slx].beidou_azideg=NMEA_Str2num(p1+posx,&dx);//得到卫星方位角
			else break; 
			posx=NMEA_Comma_Pos(p1,7+j*4);
			if(posx!=0XFF)gpsx->beidou_slmsg[slx].beidou_sn=NMEA_Str2num(p1+posx,&dx);	//得到卫星信噪比
			else break;
			slx++;	   
		}   
 		p=p1+1;//切换到下一个BDGSV信息
	}   
}
//分析GNGGA信息
//gpsx:nmea信息结构体
//buf:接收到的GPS数据缓冲区首地址
void NMEA_GNGGA_Analysis(nmea_msg *gpsx,uint8_t *buf)
{
	uint8_t *p1,dx;			 
	uint8_t posx;    
	p1=(uint8_t*)strstr((const char *)buf,"$GNGGA");
	posx=NMEA_Comma_Pos(p1,6);								//得到GPS状态
	if(posx!=0XFF)gpsx->gpssta=NMEA_Str2num(p1+posx,&dx);	
	posx=NMEA_Comma_Pos(p1,7);								//得到用于定位的卫星数
	if(posx!=0XFF)gpsx->posslnum=NMEA_Str2num(p1+posx,&dx); 
	posx=NMEA_Comma_Pos(p1,9);								//得到海拔高度
	if(posx!=0XFF)gpsx->altitude=NMEA_Str2num(p1+posx,&dx);  
}
//分析GNGSA信息
//gpsx:nmea信息结构体
//buf:接收到的GPS数据缓冲区首地址
void NMEA_GNGSA_Analysis(nmea_msg *gpsx,uint8_t *buf)
{
	uint8_t *p1,dx;			 
	uint8_t posx; 
	uint8_t i;   
	p1=(uint8_t*)strstr((const char *)buf,"$GNGSA");
	posx=NMEA_Comma_Pos(p1,2);								//得到定位类型
	if(posx!=0XFF)gpsx->fixmode=NMEA_Str2num(p1+posx,&dx);	
	for(i=0;i<12;i++)										//得到定位卫星编号
	{
		posx=NMEA_Comma_Pos(p1,3+i);					 
		if(posx!=0XFF)gpsx->possl[i]=NMEA_Str2num(p1+posx,&dx);
		else break; 
	}				  
	posx=NMEA_Comma_Pos(p1,15);								//得到PDOP位置精度因子
	if(posx!=0XFF)gpsx->pdop=NMEA_Str2num(p1+posx,&dx);  
	posx=NMEA_Comma_Pos(p1,16);								//得到HDOP位置精度因子
	if(posx!=0XFF)gpsx->hdop=NMEA_Str2num(p1+posx,&dx);  
	posx=NMEA_Comma_Pos(p1,17);								//得到VDOP位置精度因子
	if(posx!=0XFF)gpsx->vdop=NMEA_Str2num(p1+posx,&dx);  
}


//分析GNRMC信息
//gpsx:nmea信息结构体
//buf:接收到的GPS数据缓冲区首地址
void NMEA_GNRMC_Analysis(nmea_msg *gpsx,uint8_t *buf)
{
	uint8_t *p1,dx;			 
	uint8_t posx;     
	uint32_t temp;	   
	float rs;  
	p1=(uint8_t*)strstr((const char *)buf,"$GNRMC");//"$GNRMC",经常有&和GNRMC分开的情况,故只判断GPRMC.
	posx=NMEA_Comma_Pos(p1,1);								//得到UTC时间
	if(posx!=0XFF)
	{
		temp=NMEA_Str2num(p1+posx,&dx)/NMEA_Pow(10,dx);	 	//得到UTC时间,去掉ms
		gpsx->utc.hour=temp/10000;
		gpsx->utc.min=(temp/100)%100;
		gpsx->utc.sec=temp%100;	 	 
	}	
	posx=NMEA_Comma_Pos(p1,3);								//得到纬度
	if(posx!=0XFF)
	{
		temp=NMEA_Str2num(p1+posx,&dx);		 	 
		gpsx->latitude=temp/NMEA_Pow(10,dx+2);	//得到°
		rs=temp%NMEA_Pow(10,dx+2);				//得到'		 
		gpsx->latitude=gpsx->latitude*NMEA_Pow(10,5)+(rs*NMEA_Pow(10,5-dx))/60;//转换为° 
	}
	posx=NMEA_Comma_Pos(p1,4);								//南纬还是北纬 
	if(posx!=0XFF)gpsx->nshemi=*(p1+posx);					 
 	posx=NMEA_Comma_Pos(p1,5);								//得到经度
	if(posx!=0XFF)
	{												  
		temp=NMEA_Str2num(p1+posx,&dx);		 	 
		gpsx->longitude=temp/NMEA_Pow(10,dx+2);	//得到°
		rs=temp%NMEA_Pow(10,dx+2);				//得到'		 
		gpsx->longitude=gpsx->longitude*NMEA_Pow(10,5)+(rs*NMEA_Pow(10,5-dx))/60;//转换为° 
	}
	posx=NMEA_Comma_Pos(p1,6);								//东经还是西经
	if(posx!=0XFF)gpsx->ewhemi=*(p1+posx);		 
	posx=NMEA_Comma_Pos(p1,9);								//得到UTC日期
	if(posx!=0XFF)
	{
		temp=NMEA_Str2num(p1+posx,&dx);		 				//得到UTC日期
		gpsx->utc.date=temp/10000;
		gpsx->utc.month=(temp/100)%100;
		gpsx->utc.year=2000+temp%100;	 	 
	} 
}

static void m26_power_on(struct at_device *device)
{
    struct at_device_m26 *m26 = RT_NULL;

    m26 = (struct at_device_m26 *) device->user_data;

    /* not nead to set pin configuration for m26 device power on */
    if (m26->power_pin == -1 || m26->power_status_pin == -1)
    {
        return;
    }

    if (rt_pin_read(m26->power_status_pin) == PIN_HIGH)
    {
        return;
    }
    rt_pin_write(m26->power_pin, PIN_HIGH);

    while (rt_pin_read(m26->power_status_pin) == PIN_LOW)
    {
        rt_thread_mdelay(10);
    }
    rt_pin_write(m26->power_pin, PIN_LOW);
}

static void m26_power_off(struct at_device *device)
{
    struct at_device_m26 *m26 = RT_NULL;

    m26 = (struct at_device_m26 *) device->user_data;

    /* not nead to set pin configuration for m26 device power on */
    if (m26->power_pin == -1 || m26->power_status_pin == -1)
    {
        return;
    }

    if (rt_pin_read(m26->power_status_pin) == PIN_LOW)
    {
        return;
    }
    rt_pin_write(m26->power_pin, PIN_HIGH);

    while (rt_pin_read(m26->power_status_pin) == PIN_HIGH)
    {
        rt_thread_mdelay(10);
    }
    rt_pin_write(m26->power_pin, PIN_LOW);
}

/* =============================  m26 network interface operations ============================= */

/* set m26 network interface device status and address information */
static int m26_netdev_set_info(struct netdev *netdev)
{
#define M26_IEMI_RESP_SIZE      32
#define M26_IPADDR_RESP_SIZE    32
#define M26_DNS_RESP_SIZE       96
#define M26_INFO_RESP_TIMO      rt_tick_from_millisecond(300)

    int result = RT_EOK;
    ip_addr_t addr;
    at_response_t resp = RT_NULL;
    struct at_device *device = RT_NULL;
    struct at_client *client = RT_NULL;

    if (netdev == RT_NULL)
    {
        LOG_E("input network interface device is NULL.");
        return -RT_ERROR;
    }

    device = at_device_get_by_name(AT_DEVICE_NAMETYPE_NETDEV, netdev->name);
    if (device == RT_NULL)
    {
        LOG_E("get m26 deivce by netdev name failed.", netdev->name);
        return -RT_ERROR;
    }
    client = device->client;

    /* set network interface device up status */
    netdev_low_level_set_status(netdev, RT_TRUE);
    netdev_low_level_set_dhcp_status(netdev, RT_TRUE);

    resp = at_create_resp(M26_IEMI_RESP_SIZE, 0, M26_INFO_RESP_TIMO);
    if (resp == RT_NULL)
    {
        LOG_E("no memory for m26 device(%s) response structure.", netdev->name);
        result = -RT_ENOMEM;
        goto __exit;
    }

    /* set network interface device hardware address(IEMI) */
    {
        #define M26_NETDEV_HWADDR_LEN   8
        #define M26_IEMI_LEN            15

        char iemi[M26_IEMI_LEN] = {0};
        int i = 0, j = 0;

        /* send "AT+GSN" commond to get device IEMI */
        if (at_obj_exec_cmd(client, resp, "AT+GSN") < 0)
        {
            result = -RT_ERROR;
            goto __exit;
        }

        if (at_resp_parse_line_args(resp, 2, "%s", iemi) <= 0)
        {
            LOG_E("m26 device(%s) prase \"AT+GSN\" commands resposne data error.", device->name);
            result = -RT_ERROR;
            goto __exit;
        }

        LOG_D("m26 device(%s) IEMI number: %s", device->name, iemi);

        netdev->hwaddr_len = M26_NETDEV_HWADDR_LEN;
        /* get hardware address by IEMI */
        for (i = 0, j = 0; i < M26_NETDEV_HWADDR_LEN && j < M26_IEMI_LEN; i++, j+=2)
        {
            if (j != M26_IEMI_LEN - 1)
            {
                netdev->hwaddr[i] = (iemi[j] - '0') * 10 + (iemi[j + 1] - '0');
            }
            else
            {
                netdev->hwaddr[i] = (iemi[j] - '0');
            }
        }
    }

    /* set network interface device IP address */
    {
        #define IP_ADDR_SIZE_MAX    16
        char ipaddr[IP_ADDR_SIZE_MAX] = {0};
        
        at_resp_set_info(resp, M26_IPADDR_RESP_SIZE, 2, M26_INFO_RESP_TIMO);

        /* send "AT+QILOCIP" commond to get IP address */
        if (at_obj_exec_cmd(client, resp, "AT+QILOCIP") < 0)
        {
            result = -RT_ERROR;
            goto __exit;
        }

        if (at_resp_parse_line_args_by_kw(resp, ".", "%s", ipaddr) <= 0)
        {
            LOG_E("m26 device(%s) prase \"AT+QILOCIP\" commands resposne data error.", device->name);
            result = -RT_ERROR;
            goto __exit;
        }
        
        LOG_D("m26 device(%s) IP address: %s", device->name, ipaddr);

        /* set network interface address information */
        inet_aton(ipaddr, &addr);
        netdev_low_level_set_ipaddr(netdev, &addr);
    }

    /* set network interface device dns server */
    {
        #define DNS_ADDR_SIZE_MAX   16
        char dns_server1[DNS_ADDR_SIZE_MAX] = {0}, dns_server2[DNS_ADDR_SIZE_MAX] = {0};

        at_resp_set_info(resp, M26_DNS_RESP_SIZE, 0, M26_INFO_RESP_TIMO);

        /* send "AT+QIDNSCFG?" commond to get DNS servers address */
        if (at_obj_exec_cmd(client, resp, "AT+QIDNSCFG?") < 0)
        {
            result = -RT_ERROR;
            goto __exit;
        }

        if (at_resp_parse_line_args_by_kw(resp, "PrimaryDns:", "PrimaryDns:%s", dns_server1) <= 0 ||
                at_resp_parse_line_args_by_kw(resp, "SecondaryDns:", "SecondaryDns:%s", dns_server2) <= 0)
        {
            LOG_E("Prase \"AT+QIDNSCFG?\" commands resposne data error!");
            result = -RT_ERROR;
            goto __exit;
        }

        LOG_D("m26 device(%s) primary DNS server address: %s", device->name, dns_server1);
        LOG_D("m26 device(%s) secondary DNS server address: %s", device->name, dns_server2);

        inet_aton(dns_server1, &addr);
        netdev_low_level_set_dns_server(netdev, 0, &addr);

        inet_aton(dns_server2, &addr);
        netdev_low_level_set_dns_server(netdev, 1, &addr);
    }

__exit:
    if (resp)
    {
        at_delete_resp(resp);
    }
    
    return result;
}

static void check_link_status_entry(void *parameter)
{
#define M26_LINK_STATUS_OK   0
#define M26_LINK_RESP_SIZE   64
#define M26_LINK_RESP_TIMO   (3 * RT_TICK_PER_SECOND)
#define M26_LINK_DELAY_TIME  (30 * RT_TICK_PER_SECOND)

    struct netdev *netdev = (struct netdev *)parameter;
    struct at_device *device = RT_NULL;
    at_response_t resp = RT_NULL;
    int link_status;

    device = at_device_get_by_name(AT_DEVICE_NAMETYPE_NETDEV, netdev->name);
    if (device == RT_NULL)
    {
        LOG_E("get m26 deivce by netdev name failed.", netdev->name);
        return;
    }

    resp = at_create_resp(M26_LINK_RESP_SIZE, 0, M26_LINK_RESP_TIMO);
    if (resp == RT_NULL)
    {
        LOG_E("no memory for m26 device(%s) response object.", netdev->name);
        return;
    }

   while (1)
    { 
        
        /* send "AT+QNSTATUS" commond  to check netweork interface device link status */
        if (at_obj_exec_cmd(device->client, resp, "AT+QNSTATUS") < 0)
        {
            rt_thread_mdelay(M26_LINK_DELAY_TIME);

           continue;
        }
        
        link_status = -1;
        at_resp_parse_line_args_by_kw(resp, "+QNSTATUS:", "+QNSTATUS: %d", &link_status);

        /* check the network interface device link status  */
        if ((M26_LINK_STATUS_OK == link_status) != netdev_is_link_up(netdev))
        {
            netdev_low_level_set_link_status(netdev, (M26_LINK_STATUS_OK == link_status));
        }

        rt_thread_mdelay(M26_LINK_DELAY_TIME);
    }
}

static int m26_netdev_check_link_status(struct netdev *netdev)
{
#define M26_LINK_THREAD_TICK           20
#define M26_LINK_THREAD_STACK_SIZE     512
#define M26_LINK_THREAD_PRIORITY       (RT_THREAD_PRIORITY_MAX - 2)

    rt_thread_t tid;
    char tname[RT_NAME_MAX] = {0};

    if (netdev == RT_NULL)
    {
        LOG_E("input network interface device is NULL.");
        return -RT_ERROR;
    }

    rt_snprintf(tname, RT_NAME_MAX, "%s_link", netdev->name);

    tid = rt_thread_create(tname, check_link_status_entry, (void *)netdev, 
            M26_LINK_THREAD_STACK_SIZE, M26_LINK_THREAD_PRIORITY, M26_LINK_THREAD_TICK);
    if (tid)
    {
        rt_thread_startup(tid);
    }

    return RT_EOK;
}

static int m26_net_init(struct at_device *device);

static int m26_netdev_set_up(struct netdev *netdev)
{
    struct at_device *device = RT_NULL;

    device = at_device_get_by_name(AT_DEVICE_NAMETYPE_NETDEV, netdev->name);
    if (device == RT_NULL)
    {
        LOG_E("get m26 device by netdev name(%s) failed.", netdev->name);
        return -RT_ERROR;
    }

    if (device->is_init == RT_FALSE)
    {
        m26_net_init(device);
        device->is_init = RT_TRUE;

        netdev_low_level_set_status(netdev, RT_TRUE);
        LOG_D("the network interface device(%s) set up status.", netdev->name);
    }

    return RT_EOK;
}

static int m26_netdev_set_down(struct netdev *netdev)
{
    struct at_device *device = RT_NULL;

    device = at_device_get_by_name(AT_DEVICE_NAMETYPE_NETDEV, netdev->name);
    if (device == RT_NULL)
    {
        LOG_E("get m26 device by netdev name(%s) failed.", netdev->name);
        return -RT_ERROR;
    }

    if (device->is_init == RT_TRUE)
    {
        m26_power_off(device);
        device->is_init = RT_FALSE;

        netdev_low_level_set_status(netdev, RT_FALSE);
        LOG_D("the network interface device(%s) set down status.", netdev->name);
    }

    return RT_EOK;
}

static int m26_netdev_set_dns_server(struct netdev *netdev, uint8_t dns_num, ip_addr_t *dns_server)
{
#define M26_DNS_RESP_LEN    8
#define M26_DNS_RESP_TIMEO   rt_tick_from_millisecond(300)

    int result = RT_EOK;
    at_response_t resp = RT_NULL;
    struct at_device *device = RT_NULL;

    RT_ASSERT(netdev);
    RT_ASSERT(dns_server);

    device = at_device_get_by_name(AT_DEVICE_NAMETYPE_NETDEV, netdev->name);
    if (device == RT_NULL)
    {
        LOG_E("get m26 deivce by netdev name failed.", netdev->name);
        return - RT_ERROR;
    }

    resp = at_create_resp(M26_DNS_RESP_LEN, 0, M26_DNS_RESP_TIMEO);
    if (resp == RT_NULL)
    {
        LOG_E("no memory for m26 device(%s) response object.", netdev->name);
        return -RT_ENOMEM;
    }

    /* send "AT+QIDNSCFG=<pri_dns>[,<sec_dns>]" commond to set dns servers */
    if (at_exec_cmd(resp, "AT+QIDNSCFG=\"%s\"", inet_ntoa(*dns_server)) < 0)
    {
        result = -RT_ERROR;
        goto __exit;
    }

    netdev_low_level_set_dns_server(netdev, dns_num, dns_server);

__exit:
    if (resp)
    {
        at_delete_resp(resp);
    }
    return result;
}

#ifdef NETDEV_USING_PING
static int m26_netdev_ping(struct netdev *netdev, const char *host, 
            size_t data_len, uint32_t timeout, struct netdev_ping_resp *ping_resp)
{
#define M26_PING_RESP_SIZE       128
#define M26_PING_IP_SIZE         16
#define M26_PING_TIMEO           (5 * RT_TICK_PER_SECOND)

    int result = RT_EOK;
    at_response_t resp = RT_NULL;
    char ip_addr[M26_PING_IP_SIZE] = {0};
    int response, recv_data_len, time, ttl;
    struct at_device *device = RT_NULL;

    RT_ASSERT(netdev);
    RT_ASSERT(host);
    RT_ASSERT(ping_resp);

    device = at_device_get_by_name(AT_DEVICE_NAMETYPE_NETDEV, netdev->name);
    if (device == RT_NULL)
    {
        LOG_E("get m26 deivce by netdev name failed.", netdev->name);
        return - RT_ERROR;
    }

    resp = at_create_resp(M26_PING_RESP_SIZE, 5, M26_PING_TIMEO);
    if (resp == RT_NULL)
    {
        LOG_E("no memory for m26 device(%s) response object.", netdev->name);
        return  -RT_ENOMEM;
    }
    /* send "AT+QPING="<host>"[,[<timeout>][,<pingnum>]]" commond to send ping request */
    if (at_obj_exec_cmd(device->client, resp, "AT+QPING=\"%s\",%d,1", host, M26_PING_TIMEO / RT_TICK_PER_SECOND) < 0)
    {
        result = -RT_ERROR;
        goto __exit;
    }

    at_resp_parse_line_args_by_kw(resp, "+QPING:","+QPING:%d", &response);
    /* Received the ping response from the server */
    if (response == 0)
    {
        if (at_resp_parse_line_args_by_kw(resp, "+QPING:", "+QPING:%d,%[^,],%d,%d,%d",
                &response, ip_addr, &recv_data_len, &time, &ttl) <= 0)
        {
            result = -RT_ERROR;
            goto __exit;
        }   
    }

    /* prase response number */
    switch (response)
    {
    case 0:
        inet_aton(ip_addr, &(ping_resp->ip_addr));
        ping_resp->data_len = recv_data_len;
        ping_resp->ticks = time;
        ping_resp->ttl = ttl;
        result = RT_EOK;
        break;
    case 1:
        result = -RT_ETIMEOUT;
        break;
    default:
        result = -RT_ERROR;
        break;
    }

 __exit:
    if (resp)
    {
        at_delete_resp(resp);
    }

    return result;
}
#endif /* NETDEV_USING_PING */

#ifdef NETDEV_USING_NETSTAT
static void m26_netdev_netstat(struct netdev *netdev)
{ 
    // TODO netstat support
}
#endif /* NETDEV_USING_NETSTAT */

const struct netdev_ops m26_netdev_ops =
{
    m26_netdev_set_up,
    m26_netdev_set_down,

    RT_NULL, /* not support set ip, netmask, gatway address */
    m26_netdev_set_dns_server,
    RT_NULL, /* not support set DHCP status */

#ifdef NETDEV_USING_PING
    m26_netdev_ping,
#endif
#ifdef NETDEV_USING_NETSTAT
    m26_netdev_netstat,
#endif
};

static struct netdev *m26_netdev_add(const char *netdev_name)
{
#define M26_NETDEV_MTU       1500

    struct netdev *netdev = RT_NULL;

    RT_ASSERT(netdev_name);

    netdev = (struct netdev *) rt_calloc(1, sizeof(struct netdev));
    if (netdev == RT_NULL)
    {
        LOG_E("no memory for m26 device(%s) netdev structure.", netdev_name);
        return RT_NULL;
    }

    netdev->mtu = M26_NETDEV_MTU;
    netdev->ops = &m26_netdev_ops;

#ifdef SAL_USING_AT
    extern int sal_at_netdev_set_pf_info(struct netdev *netdev);
    /* set the network interface socket/netdb operations */
    sal_at_netdev_set_pf_info(netdev);
#endif

    netdev_register(netdev, netdev_name, RT_NULL);

    return netdev;
}

/* =============================  m26 device operations ============================= */

#define AT_SEND_CMD(client, resp, resp_line, timeout, cmd)                                         \
    do {                                                                                           \
        (resp) = at_resp_set_info((resp), 512, (resp_line), rt_tick_from_millisecond(timeout));    \
        if (at_obj_exec_cmd((client),(resp), (cmd)) < 0)                                           \
        {                                                                                          \
            result = -RT_ERROR;                                                                    \
            goto __exit;                                                                           \
        }                                                                                          \
    } while(0);                                                                                    \

/* init for m26 or mc20 */
static void m26_init_thread_entry(void *parameter)
{
#define INIT_RETRY                     5
#define CPIN_RETRY                     10
#define CSQ_RETRY                      10
#define CREG_RETRY                     10
#define CGREG_RETRY                    20

    at_response_t resp = RT_NULL;
    int i, qimux, qimode;
    int retry_num = INIT_RETRY;
    char parsed_data[10];
    rt_err_t result = RT_EOK;
    struct at_device *device = (struct at_device *)parameter;
    struct at_client *client = device->client;

    resp = at_create_resp(128, 0, rt_tick_from_millisecond(300));
    if (resp == RT_NULL)
    {
        LOG_E("no memory for m26 device(%s) response structure.", device->name);
        return;
    }

    LOG_D("start initializing the m26/mc20 device(%s)", device->name);

    while (retry_num--)
    {
        /* power on the m26 device */
        m26_power_on(device);
        rt_thread_mdelay(1000);

        /* wait m26|mc20 startup finish */
        if (at_client_obj_wait_connect(client, M26_WAIT_CONNECT_TIME))
        {
            result = -RT_ETIMEOUT;
            goto __exit;
        }
        
        /* disable echo */
        AT_SEND_CMD(client, resp, 0, 300, "ATE0");
        /* get module version */
        AT_SEND_CMD(client, resp, 0, 300, "ATI");
        /* show module version */
        for (i = 0; i < (int) resp->line_counts - 1; i++)
        {
            LOG_D("%s", at_resp_get_line(resp, i + 1));
        }
        /* check SIM card */
        for (i = 0; i < CPIN_RETRY; i++)
        {
            AT_SEND_CMD(client, resp, 2, 5 * RT_TICK_PER_SECOND, "AT+CPIN?");

            if (at_resp_get_line_by_kw(resp, "READY"))
            {
                LOG_D("m26 device(%s) SIM card detection success.", device->name);
                break;
            }
            rt_thread_mdelay(1000);
        }
        if (i == CPIN_RETRY)
        {
            LOG_E("SIM card detection failed!");
            result = -RT_ERROR;
            goto __exit;
        }
        /* waiting for dirty data to be digested */
        rt_thread_mdelay(10);
        /* check signal strength */
        for (i = 0; i < CSQ_RETRY; i++)
        {
            AT_SEND_CMD(client, resp, 0, 300, "AT+CSQ");
            at_resp_parse_line_args_by_kw(resp, "+CSQ:", "+CSQ: %s", &parsed_data);
            if (rt_strncmp(parsed_data, "99,99", sizeof(parsed_data)))
            {
                LOG_D("m26 device(%s) signal strength: %s", device->name, parsed_data);
                break;
            }
            rt_thread_mdelay(1000);
        }
        if (i == CSQ_RETRY)
        {
            LOG_E("m26 device(%s) signal strength check failed(%s).", device->name, parsed_data);
            result = -RT_ERROR;
            goto __exit;
        }
        /* check the GSM network is registered */
        for (i = 0; i < CREG_RETRY; i++)
        {
            AT_SEND_CMD(client, resp, 0, 300, "AT+CREG?");
            at_resp_parse_line_args_by_kw(resp, "+CREG:", "+CREG: %s", &parsed_data);
            if (!rt_strncmp(parsed_data, "0,1", sizeof(parsed_data)) ||
                    !rt_strncmp(parsed_data, "0,5", sizeof(parsed_data)))
            {
                LOG_D("m26 device(%s) GSM network is registered(%s).", device->name, parsed_data);
                break;
            }
            rt_thread_mdelay(1000);
        }
        if (i == CREG_RETRY)
        {
            LOG_E("m26 device(%s) GSM network is register failed(%s)", device->name, parsed_data);
            result = -RT_ERROR;
            goto __exit;
        }
        /* check the GPRS network is registered */
        for (i = 0; i < CGREG_RETRY; i++)
        {
            AT_SEND_CMD(client, resp, 0, 300, "AT+CGREG?");
            at_resp_parse_line_args_by_kw(resp, "+CGREG:", "+CGREG: %s", &parsed_data);
            if (!rt_strncmp(parsed_data, "0,1", sizeof(parsed_data)) ||
                    !rt_strncmp(parsed_data, "0,5", sizeof(parsed_data)))
            {
                LOG_D("m26 device(%s) GPRS network is registered(%s).", device->name, parsed_data);
                break;
            }
            rt_thread_mdelay(1000);
        }
        if (i == CGREG_RETRY)
        {
            LOG_E("m26 device(%s) GPRS network is register failed(%s).", device->name, parsed_data);
            result = -RT_ERROR;
            goto __exit;
        }

        AT_SEND_CMD(client, resp, 0, 300, "AT+QIFGCNT=0");
        AT_SEND_CMD(client, resp, 0, 300, "AT+QICSGP=1, \"CMNET\"");
        AT_SEND_CMD(client, resp, 0, 300, "AT+QIMODE?");

        at_resp_parse_line_args_by_kw(resp, "+QIMODE:", "+QIMODE: %d", &qimode);
        if (qimode == 1)
        {
            AT_SEND_CMD(client, resp, 0, 300, "AT+QIMODE=0");
        }

        /* the device default response timeout is 40 seconds, but it set to 15 seconds is convenient to use. */
        AT_SEND_CMD(client, resp, 2, 20 * 1000, "AT+QIDEACT");

        /* Set to multiple connections */
        AT_SEND_CMD(client, resp, 0, 300, "AT+QIMUX?");
        at_resp_parse_line_args_by_kw(resp, "+QIMUX:", "+QIMUX: %d", &qimux);
        if (qimux == 0)
        {
            AT_SEND_CMD(client, resp, 0, 300, "AT+QIMUX=1");
        }

        AT_SEND_CMD(client, resp, 0, 300, "AT+QIREGAPP");

        /* the device default response timeout is 150 seconds, but it set to 20 seconds is convenient to use. */
        AT_SEND_CMD(client, resp, 0, 20 * 1000, "AT+QIACT");

        AT_SEND_CMD(client, resp, 2, 300, "AT+QILOCIP");
				
				AT_SEND_CMD(client, resp, 0, 300, "AT+QGNSSC?");

        at_resp_parse_line_args_by_kw(resp, "+QGNSSC:", "+QGNSSC: %d", &qimode);
        if (qimode == 0)
        {
            AT_SEND_CMD(client, resp, 0, 300, "AT+QGNSSC=1");
        }
				
        result = RT_EOK;


    __exit:
        if (result == RT_EOK)
        {
            break;
        }
        else
        {
            /* power off the m26 device */
            m26_power_off(device);
            rt_thread_mdelay(1000);

            LOG_I("m26 device(%s) initialize retry...", device->name);
        }    
    }

    if (resp)
    {
        at_delete_resp(resp);
    }

    if (result == RT_EOK)
    {
        m26_netdev_set_info(device->netdev);
        m26_netdev_check_link_status(device->netdev);

        LOG_I("m26 device(%s) network initialize successfully.", device->name);
    }
    else
    {
        LOG_E("m26 device(%s) network initialize failed(%d).", device->name, result);
    }
}

static int m26_net_init(struct at_device *device)
{
#ifdef AT_DEVICE_M26_INIT_ASYN
    rt_thread_t tid;

    tid = rt_thread_create("m26_net_init", m26_init_thread_entry, (void *)device, 
            M26_THREAD_STACK_SIZE, M26_THREAD_PRIORITY, 20);
    if (tid)
    {
        rt_thread_startup(tid);
    }
    else
    {
        LOG_E("create m26 device(%s) initialization thread failed.", device->name);
        return -RT_ERROR;
    }
#else
    m26_init_thread_entry(device);
#endif /* AT_DEVICE_M26_INIT_ASYN */
    
    return RT_EOK;
}

static void urc_func(struct at_client *client, const char *data, rt_size_t size)
{
    RT_ASSERT(data);

    LOG_I("URC data : %.*s", size, data);
}

static const struct at_urc urc_table[] = {
        {"RING",        "\r\n",                 urc_func},
        {"Call Ready",  "\r\n",                 urc_func},
        {"RDY",         "\r\n",                 urc_func},
        {"NO CARRIER",  "\r\n",                 urc_func},
};

static int m26_init(struct at_device *device)
{
    struct at_device_m26 *m26 = (struct at_device_m26 *) device->user_data;

    /* initialize AT client */
    at_client_init(m26->client_name, m26->recv_line_num);

    device->client = at_client_get(m26->client_name);
    if (device->client == RT_NULL)
    {
        LOG_E("m26 device(%s) initialize failed, get AT client(%s) failed.", m26->device_name, m26->client_name);
        return -RT_ERROR;
    }
    
    /* register URC data execution function  */
    at_obj_set_urc_table(device->client, urc_table, sizeof(urc_table) / sizeof(urc_table[0]));
    
#ifdef AT_USING_SOCKET
    m26_socket_init(device);
#endif

    /* add m26 netdev to the netdev list */
    device->netdev = m26_netdev_add(m26->device_name);
    if (device->netdev == RT_NULL)
    {
        LOG_E("m26 device(%s) initialize failed, get network interface device failed.", m26->device_name);
        return -RT_ERROR;
    }
    
    /* initialize m26 pin configuration */
    if (m26->power_pin != -1 && m26->power_status_pin != -1)
    {
        rt_pin_mode(m26->power_pin, PIN_MODE_OUTPUT);
        rt_pin_mode(m26->power_status_pin, PIN_MODE_INPUT);
    }
    
    /* initialize m26 device network */
    return m26_netdev_set_up(device->netdev);
}

static int m26_deinit(struct at_device *device)
{
    return m26_netdev_set_down(device->netdev);
}


///* read GPS info */
static int m26_read_GPS(struct at_device *device, struct gps *info)
{
    int result = RT_EOK;
    struct at_response *resp = RT_NULL;
		nmea_msg gps;
    resp = at_create_resp(256, 0, 20 * RT_TICK_PER_SECOND);
    if (resp == RT_NULL)
    {
        LOG_E("no memory for m26 device(%s) response structure.", device->name);
        return -RT_ENOMEM;
    }
		
		AT_SEND_CMD(device->client, resp, 5, 300, "AT+QGNSSRD?");
		
    LOG_D("device: %s data: %s\r\n", device->name, at_resp_get_line(resp,1));
		LOG_D("device: %s data: %s\r\n", device->name, at_resp_get_line(resp,2));
		LOG_D("device: %s data: %s\r\n", device->name, at_resp_get_line(resp,3));
		LOG_D("device: %s data: %s\r\n", device->name, at_resp_get_line(resp,4));
		LOG_D("device: %s data: %s\r\n", device->name, at_resp_get_line(resp,5));
//		LOG_D("device: %s data: %s\r\n", device->name, at_resp_get_line(resp,6));
//		LOG_D("device: %s data: %s\r\n", device->name, at_resp_get_line(resp,7));
//		LOG_D("device: %s data: %s\r\n", device->name, at_resp_get_line(resp,8));
//		LOG_D("device: %s data: %s\r\n", device->name, at_resp_get_line(resp,9));
//		LOG_D("device: %s data: %s\r\n", device->name, at_resp_get_line(resp,10));
//		LOG_D("device: %s data: %s\r\n", device->name, at_resp_get_line(resp,11));
//		LOG_D("device: %s data: %s\r\n", device->name, at_resp_get_line(resp,12));
//		LOG_D("device: %s data: %s\r\n", device->name, at_resp_get_line(resp,13));
//		LOG_D("device: %s data: %s\r\n", device->name, at_resp_get_line(resp,14));
//		LOG_D("device: %s data: %s\r\n", device->name, at_resp_get_line(resp,15));
//		LOG_D("device: %s data: %s\r\n", device->name, at_resp_get_line(resp,3));
		
		NMEA_GNRMC_Analysis(&gps,(uint8_t *)at_resp_get_line(resp,2));
		NMEA_GNVTG_Analysis(&gps,(uint8_t *)at_resp_get_line(resp,3));	//GPVTG解析
		NMEA_GNGGA_Analysis(&gps,(uint8_t *)at_resp_get_line(resp,4));	//GNGGA解析 
		
//		NMEA_GPGSV_Analysis(&gps,buf);	//GPGSV解析
//		NMEA_BDGSV_Analysis(&gps,buf);	//BDGSV解析
			
//		NMEA_GNGSA_Analysis(&gps,buf);	//GPNSA解析
//		NMEA_GNRMC_Analysis(&gps,buf);	//GPNMC解析
		LOG_D("Longitude:%d",gps.longitude);
		LOG_D("Latitude:%d",gps.latitude);
		LOG_D("Speed:%d km/h",gps.speed);
		LOG_D("GPS+BD Valid satellite:%02d",gps.posslnum);
		LOG_D("GPS Visible satellite:%02d",gps.svnum%100);

//    if (  != RT_EOK)
//    {
//        LOG_E("esp8266 device(%s) wifi connect failed, check ssid(%s) and password(%s).",  
//                device->name, info->ssid, info->password);
//        result = -RT_ERROR;
//    }

 __exit:
    if (resp)
    {
        at_delete_resp(resp);
    }

    return result;
}
/* read signl info */
static int m26_get_signal(struct at_device *device, struct m26_info *info)
{
    int result = RT_EOK;
    struct at_response *resp = RT_NULL;
		int i = 0;
	  char parsed_data[10];
    resp = at_create_resp(128, 0, 20 * RT_TICK_PER_SECOND);
    if (resp == RT_NULL)
    {
        LOG_E("no memory for m26 device(%s) response structure.", device->name);
        return -RT_ENOMEM;
    }
				 /* check signal strength */
        for (i = 0; i < CSQ_RETRY; i++)
        {
            AT_SEND_CMD(device->client, resp, 0, 300, "AT+CSQ");
            at_resp_parse_line_args_by_kw(resp, "+CSQ:", "+CSQ: %s", &parsed_data);
            if (rt_strncmp(parsed_data, "99,99", sizeof(parsed_data)))
            {
                LOG_D("m26 device(%s) signal strength: %s", device->name, parsed_data);
                break;
            }
            rt_thread_mdelay(1000);
        }
        if (i == CSQ_RETRY)
        {
            LOG_E("m26 device(%s) signal strength check failed(%s).", device->name, parsed_data);
            result = -RT_ERROR;
            goto __exit;
        }	
__exit:
    if (resp)
    {
        at_delete_resp(resp);
    }
		
    return result;
}

/* read sim info */
static int m26_get_sim_info(struct at_device *device, struct m26_info *info)
{
    int result = RT_EOK;
    struct at_response *resp = RT_NULL;

    resp = at_create_resp(128, 0, 20 * RT_TICK_PER_SECOND);
    if (resp == RT_NULL)
    {
        LOG_E("no memory for m26 device(%s) response structure.", device->name);
        return -RT_ENOMEM;
    }
    
				#define M26_IEMI_LEN            15
				#define M26_CCID_LEN            20
				#define M26_IMSI_LEN            15

        char iemi[M26_IEMI_LEN] = {0};
				char ccid[M26_CCID_LEN] = {0};
				char imsi[M26_IMSI_LEN] = {0};

        /* send "AT+GSN" commond to get device IEMI */
        if (at_obj_exec_cmd(device->client, resp, "AT+GSN") < 0)
        {
            result = -RT_ERROR;
            goto __exit;
        }

        if (at_resp_parse_line_args(resp, 2, "%s", iemi) <= 0)
        {
            LOG_E("m26 device(%s) prase \"AT+GSN\" commands resposne data error.", device->name);
            result = -RT_ERROR;
            goto __exit;
        }

        LOG_D("m26 device(%s) IEMI number: %s", device->name, iemi);
				
				/* send "AT+QCCID" commond to get device CCID */
        if (at_obj_exec_cmd(device->client, resp, "AT+QCCID") < 0)
        {
            result = -RT_ERROR;
            goto __exit;
        }

        if (at_resp_parse_line_args(resp, 2, "%s", ccid) <= 0)
        {
            LOG_E("m26 device(%s) prase \"AT+QCCID\" commands resposne data error.", device->name);
            result = -RT_ERROR;
            goto __exit;
        }

        LOG_D("m26 device(%s) QCCID number: %s", device->name, ccid);
				
				/* send "AT+CIMI" commond to get device imsi */
        if (at_obj_exec_cmd(device->client, resp, "AT+CIMI") < 0)
        {
            result = -RT_ERROR;
            goto __exit;
        }

        if (at_resp_parse_line_args(resp, 2, "%s", imsi) <= 0)
        {
            LOG_E("m26 device(%s) prase \"AT+QCIMI\" commands resposne data error.", device->name);
            result = -RT_ERROR;
            goto __exit;
        }

        LOG_D("m26 device(%s) IMSI number: %s", device->name, imsi);
    		
__exit:
    if (resp)
    {
        at_delete_resp(resp);
    }
		
    return result;
}


static int m26_control(struct at_device *device, int cmd, void *arg)
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
				m26_get_signal(device,(struct m26_info *)arg);
				break;
    case AT_DEVICE_CTRL_GET_GPS:
				m26_read_GPS(device,(struct gps *)arg);
				break;
		case AT_DEVICE_CTRL_GET_SIM:
				m26_get_sim_info(device,(struct m26_info *)arg);
				break;
    case AT_DEVICE_CTRL_GET_VER:
        LOG_W("m26 not support the control command(%d).", cmd);
        break;
    default:
        LOG_E("input error control command(%d).", cmd);
        break;
    }

    return result;
}
static const struct at_device_ops m26_device_ops = 
{
    m26_init,
    m26_deinit,
    m26_control,
};

static int m26_device_class_register(void)
{
    struct at_device_class *class = RT_NULL;

    class = (struct at_device_class *) rt_calloc(1, sizeof(struct at_device_class));
    if (class == RT_NULL)
    {
        LOG_E("no memory for m26 device class create.");
        return -RT_ENOMEM;
    }

    /* fill m26 device class object */
#ifdef AT_USING_SOCKET
    m26_socket_class_register(class);
#endif
    class->device_ops = &m26_device_ops;

    return at_device_class_register(class, AT_DEVICE_CLASS_M26_MC20);
}
INIT_DEVICE_EXPORT(m26_device_class_register);

#endif /* AT_DEVICE_USING_M26 */

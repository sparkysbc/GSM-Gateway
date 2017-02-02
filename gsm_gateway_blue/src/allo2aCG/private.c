/*
 * OpenVox A24xx FXS/FXO Interface Driver for Zapata Telephony interface
 *
 * Written by MiaoLin<miaolin@openvox.cn>
 * Written by mark.liu<mark.liu@openvox.cn>
 * $Id: private.c 446 2011-05-12 04:01:57Z liuyuan $
 *
 * Copyright (C) 2005-2010 OpenVox Communication Co. Ltd,
 *
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <linux/kernel.h>
#include "../allospi/allospi.h"
#include <linux/delay.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,26)
#include <linux/semaphore.h>
#else
#include <asm/semaphore.h>
#endif


int debugsem = 0;
struct semaphore spisem;

#define ZT_CHUNKSIZE			8
#define ZT_MIN_CHUNKSIZE		ZT_CHUNKSIZE
#define ZT_DEFAULT_CHUNKSIZE	ZT_CHUNKSIZE
#define ZT_MAX_CHUNKSIZE		ZT_CHUNKSIZE

#define MAX_NUM_CARDS 4

/* Platform SPI module commn flags */
#define GWSPI_TDM_DEV_NUM	0
#define GWSPI_REG_DEV_NUM	0

#define GWSPI_TDM_WRITE		1
#define GWSPI_TDM_READ		2
#define GWSPI_REG_WRITE		3
#define GWSPI_REG_READ		4
#define GWSPI_TDM_READWRITE	6
#define GWSPI_SIG_READ		7

#define GWSPI_SIG_GSM1          1
#define GWSPI_GSM_PKT           4
#define GWSPI_GSM_CTRL          8
#define GWSPI_DRV_RST           9
#define GWSPI_MS_IRQ            10
#define GWSPI_DRV_RUN           11
#define GWSPI_FRM_VER           12
#define GWSPI_CONTROL		15	/* [0-3] gsm module uart debug enable*/ /* Bit-5 media with pattern*//*[4-7] gsm module reset make high for some duration then low*/

void __g4_outl__(unsigned int regno, unsigned char value, int flag)
{
        u8 cmd,data;
        int spidev_num=0;

        switch(flag) {
                case GWSPI_TDM_WRITE:
                        cmd = GWSPI_TDM_WRITE;
                        spidev_num = GWSPI_TDM_DEV_NUM;
                        break;
                case GWSPI_REG_WRITE:
                        cmd = GWSPI_REG_WRITE;
                        spidev_num = GWSPI_REG_DEV_NUM;
                        break;
                default:
                        cmd = 0;
                        break;
        }
        cmd |= ( ((regno)&0x1f) << 3);
        data = value&0xff;

        down(&spisem);
        allo_spi_write_direct(cmd, spidev_num);
        allo_spi_write_direct(data, spidev_num);
        up(&spisem);
}

unsigned char __g4_inl__( unsigned int regno, int flag)
{
        u8 cmd= 0,data;
        int spidev_num=0;
        switch(flag) {
                case GWSPI_TDM_READ:
                        cmd = GWSPI_TDM_READ;
                        spidev_num = GWSPI_TDM_DEV_NUM;
                        break;
                case GWSPI_REG_READ:
                        cmd = GWSPI_REG_READ;
                        spidev_num = GWSPI_REG_DEV_NUM;
                        break;
                default:
                        break;
        }
        cmd |= ( ((regno)&0x1f) << 3);

        down(&spisem);
        allo_spi_write_direct(cmd, spidev_num);
        data = allo_spi_read_direct(spidev_num);
        up(&spisem);

return data;
}

void init_interrupt_deps(void){
        sema_init(&spisem, 1);
}

void init_fpga(int ms_per_irq){
	unsigned int fversion;

	__g4_outl__(GWSPI_DRV_RUN,0x01,GWSPI_REG_WRITE);
        __g4_outl__(GWSPI_MS_IRQ,ms_per_irq,GWSPI_REG_WRITE);//1ms intx
        printk("Miliseconds per IRQ: %x\n", __g4_inl__(GWSPI_MS_IRQ, GWSPI_REG_READ));
	fversion = __g4_inl__(GWSPI_FRM_VER, GWSPI_REG_READ);
        printk("FPGA Firmware Version: %02x.%02x\n", ((0xF0&fversion)>>4), (0x0F&fversion) );
}

void stop_fpga(void){
	printk("%s %d\n", __func__, __LINE__); 
        __g4_outl__(GWSPI_MS_IRQ,0,GWSPI_REG_WRITE);//1ms intx
	__g4_outl__(GWSPI_DRV_RUN,0x00,GWSPI_REG_WRITE);
        __g4_outl__(GWSPI_DRV_RST,0x00,GWSPI_REG_WRITE);
}

static void __gw_spi_write_read_pcm(u8 **txbuf, u8 **rxbuf, unsigned int size, unsigned int flag)
{
	u8 cmd;
	int spidev_num;

	cmd = GWSPI_TDM_READWRITE;
	spidev_num = GWSPI_TDM_DEV_NUM;
	allo_spi_write_direct(cmd, spidev_num);
	allo_spi_write_read(txbuf, rxbuf, size, spidev_num);

return ;
}

static unsigned int __gw_spi_write_signaling(u8 *addr, unsigned int size, unsigned int regno, unsigned char flag)
{
	u8 cmd = 0;
	int spidev_num;

	cmd = flag;
	spidev_num = GWSPI_TDM_DEV_NUM;
	cmd |= ( (regno&0x1f) << 3);
	allo_spi_write_direct(cmd, spidev_num);
	allo_spi_write(addr, size, spidev_num);
return 0;
}

static unsigned int __gw_spi_read_signaling(u8 *addr, unsigned int size, unsigned int regno, unsigned char flag)
{
	u8 cmd = 0;
	int spidev_num;

	cmd = flag;
	spidev_num = GWSPI_TDM_DEV_NUM;
	cmd |= ( (regno&0x1f) << 3);
	allo_spi_write_direct(cmd, spidev_num);
	allo_spi_read(addr, size, spidev_num);
return 0;
}

void __allo_gsm_transmit(unsigned long mem32, unsigned char *writechunk, unsigned char **txbuf,unsigned int irq_frq , unsigned int order)
{
	*txbuf = writechunk + ZT_CHUNKSIZE * MAX_NUM_CARDS * irq_frq + ZT_CHUNKSIZE * MAX_NUM_CARDS * order;
}

void __allo_gsm_receive(unsigned long mem32, unsigned char *readchunk, unsigned char **rxbuf,unsigned int irq_frq , unsigned int order)
{	
	*rxbuf = readchunk + ZT_CHUNKSIZE * MAX_NUM_CARDS * irq_frq + ZT_CHUNKSIZE * MAX_NUM_CARDS * order;
}

unsigned int __allo_gsm_signaling_write(u8 *txbuf, unsigned int size, unsigned int regno)
{
	unsigned int res;
	if(debugsem)printk("down 8");
	down(&spisem);
	res = __gw_spi_write_signaling(txbuf , size, regno, GWSPI_REG_WRITE);
	up(&spisem);
	if(debugsem)printk("up ");
return res; 
}

unsigned int __allo_gsm_signaling_read(u8 *rxbuf, unsigned int size, unsigned int regno)
{
	unsigned int res;
	if(debugsem)printk("down 8");
	down(&spisem);
	res = __gw_spi_read_signaling(rxbuf , size, regno, GWSPI_SIG_READ);
	up(&spisem);
	if(debugsem)printk("up ");
return res; 
}

unsigned int __allo_gsm_pcm_write_read(unsigned char **txbuf, unsigned char **rxbuf, unsigned int size){
	unsigned int res=0;
	if(debugsem)printk("down 5");
	down(&spisem);
	__gw_spi_write_read_pcm(txbuf , rxbuf, size, GWSPI_TDM_READWRITE);
	up(&spisem);
	if(debugsem)printk("up ");
return res; 
}

struct device *  __allo_gsm_get_spidev(unsigned long mem32) 
{
	return allo_spi_get_dev(GWSPI_TDM_DEV_NUM);
}

void __allo_gsm_set_chunk(void *readchunk, void *writechunk,unsigned int frq) 
{
	unsigned char *tmp;
	tmp =  *((unsigned char **)(writechunk)) + (frq * ZT_MAX_CHUNKSIZE * (MAX_NUM_CARDS) * 4);	/* in bytes */
	*(char **)readchunk = tmp;
}

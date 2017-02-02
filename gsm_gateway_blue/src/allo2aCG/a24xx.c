/*
 * OpenVox A24xx FXS/FXO Interface Driver for Zapata Telephony interface
 *
 * Written by MiaoLin<miaolin@openvox.cn>
 * $Id: a24xx.c 185 2010-12-14 07:58:51Z yangshugang $
 
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
 
#include <linux/spinlock.h>
#include <linux/jiffies.h>

#include <dahdi/kernel.h>

#if 0
#include "base.h"
#include "proslic.h"
#endif
#include "allo2aCG.h"

#define BIT_EC_PRESENT      (1<<0)

#define VPM_DEFAULT_DTMFTHRESHOLD 1000
#if 0
/* indirect_resg */
static alpha  indirect_regs[] =
{
{0,255,"DTMF_ROW_0_PEAK",0x55C2},
{1,255,"DTMF_ROW_1_PEAK",0x51E6},
{2,255,"DTMF_ROW2_PEAK",0x4B85},
{3,255,"DTMF_ROW3_PEAK",0x4937},
{4,255,"DTMF_COL1_PEAK",0x3333},
{5,255,"DTMF_FWD_TWIST",0x0202},
{6,255,"DTMF_RVS_TWIST",0x0202},
{7,255,"DTMF_ROW_RATIO_TRES",0x0198},
{8,255,"DTMF_COL_RATIO_TRES",0x0198},
{9,255,"DTMF_ROW_2ND_ARM",0x0611},
{10,255,"DTMF_COL_2ND_ARM",0x0202},
{11,255,"DTMF_PWR_MIN_TRES",0x00E5},
{12,255,"DTMF_OT_LIM_TRES",0x0A1C},
{13,0,"OSC1_COEF",0x7B30},
{14,1,"OSC1X",0x0063},
{15,2,"OSC1Y",0x0000},
{16,3,"OSC2_COEF",0x7870},
{17,4,"OSC2X",0x007D},
{18,5,"OSC2Y",0x0000},
{19,6,"RING_V_OFF",0x0000},
{20,7,"RING_OSC",0x7EF0},
{21,8,"RING_X",0x0160},
{22,9,"RING_Y",0x0000},
{23,255,"PULSE_ENVEL",0x2000},
{24,255,"PULSE_X",0x2000},
{25,255,"PULSE_Y",0x0000},
//{26,13,"RECV_DIGITAL_GAIN",0x4000},	// playback volume set lower
{26,13,"RECV_DIGITAL_GAIN",0x2000},	// playback volume set lower
{27,14,"XMIT_DIGITAL_GAIN",0x4000},
//{27,14,"XMIT_DIGITAL_GAIN",0x2000},
{28,15,"LOOP_CLOSE_TRES",0x1000},
{29,16,"RING_TRIP_TRES",0x3600},
{30,17,"COMMON_MIN_TRES",0x1000},
{31,18,"COMMON_MAX_TRES",0x0200},
{32,19,"PWR_ALARM_Q1Q2",0x0ff4},
{33,20,"PWR_ALARM_Q3Q4",0x6e7e},
{34,21,"PWR_ALARM_Q5Q6",0x0ff4},
{35,22,"LOOP_CLOSURE_FILTER",0x8000},
{36,23,"RING_TRIP_FILTER",0x0320},
{37,24,"TERM_LP_POLE_Q1Q2",0x0012},
{38,25,"TERM_LP_POLE_Q3Q4",0x0012},
{39,26,"TERM_LP_POLE_Q5Q6",0x0012},
{40,27,"CM_BIAS_RINGING",0x0C00},
{41,64,"DCDC_MIN_V",0x0C00},
{42,255,"DCDC_XTRA",0x1000},
{43,66,"LOOP_CLOSE_TRES_LOW",0x1000},
};

void __a24xx_wait_just_a_bit(int foo)
{
	long newjiffies;
	newjiffies = jiffies + foo;
	while(jiffies < newjiffies);
}

void __a24xx_setcard(void *wc_dev, int card)
{
	struct a24xx_dev *dev = (struct a24xx_dev *)(wc_dev);
//	if (dev->curcard != card) 
	{
		__opvx_a24xx_setcard(dev->mem32, card);
		dev->curcard = card;
	}
}

inline void __a24xx_spi_setreg(struct a24xx_dev *wc_dev, int card, unsigned char reg, unsigned char value)
{
	__opvx_a24xx_spi_setreg((void *)wc_dev, wc_dev->mem32, card, wc_dev->modtype[card], reg, value, __a24xx_setcard);
}

inline unsigned char __a24xx_spi_getreg(struct a24xx_dev *wc_dev, int card, unsigned char reg)
{
	return __opvx_a24xx_spi_getreg((void *)wc_dev, wc_dev->mem32, card, wc_dev->modtype[card], reg, __a24xx_setcard);
}
#endif
int __a24xx_malloc_chunk(struct g4 *wc,unsigned int frq) 
{
	__opvx_a24xx_set_chunk(&(wc->readchunk), &(wc->writechunk),frq);
	wc->readdma = wc->writedma + frq * DAHDI_MAX_CHUNKSIZE * (MAX_NUM_CARDS) * 2;

	return 0;
}

#if 0
static unsigned char __translate_3215(unsigned char address)
{
	int x;
	for (x=0;x<sizeof(indirect_regs)/sizeof(indirect_regs[0]);x++) {
		if (indirect_regs[x].address == address) {
			address = indirect_regs[x].altaddr;
			break;
		}
	}
	return address;
}

int __a24xx_wait_access(struct a24xx_dev *wc_dev, int card)
{
	unsigned char data = 0;
	long origjiffies;
	int count = 0;

	#define MAX 6000 /* attempts */

	origjiffies = jiffies;
	/* Wait for indirect access */
	while (count++ < MAX) {
		data = __a24xx_spi_getreg(wc_dev, card, I_STATUS);
		if (!data) {
			return 0;
		}
	}

	if(count > (MAX-1)) {
		printk(" ##### Loop error (%02x) #####\n", data);
	}

	return 0;
}
#endif
#if 0
int __a24xx_proslic_setreg_indirect(struct a24xx_dev *wc_dev, int card, unsigned char address, unsigned short data)
{
	int res = -1;
	/* Translate 3215 addresses */
	if (wc_dev->flags[card] & FLAG_3215) {
		address = __translate_3215(address);
		if (address == 255) {
			return 0;
		}
	}
	if(!__a24xx_wait_access(wc_dev, card)) {
		__a24xx_spi_setreg(wc_dev, card, IDA_LO,(unsigned char)(data & 0xFF));
		__a24xx_spi_setreg(wc_dev, card, IDA_HI,(unsigned char)((data & 0xFF00)>>8));
		__a24xx_spi_setreg(wc_dev, card, IAA,address);
		res = 0;
	}
	
	return res;
}

int __a24xx_proslic_getreg_indirect(struct a24xx_dev *wc_dev, int card, unsigned char address)
{
	int res = -1;
	char *p=NULL;
	/* Translate 3215 addresses */
	if (wc_dev->flags[card] & FLAG_3215) {
		address = __translate_3215(address);
		if (address == 255) {
			return 0;
		}
	}
	if (!__a24xx_wait_access(wc_dev, card)) {
		__a24xx_spi_setreg(wc_dev, card, IAA, address);
		if (!__a24xx_wait_access(wc_dev, card)) {
			unsigned char data1, data2;
			data1 = __a24xx_spi_getreg(wc_dev, card, IDA_LO);
			data2 = __a24xx_spi_getreg(wc_dev, card, IDA_HI);
			res = data1 | (data2 << 8);
		} else {
			p = "Failed to wait inside\n";
		}
	} else {
		p = "failed to wait\n";
	}
	if (p) {
		printk("%s\n", p);
	}
	return res;
}
void __a24xx_vpm_setpresent(struct a24xx_dev *wc_dev)
{
	wc_dev->vpm = BIT_EC_PRESENT;
    if  ( (wc_dev->fwversion&0xffff) > 0x3)
	    __opvx_a24xx_vpm_setpresent_v2(wc_dev->mem32);
	else 
	    __opvx_a24xx_vpm_setpresent(wc_dev->mem32);
	printk("OpenVox VPM: Present and operational servicing %d span(s)\n", 1/*wc_dev->numspans*/);
}

void a24xx_spi_setreg(struct a24xx_dev *wc_dev, int card, unsigned char reg, unsigned char value)
{
	printk("setreg card:%d reg:%d val:%d \n",card,reg,value);
	//unsigned long flags;
	//spin_lock_irqsave(&wc_dev->lock, flags);
	__a24xx_spi_setreg(wc_dev, card, reg, value);
	//spin_unlock_irqrestore(&wc_dev->lock);
}

unsigned char a24xx_spi_getreg(struct a24xx_dev *wc_dev, int card, unsigned char reg)
{
	//unsigned long flags;
	unsigned char ret;
	//spin_lock_irqsave(&wc_dev->lock, flags);
	ret = __a24xx_spi_getreg(wc_dev, card, reg);
	//spin_unlock_irqrestore(&wc_dev->lock, flags);
	return ret;
}

static inline unsigned int a24xx_oct_in(struct a24xx_dev *wc_dev, const unsigned int addr)
{
	unsigned long flags;
	unsigned int ret;

	spin_lock_irqsave(&wc_dev->lock, flags);
    if  ( (wc_dev->fwversion&0xffff) > 0x3)
	    ret = __opvx_a24xx_oct_in_v2(wc_dev->mem32, addr);
	else 
	    ret = __opvx_a24xx_oct_in(wc_dev->mem32, addr);
	spin_unlock_irqrestore(&wc_dev->lock, flags);
	
	return ret;
}

static inline void a24xx_oct_out(struct a24xx_dev *wc_dev, const unsigned int addr, const unsigned int value)
{
	unsigned long flags;

	spin_lock_irqsave(&wc_dev->lock, flags);
    if  ( (wc_dev->fwversion&0xffff) > 0x3)
	    __opvx_a24xx_oct_out_v2(wc_dev->mem32, addr, value);
	else 
	    __opvx_a24xx_oct_out(wc_dev->mem32, addr, value);
	spin_unlock_irqrestore(&wc_dev->lock, flags);
}

void oct_set_reg(void *data, unsigned int reg, unsigned int val)
{
	struct a24xx_dev *wc_dev = data;
	a24xx_oct_out(wc_dev, reg, val);
}

unsigned int oct_get_reg(void *data, unsigned int reg)
{
	struct a24xx_dev *wc_dev = data;
	unsigned int ret;
	ret = a24xx_oct_in(wc_dev, reg);
	return ret;
}

void a24xx_reset_spi(struct a24xx_dev *wc_dev, int card)
{
	unsigned long flags;
	//spin_lock_irqsave(&wc_dev->lock, flags);
	__opvx_a24xx_reset_spi(wc_dev, card, __a24xx_setcard);
	//spin_unlock_irqrestore(&wc_dev->lock, flags);
}

#endif

/*
 * ALLO G4 GSM Interface Driver for DAHDI Telephony interface
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2 as published by the
 * Free Software Foundation. See the LICENSE file included with
 * this program for more details.
 */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <asm/io.h>
#include <linux/version.h>
#include <linux/delay.h>
#include <linux/moduleparam.h>
#include <linux/crc32.h>
#include<linux/slab.h>

#include <stdbool.h>
#include <dahdi/kernel.h>

#include "allo2aCG.h"
#include "private.h"
#include "eint.h"
#include "../allospi/allospi.h"

#define FULL_DUPLEX 1

#define DEBUG_MAIN 		(1 << 0)
#define DEBUG_FRAMER		(1 << 6)

struct devtype tmp_devtype;

/* Platform SPI module commn flags */
#define GWSPI_GSM_ATcmd         14
#define GWSPI_REG_WRITE         3
#define GWSPI_REG_READ          4
#define GWSPI_FRM_VER           12

/* Enable disable features */
#define WORK_QUEUE              1
#ifdef WORK_QUEUE
static void mykmod_work_handler(struct work_struct *w);
static DECLARE_WORK(mykmod_work, mykmod_work_handler);
#endif

static int debug=0;
static int sigmode = FRMR_MODE_NO_ADDR_CMP;
static int max_latency = 4;
static int latency = 1;
static int ms_per_irq = 4;
static int ignore_rotary;

#define FLAG_2NDGEN  (1 << 3)
#define FLAG_3RDGEN  (1 << 7)
#define FLAG_5THGEN  (1 << 10)

struct devtype {
	char *desc;
	unsigned int flags;
};

struct g4;

enum linemode {T1, E1, J1};

struct spi_state {
	int wrreg;
	int rdreg;
};

struct g4_span {
	struct g4 *owner;
	u8 *writechunk;
	u8 *readchunk;
	enum linemode linemode;
	int sync;
	int alarmtimer;
	int notclear;
	unsigned long alarm_time;
	unsigned long losalarm_time;
	unsigned long aisalarm_time;
	unsigned long yelalarm_time;
	unsigned long alarmcheck_time;
	int spanflags;
	int syncpos;
	struct dahdi_span span;
	unsigned char txsigs[16];	/* Transmit sigs */
	int loopupcnt;
	int loopdowncnt;
	/* HDLC controller fields */
	struct dahdi_chan *sigchan;
	unsigned char sigmode;
	int sigactive;
	int frames_out;
	int frames_in;

	struct dahdi_chan *chans[2];		/* Individual channels */
	struct dahdi_echocan_state *ec[2];	/* Echocan state for each channel */
};

struct g4 {
	/* This structure exists one per card */
	struct device *dev;
	unsigned int intcount;
	int num;			/* Which card we are */
	int syncsrc;			/* active sync source */
	struct dahdi_device *ddev;
	struct g4_span *tspans[8];	/* Individual spans */
	int numspans;			/* Number of spans on the card */
	int blinktimer;
	int irq;			/* IRQ used by device */
	int order;			/* Order */
	const struct devtype *devtype;
	unsigned int falc31:1;	/* are we falc v3.1 (atomic not necessary) */
	unsigned int t1e1:8;	/* T1 / E1 select pins */
	int ledreg;				/* LED Register */
	int ledreg2;				/* LED Register2 */
	unsigned int gpio;
	unsigned int gpioctl;
	int e1recover;			/* E1 recovery timer */
	spinlock_t reglock;		/* lock register access */
	int spansstarted;		/* number of spans started */
	u32 *writechunk;		/* Double-word aligned write memory */
	u32 *readchunk;			/* Double-word aligned read memory */
	int last0;		/* for detecting double-missed IRQ */

	/* DMA related fields */
	unsigned int dmactrl;
	dma_addr_t 	readdma;
	dma_addr_t	writedma;
	void __iomem	*membase;	/* Base address of card */

	/* Flags for our bottom half */
	unsigned long checkflag;
	struct tasklet_struct g4_tlet;
	/* Latency related additions */
	unsigned char rxident;
	unsigned char lastindex;
	int numbufs;
	int needed_latency;
	
	struct spi_state st;
#if (DAHDI_VER_NUM < 2060000)
	char* variety;
#endif //(DAHDI_VER_NUM >= 2600)
	struct workqueue_struct *wq; /* work queue strct pointer */
};

static inline int G4_BASE_SIZE(struct g4 *wc)
{
	return DAHDI_MAX_CHUNKSIZE * 32 * 4;
}

/**
 * ports_on_framer - The number of ports on the framers.
 * @wc:		Board to check.
 *
 * The framer ports could be different the the number of ports on the card
 * since the dual spans have four ports internally but two ports extenally.
 *
 */
static inline unsigned int ports_on_framer(const struct g4 *wc)
{
	return 4;	
}

static int g4_startup(struct file *file, struct dahdi_span *span);
static int g4_shutdown(struct dahdi_span *span);
static int g4_reset_counters(struct dahdi_span *span);
static void g4_hdlc_hard_xmit(struct dahdi_chan *chan);
static int g4_ioctl(struct dahdi_chan *chan, unsigned int cmd, unsigned long data);

#define MAX_G4_CARDS 64

static void g4_isr_bh(unsigned long data);

static struct g4 *cards[MAX_G4_CARDS];

static int g4_ioctl(struct dahdi_chan *chan, unsigned int cmd, unsigned long data)
{
	struct g4_regs regs;
	struct g4_reg reg;
	int x;

	switch(cmd) {
	case ALLOG4_SET_REG:
		if (copy_from_user(&reg, (struct g4_reg __user *)data,
				   sizeof(reg)))
			return -EFAULT;
		printk("Setting REG:%d val:%d\n", reg.reg, reg.val);
 		__g4_outl__(reg.reg, reg.val, GWSPI_REG_WRITE); 
		break;
	case ALLOG4_GET_REG:
		if (copy_from_user(&reg, (struct g4_reg __user *)data,
				   sizeof(reg)))
			return -EFAULT;
		reg.val = 0; /*read register*/
		if (copy_to_user((struct g4_reg __user *)data,
				  &reg, sizeof(reg)))
			return -EFAULT;
		break;
	case ALLOG4_GET_REGS:
		for (x=0;x<NUM_REGS;x++)
			regs.regs[x] = 0; /*read all register*/ 
		if (copy_to_user((void __user *) data,
				 &regs, sizeof(regs)))
			return -EFAULT;
		break;
#if 0
	case ALLOG4C_SPAN_INIT:
	{
		/*
	        int stat=0;
	        stat=(unsigned int)data;
	        return g4_gsm_power_on(g4,2500,span,stat);
		*/
		return 0;
	        break;
	}
	case ALLOG4C_SPAN_REMOVE:
	{	/*
	        return g4_gsm_power_off(g4,800,span);
		*/
		return 0;
	        break;
	}
	case ALLOG4C_SPAN_STAT:
	{
		/*
	        unsigned char stat=g4->power_on_flag[span];
	        put_user(stat, (unsigned char __user *)data);
		*/
	        return 0;
	}
#endif
	default:
		return -ENOTTY;
	}
	return 0;
}

static void inline g4_hdlc_xmit_fifo(struct g4 *wc, unsigned int span, struct g4_span *ts)
{
	int res, i;
	unsigned char buf[32];
	unsigned int size =  sizeof(buf) / sizeof(buf[0]);

	res = dahdi_hdlc_getbuf(ts->sigchan, buf, &size);

	if (debug & DEBUG_FRAMER)
		printk( "Got buffer sized %d and res %d "
				"for %d\n", size, res, span);
	
	if (size > 0) {
		ts->sigactive = 1;

		if (debug & DEBUG_FRAMER) {
			printk( "TX(");
			for (i = 0; i < size; i++)
				printk( "%s%02x",
						(i ? " " : ""), buf[i]);
			printk( ")\n");
		}
		printk("TX on span %d (size %d intcount:%d)[",span+1,size, wc->intcount);
                for (i = 0; i < size; i++){
			if (buf[i] == 0xd) {
				printk("%02X \\r (%02X) \n", span+1, buf[i]);
			} else if (buf[i] == 0xa) {
				printk("%02X \\n (%02X) \n", span+1, buf[i]);
			} else  {
				printk("%02X %c  (%02X) \n", span+1, buf[i], buf[i]);
			}
                }
#ifdef SPI
		__allo_gsm_signaling_write(&buf[0], size, span);

		__g4_outl__(GWSPI_GSM_ATcmd, (0x01 << span), GWSPI_REG_WRITE);
		printk("]\n Send ATcmd end\n");
#endif
		printk(" ]\n");
	}
	else if (res < 0)
		ts->sigactive = 0;
}

static void g4_hdlc_hard_xmit(struct dahdi_chan *chan)
{
	struct g4 *wc = chan->pvt;
	int span = chan->span->offset;
	struct g4_span *ts = wc->tspans[span];
	unsigned long flags; 

	spin_lock_irqsave(&wc->reglock, flags);
	if (!ts->sigchan) {

		printk( "g4_hdlc_hard_xmit: Invalid (NULL) "
				"signalling channel\n");
		spin_unlock_irqrestore(&wc->reglock, flags);
		return;
	}
	spin_unlock_irqrestore(&wc->reglock, flags);

	if (debug & DEBUG_FRAMER)
		printk( "g4_hdlc_hard_xmit on channel %s "
				"(sigchan %s), sigactive=%d\n", chan->name,
				ts->sigchan->name, ts->sigactive);
}

static int g4_reset_counters(struct dahdi_span *span)
{
	struct g4_span *ts = container_of(span, struct g4_span, span);
	memset(&ts->span.count, 0, sizeof(ts->span.count));
	return 0;
}

static int g4_shutdown(struct dahdi_span *span)
{
	int tspan;
	int wasrunning;
	unsigned long flags;
	struct g4_span *ts = container_of(span, struct g4_span, span);
	struct g4 *wc = ts->owner;

	printk("%s %d\n", __func__, __LINE__); //pawan print
	tspan = span->offset + 1;
	if (tspan < 0) {
		printk( "T%dXXP: Span '%d' isn't us?\n",
				wc->numspans, span->spanno);
		return -1;
	}

	if (debug & DEBUG_MAIN)
		printk( "Shutting down span %d (%s)\n",
				span->spanno, span->name);

	spin_lock_irqsave(&wc->reglock, flags);
	wasrunning = span->flags & DAHDI_FLAG_RUNNING;

	span->flags &= ~DAHDI_FLAG_RUNNING;
	if (((wc->numspans == 8) &&
	    (!(wc->tspans[0]->span.flags & DAHDI_FLAG_RUNNING)) &&
	    (!(wc->tspans[1]->span.flags & DAHDI_FLAG_RUNNING)) &&
	    (!(wc->tspans[2]->span.flags & DAHDI_FLAG_RUNNING)) &&
	    (!(wc->tspans[3]->span.flags & DAHDI_FLAG_RUNNING)) &&
	    (!(wc->tspans[4]->span.flags & DAHDI_FLAG_RUNNING)) &&
	    (!(wc->tspans[5]->span.flags & DAHDI_FLAG_RUNNING)) &&
	    (!(wc->tspans[6]->span.flags & DAHDI_FLAG_RUNNING)) &&
	    (!(wc->tspans[7]->span.flags & DAHDI_FLAG_RUNNING)))
				||
	    ((wc->numspans == 4) &&
	    (!(wc->tspans[0]->span.flags & DAHDI_FLAG_RUNNING)) &&
	    (!(wc->tspans[1]->span.flags & DAHDI_FLAG_RUNNING)) &&
	    (!(wc->tspans[2]->span.flags & DAHDI_FLAG_RUNNING)) &&
	    (!(wc->tspans[3]->span.flags & DAHDI_FLAG_RUNNING)))
				||
	    ((wc->numspans == 2) &&
	    (!(wc->tspans[0]->span.flags & DAHDI_FLAG_RUNNING)) &&
	    (!(wc->tspans[1]->span.flags & DAHDI_FLAG_RUNNING)))) {
		/* No longer in use, disable interrupts */
		set_bit(G4_STOP_DMA, &wc->checkflag);
	} else
		set_bit(G4_CHECK_TIMING, &wc->checkflag);

	spin_unlock_irqrestore(&wc->reglock, flags);

	printk("%s %d\n", __func__, __LINE__); //pawan print
	/* Wait for interrupt routine to shut itself down */
	msleep(10);

	if (wasrunning)
		wc->spansstarted--;

	if (debug & DEBUG_MAIN)
		printk( "Span %d (%s) shutdown\n",
				span->spanno, span->name);
	printk("%s %d\n", __func__, __LINE__); //pawan print
	return 0;
}

static int
g4_spanconfig(struct file *file, struct dahdi_span *span,
	      struct dahdi_lineconfig *lc)
{
	return 0;
}
static int
g4_chanconfig(struct file *file, struct dahdi_chan *chan, int sigtype)
{
	return 0;
}

static int g4_open(struct dahdi_chan *chan)
{
        try_module_get(THIS_MODULE);
	return 0;
}

static int g4_close(struct dahdi_chan *chan)
{
        module_put(THIS_MODULE);
	return 0;
}

static int set_span_devicetype(struct g4 *wc)
{
#if (DAHDI_VER_NUM < 2060000)
	int x;
	struct g4_span *ts;

	for (x = 0; x < wc->numspans; x++) {
		ts = wc->tspans[x];
		strlcpy(ts->span.devicetype, wc->variety,
			sizeof(ts->span.devicetype));
	}
#else
#ifdef SPI
	wc->ddev->devicetype = kasprintf(GFP_KERNEL, "SPI");
#endif
	if (!wc->ddev->devicetype)
		return -ENOMEM;
#endif
	return 0;
}

/* The number of cards we have seen with each
   possible 'order' switch setting.
*/
static unsigned int order_index[16];

static void setup_chunks(struct g4 *wc, int which)
{
	struct g4_span *ts;
	int offset = 1;
	int x, y;
	int gen2;
	int basesize = G4_BASE_SIZE(wc) >> 2;

	gen2 = (wc->tspans[0]->spanflags & FLAG_2NDGEN);

	for (x = 0; x < wc->numspans; x++) {
		ts = wc->tspans[x];
		ts->writechunk = (void *)(wc->writechunk + (x * 32 * 2) + (which * (basesize)));
		ts->readchunk = (void *)(wc->readchunk + (x * 32 * 2) + (which * (basesize)));
		for (y=0;y<wc->tspans[x]->span.channels;y++) {
			struct dahdi_chan *mychans = ts->chans[y];
			if (gen2) {
				mychans->writechunk = (void *)(wc->writechunk + ((x * 32 + y + offset) * 2) + (which * (basesize)));
				mychans->readchunk = (void *)(wc->readchunk + ((x * 32 + y + offset) * 2) + (which * (basesize)));
			}
		}
	}
}

static void free_wc(struct g4 *wc)
{
	unsigned int x, y;

	printk("%s %d\n", __func__, __LINE__); //pawan print
	for (x = 0; x < ARRAY_SIZE(wc->tspans); x++) {
		if (!wc->tspans[x])
			continue;
		for (y = 0; y < ARRAY_SIZE(wc->tspans[x]->chans); y++) {
			kfree(wc->tspans[x]->chans[y]);
			kfree(wc->tspans[x]->ec[y]);
		}
		kfree(wc->tspans[x]);
	}

#if (DAHDI_VER_NUM >= 2060000)
	kfree(wc->ddev->devicetype);
	kfree(wc->ddev->location);
	kfree(wc->ddev->hardware_id);
#endif
	kfree(wc);
}

/**
 * g4_alloc_channels - Allocate the channels on a span.
 * @wc:		The board we're allocating for.
 * @ts:		The span we're allocating for.
 * @linemode:	Which mode (T1/E1/J1) to use for this span.
 *
 * This function must only be called before the span is assigned it's
 * possible for user processes to have an open reference to the
 * channels.
 *
 */
static int g4_alloc_channels(struct g4 *wc, struct g4_span *ts,
			     enum linemode linemode)
{
	int i;

	if (test_bit(DAHDI_FLAGBIT_REGISTERED, &ts->span.flags)) {
		return -EINVAL;
	}

	/* Cleanup any previously allocated channels. */
	for (i = 0; i < ARRAY_SIZE(ts->chans); ++i) {
		kfree(ts->chans[i]);
		kfree(ts->ec[i]);
	}

	ts->linemode = linemode;
	for (i = 0; i < 2; i++) 
	{
		struct dahdi_chan *chan;
		struct dahdi_echocan_state *ec;

		chan = kzalloc(sizeof(*chan), GFP_KERNEL);
		if (!chan) {
			free_wc(wc);
			return -ENOMEM;
		}
		ts->chans[i] = chan;

		ec = kzalloc(sizeof(*ec), GFP_KERNEL);
		if (!ec) {
			free_wc(wc);
			return -ENOMEM;
		}
		ts->ec[i] = ec;
	}

	return 0;
}

static const struct dahdi_span_ops g4_gen2_span_ops = {
	.owner = THIS_MODULE,
	.spanconfig = g4_spanconfig,
	.chanconfig = g4_chanconfig,
	.startup = g4_startup,
	.shutdown = g4_shutdown,
	.open = g4_open,
	.close  = g4_close,
	.ioctl = g4_ioctl,
	.hdlc_hard_xmit = g4_hdlc_hard_xmit,
};

/**
 * init_spans - Do first initialization on all the spans
 * @wc:		Card to initialize the spans on.
 *
 * This function is called *before* the dahdi_device is first registered
 * with the system. What happens in g4_init_one_span can happen between
 * when the device is registered and when the spans are assigned via
 * sysfs (or automatically).
 *
 */
static void init_spans(struct g4 *wc)
{
	int x, y;
	int gen2;
	struct g4_span *ts;
	//unsigned int reg;
	unsigned long flags;

	gen2 = (wc->tspans[0]->spanflags & FLAG_2NDGEN);
	for (x = 0; x < wc->numspans; x++) {
		ts = wc->tspans[x];

	        sprintf(ts->span.name, "allo2aCG/%d/%d",wc->num, x + 1);
                sprintf(ts->span.desc,"AlloGSM 2aCG  GSM/CDMA PCI Card %d Span %d", wc->num, x+1);    /* we always be 4 ports card */
		/* HDLC Specific init */
		ts->sigchan = NULL;
		ts->sigmode = sigmode;
		ts->sigactive = 0;
		ts->span.channels = 2;
		ts->span.deflaw = DAHDI_LAW_ALAW;
		ts->span.linecompat = DAHDI_CONFIG_AMI | DAHDI_CONFIG_CCS;
		ts->span.chans = ts->chans;
		ts->span.flags = 0;
		ts->owner = wc;
		ts->span.offset = x;
		printk("Span %d wc addr:%p rc addr: %p\n", x, ts->writechunk ,ts->readchunk);

		ts->span.ops = &g4_gen2_span_ops;
		for (y=0;y<wc->tspans[x]->span.channels;y++) {
			struct dahdi_chan *mychans = ts->chans[y];
			sprintf(mychans->name, "allo2aCG%d/%d/%d/%d", wc->numspans, wc->num, x + 1, y + 1);
			mychans->pvt = wc;
			if (y==0)
				mychans->sigcap =  DAHDI_SIG_CLEAR;
			else if (y==1){
			        mychans->sigcap =  DAHDI_SIG_HARDHDLC;
				ts->sigchan = mychans;
			}
			mychans->chanpos = y + 1;
		}
	        ts->sigactive = 0;

		/* Start checking for alarms in 250 ms */
		ts->alarmcheck_time = jiffies + msecs_to_jiffies(250);

		/* Enable 1sec timer interrupt */
		spin_lock_irqsave(&wc->reglock, flags);
		spin_unlock_irqrestore(&wc->reglock, flags);

		g4_reset_counters(&ts->span);

	}

	set_span_devicetype(wc);
	setup_chunks(wc, 0);
	wc->lastindex = 0;
}

static int g4_startup(struct file *file, struct dahdi_span *span)
{
	int tspan;
	unsigned long flags;
	int alreadyrunning;
	struct g4_span *ts = container_of(span, struct g4_span, span);
	struct g4 *wc = ts->owner;
	printk("g4_startup called ms_per_irq %d\n", ms_per_irq);// pawan print

	if (debug)
		printk( "About to enter startup!\n");

	tspan = span->offset + 1;
	if (tspan < 0) {

		printk( "GSM%dXX: Span '%d' isn't us?\n",
				wc->numspans, span->spanno);
		return -1;
	}

	printk("%s %d\n", __func__, __LINE__); //pawan print
	spin_lock_irqsave(&wc->reglock, flags);

	alreadyrunning = span->flags & DAHDI_FLAG_RUNNING;

	printk("%s %d\n", __func__, __LINE__); //pawan print

	if (!alreadyrunning) {
		span->flags |= DAHDI_FLAG_RUNNING;
		wc->spansstarted++;
		/* enable interrupts */
		/* Start DMA, enabling DMA interrupts on read only */
		printk("%s %d\n", __func__, __LINE__); //pawan print
		wc->dmactrl |= (ts->spanflags & FLAG_2NDGEN) ? 0xc0000000 : 0xc0000003;
	}

	printk("%s %d\n", __func__, __LINE__); //pawan print
	spin_unlock_irqrestore(&wc->reglock, flags);
	printk("%s %d\n", __func__, __LINE__); //pawan print

	if (debug)
		printk( "Completed startup!\n");
	printk("%s %d\n", __func__, __LINE__); //pawan print
	clear_bit(G4_IGNORE_LATENCY, &wc->checkflag);
	printk("%s %d\n", __func__, __LINE__); //pawan print
	return 0;
}

#if (DAHDI_CHUNKSIZE != 8)
#error Sorry, nextgen does not support chunksize != 8
#endif

static void __receive_span(struct g4_span *ts)
{
	_dahdi_receive(&ts->span);
}

static inline void __transmit_span(struct g4_span *ts)
{
	_dahdi_transmit(&ts->span);
}

#define MAX_NUM_CARDS 4
int __gsm_malloc_chunk(struct g4 *wc,unsigned int frq)
{
        __allo_gsm_set_chunk(&(wc->readchunk), &(wc->writechunk),frq);
        wc->readdma = wc->writedma + frq * DAHDI_MAX_CHUNKSIZE * (MAX_NUM_CARDS) * 2;

        return 0;
}

static int g4_allocate_buffers(struct g4 *wc, int numbufs,
			       void **oldalloc, dma_addr_t *oldwritedma)
{
#ifdef SPI
        wc->dev = __allo_gsm_get_spidev((unsigned long)wc->membase);
        dma_set_coherent_mask(wc->dev, DMA_BIT_MASK(32));
        wc->writechunk = dma_alloc_coherent(wc->dev, ms_per_irq * DAHDI_MAX_CHUNKSIZE * MAX_NUM_CARDS * 2 * 4  , &wc->writedma, GFP_KERNEL);
                if (!wc->writechunk) {
                        printk("g4: Unable to allocate DMA-able memory\n");
                        return -ENOMEM;
                }
	 __gsm_malloc_chunk(wc,ms_per_irq);

#endif
	if (oldwritedma)
		*oldwritedma = wc->writedma;
	if (oldalloc)
		*oldalloc = wc->writechunk;
#ifdef SPI
	printk("Addr writechunk: %p Addr readchunk: %p ; size :%d\n ", wc->writechunk, wc->readchunk, ms_per_irq * DAHDI_CHUNKSIZE * wc->numspans);
	memset(wc->writechunk, 0x12, ms_per_irq * DAHDI_CHUNKSIZE * wc->numspans * 2 * 4);
	memset(wc->readchunk, 0x34, ms_per_irq * DAHDI_CHUNKSIZE * wc->numspans * 2 * 4);
#endif
	
	wc->numbufs = numbufs;
	return 0;
}

static void g4_isr_bh(unsigned long data)
{
	struct g4 *wc = (struct g4 *)data;

	if (test_bit(G4_CHANGE_LATENCY, &wc->checkflag)) {
		if (wc->needed_latency != wc->numbufs) {
			clear_bit(G4_CHANGE_LATENCY, &wc->checkflag);
		}
	}
}

#ifdef FULL_DUPLEX
static void gsm_transmit_recieve(struct g4 *wc, unsigned int ms_per_irq, int order)
{
	unsigned char *txbuf;
	unsigned char *rxbuf;

	__allo_gsm_transmit((unsigned long)wc->membase, (unsigned char *)wc->writechunk, &txbuf,ms_per_irq, order);
	__allo_gsm_receive((unsigned long)wc->membase, (unsigned char *)wc->readchunk, &rxbuf ,ms_per_irq, order);

	__allo_gsm_pcm_write_read((unsigned char **)&txbuf, (unsigned char **)&rxbuf, wc->numspans * DAHDI_CHUNKSIZE * ms_per_irq);
}

static void gsm_transmit_ready(struct g4 *wc, unsigned int ms_per_irq, unsigned int order)
{
	int x, y, pos;
	unsigned char *txbuf;
	__allo_gsm_transmit((unsigned long)wc->membase, (unsigned char *)wc->writechunk, &txbuf,ms_per_irq,order); /* Take starting location for tx in txbuf from this function */ 

	for (y=0;y<DAHDI_CHUNKSIZE;y++) {
		for (x=0;x<wc->numspans;x++) {
			pos = y * wc->numspans + x;
			txbuf[pos] = wc->tspans[x]->chans[0]->writechunk[y];
		}
	}
}

static void gsm_receive_complete(struct g4 *wc, unsigned int ms_per_irq, unsigned int order)
{
	int x, y, pos;
	unsigned char *rxbuf;
	
	__allo_gsm_receive((unsigned long)wc->membase, (unsigned char *)wc->readchunk, &rxbuf ,ms_per_irq,order);
	for (x=0;x<DAHDI_CHUNKSIZE;x++) {
		for (y=0;y<wc->numspans/*MAX_NUM_CARDS*/;y++) {
			if ((1 << y)) {
				pos = (wc->numspans* x + y);
				wc->tspans[y]->chans[0]->readchunk[x] = rxbuf[pos];
			}
		}
	}
}
#endif

static inline void g4_run(struct g4 *wc)
{
        int s=0;
        struct g4_span *ts = NULL;
        //static int tcount =0 ,rcount=0;
        for (s=0; s< wc->numspans; s++) {
                ts = wc->tspans[s];
                if (ts) {
                        if (ts->span.flags & DAHDI_FLAG_RUNNING) {
				g4_hdlc_xmit_fifo(wc, s, ts);
                        }
                }
        }
}

#define MAX_RX_READ		(32-1)
#define MAX_RX_READ_BUF		128

static irqreturn_t _g4_interrupt_gen2(int irq, void *dev_id)
{
	struct g4 *wc = dev_id;
	struct dahdi_chan *sigchan;
	unsigned long flags;
	
        unsigned int readsize=0, i, j;
        unsigned char readbuf[MAX_RX_READ_BUF];
	struct g4_span *ts; 
	int order;
	int x;
	int remreadsize=0;
	int orgreadsize=0;

 	__g4_outl__(0x0D,0x00,GWSPI_REG_WRITE); // Clearing interrupt here

	/* Check this first in case we get a spurious interrupt */
	if (unlikely(test_bit(G4_STOP_DMA, &wc->checkflag))) {
		/* Stop DMA cleanly if requested */
		wc->dmactrl = 0x0;
		return IRQ_RETVAL(1);
	}

	g4_run(wc);

////////////////////////////////////////////////////////////////////////////////////////
	
	for (i = 0; i < wc->numspans ; i++) {
		if((wc->intcount % 4) != i) continue;
#ifdef SPI
		readsize = __g4_inl__(i + 4, GWSPI_REG_READ);
#endif
		if (!readsize)
			continue;

		printk("%s %d SPAN %d readsize:%d \n", __func__, __LINE__, i, readsize );
		if(readsize<(MAX_RX_READ+1)){
			int newreadsize = __g4_inl__(i + 4, GWSPI_REG_READ);	/* Make sure there is no data inflow*/
			if(newreadsize > readsize){
				printk("%s %d SPAN %d newreadsize:%d data still coming check in next interrupt!!\n", __func__, __LINE__, i, newreadsize );
				continue;	
			}
		}

		orgreadsize = readsize;
		memset(readbuf,  0, MAX_RX_READ_BUF);

		if(readsize > MAX_RX_READ){
			remreadsize = orgreadsize - MAX_RX_READ; 
			readsize = MAX_RX_READ;
		}

#ifdef SPI
		printk("%s %d SPAN %d readsize:%d \n", __func__, __LINE__, i, readsize );
		__allo_gsm_signaling_read(&readbuf[0], readsize, i);
#endif
		if((orgreadsize > MAX_RX_READ) && !(orgreadsize < (MAX_RX_READ*2))){
			if(remreadsize > MAX_RX_READ){
				remreadsize = MAX_RX_READ;
			}
			readsize += remreadsize;
#ifdef SPI
			__allo_gsm_signaling_read(&readbuf[MAX_RX_READ], remreadsize, i);
			printk("%s %d SPAN %d remreadsize:%d \n", __func__, __LINE__, i, remreadsize);
#endif
		}

		printk("RX on span %d (size %d:%d)[\n",i+1, readsize, orgreadsize);
		for ( j = 0; j < readsize; j++){
			if (readbuf[j] == 0xd) {
				printk("%02X \\r (%02X) \n", i+1, readbuf[j]);
			} else if (readbuf[j] == 0xa) {
				printk("%02X \\n (%02X) \n", i+1, readbuf[j]);
			} else if ((readbuf[j] > 0x20) && (readbuf[j] <0x7E)) {
				printk("%02X %c  (%02X) \n", i+1, readbuf[j], readbuf[j]);
			} else  {
				printk("%02X    (%02X) \n", i+1, readbuf[j]);
			}
		}
		printk("]]\n");

		ts = wc->tspans[i];
		spin_lock_irqsave(&wc->reglock, flags);
                if (!ts->sigchan) {
                        spin_unlock_irqrestore(&wc->reglock, flags);
                }else{
			sigchan = ts->sigchan;
			spin_unlock_irqrestore(&wc->reglock, flags);

			printk("%s %d %d readbuf%x[%d] size %d\n", __func__, __LINE__, i, readbuf[j-1], j, readsize);
			dahdi_hdlc_putbuf(sigchan, readbuf, readsize);
			dahdi_hdlc_finish(sigchan);
			printk("%s %d %d %x size %d\n", __func__, __LINE__, i, readbuf[j-1], readsize);
		}
	}

/*******************interrupt latecy control*///////////////

	wc->intcount++;

#ifdef SPI
	for(order=0;order < ms_per_irq;order++)
	{
		for (x=0;x<wc->numspans;x++) {
			if (wc->tspans[x]->span.flags & DAHDI_FLAG_RUNNING) {
				//dahdi_transmit(wc->tspans[x]);
				__transmit_span(wc->tspans[x]);
			}
		}
		gsm_transmit_ready(wc, ms_per_irq,  order); //shd have only 1mspirq have to do tx rx at once
	}
	gsm_transmit_recieve(wc, ms_per_irq,0); //shd have only 1mspirq have to do tx rx at once
	for(order=0;order < ms_per_irq;order++)
	{
		//printk ("--------------- Order %d \n", order);
		gsm_receive_complete(wc,ms_per_irq, order);
		for (x=0;x<wc->numspans;x++) {
			if (wc->tspans[x]->span.flags & DAHDI_FLAG_RUNNING) {
				__receive_span(wc->tspans[x]);
				//dahdi_receive(wc->tspans[x]);
			}
		}
	}
#endif

	return IRQ_RETVAL(1);
/*------------------------------------------------------------------------------------------------*/
}

#ifdef WORK_QUEUE
static void mykmod_work_handler(struct work_struct *w)
{
	_g4_interrupt_gen2(0, cards[0]); // FIXME keeping it fixed for sometime. 
}
#endif

DAHDI_IRQ_HANDLER(g4_interrupt_gen2)
{
	irqreturn_t ret;
	//unsigned long flags;
#ifdef SPI
#ifdef WORK_QUEUE
	struct g4 *wc = dev_id;
	queue_work(wc->wq, &mykmod_work);
#endif
#endif
	return IRQ_RETVAL(1);
	return ret;
}

static int __devinit g4_launch(struct g4 *wc)
{
	int x;
	int res;

	if (test_bit(DAHDI_FLAGBIT_REGISTERED, &wc->tspans[0]->span.flags))
		return 0;

	if (debug) {
		printk(
			 "GSM%d: Launching card: %d\n", wc->numspans,
			 wc->order);
	}

#if (DAHDI_VER_NUM >= 2060000)
	wc->ddev->manufacturer = "allo.com";
	if (!ignore_rotary && (1 == order_index[wc->order])) {
		wc->ddev->location = kasprintf(GFP_KERNEL,
					      "Board ID Switch %d", wc->order);
	} else {
		wc->ddev->location = kasprintf(GFP_KERNEL,
					      "SPI");
	}

	if (!wc->ddev->location)
		return -ENOMEM;

	for (x = 0; x < wc->numspans; ++x) {
		list_add_tail(&wc->tspans[x]->span.device_node,
			      &wc->ddev->spans);
	}
#ifdef SPI
        wc->ddev->dev.kobj.name = "SPI";
        res = dahdi_register_device(wc->ddev, NULL); 
#endif
	if (res) {
		printk( "Failed to register with DAHDI.\n");
		return res;
	}
#else
        if (dahdi_register(&wc->tspans[0]->span, 0)) {
                printk( "Unable to register span %s\n",
                                wc->tspans[0]->span.name);
                return -1;
        }
        if (dahdi_register(&wc->tspans[1]->span, 0)) {
                printk( "Unable to register span %s\n",
                                wc->tspans[1]->span.name);
                dahdi_unregister(&wc->tspans[0]->span);
                return -1;
        }

        if (wc->numspans == 4) {
                if (dahdi_register(&wc->tspans[2]->span, 0)) {
                        printk( "Unable to register span %s\n",
                                        wc->tspans[2]->span.name);
                        dahdi_unregister(&wc->tspans[0]->span);
                        dahdi_unregister(&wc->tspans[1]->span);
                        return -1;
                }
                if (dahdi_register(&wc->tspans[3]->span, 0)) {
                        printk( "Unable to register span %s\n",
                                        wc->tspans[3]->span.name);
                        dahdi_unregister(&wc->tspans[0]->span);
                        dahdi_unregister(&wc->tspans[1]->span);
                        dahdi_unregister(&wc->tspans[2]->span);
                        return -1;
                }
        }
#endif
	set_bit(G4_CHECK_TIMING, &wc->checkflag);
	tasklet_init(&wc->g4_tlet, g4_isr_bh, (unsigned long)wc);
	return 0;
}

/**
 * allo2aCG_sort_cards - Sort the cards in card array by rotary switch settings.
 *
 */
static void allo2aCG_sort_cards(void)
{
	int x;

	/* get the current number of probed cards and run a slice of a tail
	 * insertion sort */
	for (x = 0; x < MAX_G4_CARDS; x++) {
		if (!cards[x+1])
			break;
	}
	for ( ; x > 0; x--) {
		if (cards[x]->order < cards[x-1]->order) {
			struct g4 *tmp = cards[x];
			cards[x] = cards[x-1];
			cards[x-1] = tmp;
		} else {
			/* if we're not moving it, we won't move any more
			 * since all cards are sorted on addition */
			break;
		}
	}
}

static int clear_fpga_buff(void)
{
	int j=0;
	int l=10;
	int c_size=0;
	u8 readbuf[32];
	for(j=0; j<4; j++){
		/* clear all junk data before command*/
		c_size=__g4_inl__((j+4), GWSPI_REG_READ);
		if(c_size==0) c_size=31;
		for(l=10; l>0; l--){
			int ck, cj;
			printk("GSM-%d (size %d) \n",j+1,c_size );
			for(ck=c_size; ck>0; ck=ck-32){
				cj = ck; 
				if(cj>31) cj=31;
				__allo_gsm_signaling_read(&readbuf[0], cj , j);
			}
			c_size=__g4_inl__((j+4), GWSPI_REG_READ);
			if(c_size==0) 
				break;
		}
	}
	return 0;
}


static int rw_test_bulk(void)
{
	int j=0;
	int x=0;
	int c_size=0;
	u8 txbuf[9]={0x61,0x74,0x2b,0x63,0x67,0x6d,0x72,0x0d,0x0a};
	u8 readbuf[32];
	u8 data;

	for(j=0; j<4; j++){
		int ck, cj;
#if 1	/* clear all junk data before command*/
		c_size=__g4_inl__((j+4), GWSPI_REG_READ);
		if(c_size==0) c_size=31;
		printk("GSM-%d (size %d) \n",j+1,c_size );

		//if(c_size>31) c_size = 31;
		for(ck=c_size; ck>0; ck=ck-32){
			cj = ck; 
			if(cj>31) cj=31;
			__allo_gsm_signaling_read(&readbuf[0], cj , j);
		}
#endif

		__allo_gsm_signaling_write(&txbuf[0], sizeof(txbuf), j);

		__g4_outl__(GWSPI_GSM_ATcmd,(0x01<<j), GWSPI_REG_WRITE);
		 printk("TX on span %d (size %d:)[\n ",j+1, sizeof(txbuf));
		for(x=0; x < sizeof(txbuf); x++){
			data=txbuf[x];
        		if (data == 0xd) {
        		        printk("%d        \\r                       (%X)\n",j+1,data);
        		} else if (data == 0xa) {
        		        printk("%d        \\n                       (%X)\n",j+1,data);
        		} else if ((data > 0x20) && (data < 0x7e)) {
        		        printk("%d         %c                       (%X)\n",j+1, data, data);
        		} else  {
        		        printk("%d                                 (%X)\n",j+1, data);
        		}
		}

		msleep(40); //msleep(40); msleep(40); msleep(40); 
#if 1
		c_size=__g4_inl__((j+4), GWSPI_REG_READ);

		printk("GSM-%d (size %d) \n",j+1,c_size );

		for(ck=c_size; ck>0; ck=ck-32){
			cj = ck; 
			if(cj>31) cj=31;
			__allo_gsm_signaling_read(&readbuf[0], cj , j);

			for(x=0; x<cj; x++){
				data=readbuf[x];
        		        if (data == 0xd) {
        		                printk("%d        \\r                       (%X)\n",j+1,data);
        		        } else if (data == 0xa) {
        		                printk("%d        \\n                       (%X)\n",j+1,data);
        		        } else if ((data > 0x20) && (data < 0x7e)) {
        		                printk("%d         %c                       (%X)\n",j+1, data, data);
        		        } else  {
        		                printk("%d                                 (%X)\n",j+1, data);
        		        }
			}
		}
#endif
	}
	return 0;
}

#ifdef SPI
static int g4_init_one(void) {
#endif
	int res;
	struct g4 *wc;
	unsigned int x;
	int init_latency;
	char tmp[32];
	unsigned int fversion;

	printk("%s %d\n", __func__, __LINE__); //pawan print
	wc = kzalloc(sizeof(*wc), GFP_KERNEL);
	if (!wc)
		return -ENOMEM;

#if (DAHDI_VER_NUM >= 2060000)
	wc->ddev = dahdi_create_device();
	if (!wc->ddev) {
		kfree(wc);
		return -ENOMEM;
	}
#endif

	printk("%s %d\n", __func__, __LINE__); //pawan print
	spin_lock_init(&wc->reglock);
	init_interrupt_deps();
	printk("%s %d\n", __func__, __LINE__); //pawan print

#ifdef SPI
	
	tmp_devtype.flags = FLAG_3RDGEN | FLAG_2NDGEN | FLAG_5THGEN;
	wc->devtype = (const struct devtype *)(&tmp_devtype);
	wc->numspans = 4;
#endif
	
#ifdef SPI
	printk("%s %d\n", __func__, __LINE__); //pawan print
	
	wc->membase =  0x00000000; 
	init_latency = 1;
#endif

	if (max_latency < init_latency) {
		printk(KERN_INFO "maxlatency must be set to something greater than %d ms, increasing it to %d\n", init_latency, init_latency);
		max_latency = init_latency;
	}
	
	/* FIXME for SPI */
	if (g4_allocate_buffers(wc, init_latency, NULL, NULL)) {
		return -ENOMEM;
	}

	/* Initialize hardware */
	for(x = 0; x < MAX_G4_CARDS; x++) {
		if (!cards[x])
			break;
	}
	
	if (x >= MAX_G4_CARDS) {
		printk( "No cards[] slot available!!\n");
		kfree(wc);
		return -ENOMEM;
	}
	
	wc->num = x;
	cards[x] = wc;
	

#ifdef WORK_QUEUE
#ifdef SPI
	sprintf(tmp, "gsm%d", wc->numspans);
	wc->wq = create_workqueue(tmp);
#endif			
#endif			
	/* Allocate pieces we need here */
	for (x = 0; x < ports_on_framer(wc); x++) {
		struct g4_span *ts;
		enum linemode linemode;

		ts = kzalloc(sizeof(*ts), GFP_KERNEL);
		if (!ts) {
			free_wc(wc);
			return -ENOMEM;
		}
		wc->tspans[x] = ts;

		ts->spanflags |= wc->devtype->flags;
		linemode = 0;
		g4_alloc_channels(wc, wc->tspans[x], linemode);
	}

#ifdef SPI
      	s500_fpga_reset(1);
	fversion = __g4_inl__(GWSPI_FRM_VER, GWSPI_REG_READ);
	if(fversion==0) 
		s500_fpga_reset(0);
	printk("RESETING FPGA COMPLETE..\n");

        wc->irq = s500_eint_init();
#if (DAHDI_VER_NUM >= 2100200)
        if (request_irq(wc->irq, g4_interrupt_gen2, 
			IRQF_TRIGGER_FALLING | IRQF_DISABLED, "allo2aCG", wc)) {
#else
	if (request_irq(wc->irq, g4_interrupt_gen2,
			IRQF_TRIGGER_FALLING | IRQF_DISABLED, "allo2aCG", wc)) {
#endif
		free_wc(wc);
		return -EIO;
	}
#endif
	printk("%s %d\n", __func__, __LINE__); //pawan print
	
	init_spans(wc);

	printk("%s %d\n", __func__, __LINE__); //pawan print
	if (!ignore_rotary)
		allo2aCG_sort_cards();
	
	printk("%s %d\n", __func__, __LINE__); //pawan print
#ifdef SPI
	clear_fpga_buff();
	init_fpga(ms_per_irq);
	rw_test_bulk();
#endif
	res = 0;
	if (ignore_rotary)
		res = g4_launch(wc);

	return res;
}

static int g4_hardware_stop(struct g4 *wc)
{

	stop_fpga();

	/* Turn off DMA, leave interrupts enabled */
	set_bit(G4_STOP_DMA, &wc->checkflag);

	/* Wait for interrupts to stop */
	msleep(25);
	wc->gpio = 0x00000000;

	if (debug) {
		printk( "Stopped GSM%d, Turned off DMA\n",
				wc->numspans);
	}
	return 0;
}

static void _g4_remove_one(struct g4 *wc)
{
	int basesize;

	if (!wc)
		return;

	printk("%s %d\n", __func__, __LINE__); //pawan print
#if (DAHDI_VER_NUM >= 2060000)
	dahdi_unregister_device(wc->ddev);
#endif
	printk("%s %d\n", __func__, __LINE__); //pawan print

	/* Stop hardware */
	g4_hardware_stop(wc);

	printk("%s %d\n", __func__, __LINE__); //pawan print

	basesize = DAHDI_MAX_CHUNKSIZE * 32 * 4;

	if (!(wc->tspans[0]->spanflags & FLAG_2NDGEN))
		basesize = basesize * 2;

#ifdef SPI
	printk("%s %d\n", __func__, __LINE__); //pawan print
	free_irq(wc->irq, wc);
#ifdef WORK_QUEUE
	if (wc->wq) {
		flush_workqueue(wc->wq);
		destroy_workqueue(wc->wq);
		printk(KERN_ALERT "%s :%d\n",__FUNCTION__, __LINE__);
	}
#endif
	printk("%s %d\n", __func__, __LINE__); //pawan print
	s500_eint_exit(wc->irq);
#endif
	printk("%s %d\n", __func__, __LINE__); //pawan print
	
	order_index[wc->order]--;
	
	cards[wc->num] = NULL;
	free_wc(wc);
	printk(KERN_INFO "%s \n",__FUNCTION__);
}

static int __init g4_init(void)
{
	int i;
	int res=0;

	printk("%s %d\n", __func__, __LINE__); //pawan print
#ifdef SPI
	g4_init_one();
#endif
	printk("%s %d\n", __func__, __LINE__); //pawan print

/*temp disabled for gsm*/
	/* If we're ignoring the rotary switch settings, then we've already
	 * registered in the context of .probe */
	if (!ignore_rotary) {

		/* Initialize cards since we have all of them. Warn for
		 * missing zero and duplicate numbers. */

		if (cards[0] && cards[0]->order != 0) {
			printk(KERN_NOTICE "allo2aCG: Ident of first card is not zero (%d)\n",
				cards[0]->order);
		}

		for (i = 0; cards[i]; i++) {
			/* warn the user of duplicate ident values it is
			 * probably unintended */
			if (debug && res < 15 && cards[i+1] &&
			    cards[res]->order == cards[i+1]->order) {
				printk(KERN_NOTICE "allo2aCG: Duplicate ident "
				       "value found (%d)\n", cards[i]->order);
			}
			res = g4_launch(cards[i]);
			if (res) {
				int j;
				for (j = 0; j < i; ++j)
					_g4_remove_one(cards[j]);
				break;
			}
		}
	}
return res;
}

static void __exit g4_cleanup(void)
{
#ifdef SPI
	int i;
	for (i = 0; cards[i]; i++) {
		printk("%s %d\n", __func__, __LINE__); //pawan print
		_g4_remove_one(cards[i]);
	}
#endif
}

MODULE_AUTHOR("allo.com");
MODULE_DESCRIPTION("ALLO Dual/Quad-port GSM Card Driver");
MODULE_ALIAS("allo2aCG");
MODULE_LICENSE("GPL v2");
module_param(debug, int, 0600);
module_param(max_latency, int, 0600);
module_param(sigmode, int, 0600);
module_param(latency, int, 0600);
module_param(ms_per_irq, int, 0600);
module_param(ignore_rotary, int, 0400);
MODULE_PARM_DESC(ignore_rotary, "Set to > 0 to ignore the rotary switch when " \
		 "registering with DAHDI.");

module_init(g4_init);
module_exit(g4_cleanup);

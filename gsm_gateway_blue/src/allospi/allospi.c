#include <linux/init.h>
#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/delay.h>

#include "allospi.h"
#define MAX_SPIDEV 	7
#define MAX_SPEED	16000000
#define SPI_TEST	1
//#define SPI_DMA	1

struct allospi {
	struct spi_device *spidev[MAX_SPIDEV];
};

struct allospi wc;

int allo_spi_write_read(u8 **txbuf, u8 **rxbuf, unsigned int size, int module)
{
	struct spi_transfer t;
        struct spi_message m;
	int ret;
	if(!wc.spidev[module]) {
		printk("allospi: write dir: spi module:%d  not registered \n",module);
		return -1;
	}

        spi_message_init(&m);
	memset(&t, 0, (sizeof t));

#ifdef SPI_DMA
	{
		int i;
		wc.spidev[module]->bits_per_word = 32;
		spi_setup(wc.spidev[module]);

		u8 * word = (u8 *) *txbuf;
		for(i=0; i<size; i=i+4){
			u8 tmp = word[i+3];
			word[i+3] = word[i];
			word[i] = tmp;
			tmp = word[i+2];
			word[i+2] = word[i+1];
			word[i+1] = tmp;
		}
	}
#endif

        t.len            = size;
        //t.speed_hz       = bfin_spi_adc->hz, //change speed of the bus.
        t.rx_buf = *rxbuf;
        t.tx_buf = *txbuf;
        spi_message_add_tail(&t, &m);
        ret = spi_sync(wc.spidev[module], &m);

#ifdef SPI_DMA
	word = (u8 *) *rxbuf;
	for(i=0; i<size; i=i+4){
		u8 tmp = word[i+3];
		word[i+3] = word[i];
		word[i] = tmp;
		tmp = word[i+2];
		word[i+2] = word[i+1];
		word[i+1] = tmp;
	}

	wc.spidev[module]->bits_per_word = 8;
        spi_setup(wc.spidev[module]);
#endif
return ret;
}
EXPORT_SYMBOL(allo_spi_write_read);

int allo_spi_write_direct_u32(u32 word, int module)
{
	int ret=0;
	if(!wc.spidev[module]) {
		printk("allospi: write dir: spi module:%d  not registered \n",module);
		return -1;
	}
        wc.spidev[module]->bits_per_word = 32;
        spi_setup(wc.spidev[module]);

	ret = spi_write(wc.spidev[module], &word, sizeof(word));

        wc.spidev[module]->bits_per_word = 8;
        spi_setup(wc.spidev[module]);

	return ret;
}
EXPORT_SYMBOL(allo_spi_write_direct_u32);

int allo_spi_write_direct(u8 word, int module)
{
	if(!wc.spidev[module]) {
		printk("allospi: write dir: spi module:%d  not registered \n",module);
		return -1;
	}
	return spi_write(wc.spidev[module], &word, sizeof(word));
}
EXPORT_SYMBOL(allo_spi_write_direct);

int allo_spi_write(u8 *word,unsigned int size, int module)
{
	if(!wc.spidev[module]) {
		printk("allospi: write dir: spi module:%d  not registered \n",module);
		return -1;
	}

#ifdef SPI_DMA
	if(size < 64){
		spi_write(wc.spidev[module], (const void *)word, size);
	}else{
		int i=0;

		wc.spidev[module]->bits_per_word = 32;
        	spi_setup(wc.spidev[module]);

		u8 * word1 = (u8 *) word;
		for(i=0; i<size; i=i+4){
			u8 tmp = word1[i+3];
			word1[i+3] = word1[i];
			word1[i] = tmp;
			tmp = word1[i+2];
			word1[i+2] = word1[i+1];
			word1[i+1] = tmp;
		}
		
		spi_write(wc.spidev[module], (const void *)word, size);

		wc.spidev[module]->bits_per_word = 8;
        	spi_setup(wc.spidev[module]);
	}
return 0;
#else		
	return spi_write(wc.spidev[module], (const void *)word, size);
#endif
}
EXPORT_SYMBOL(allo_spi_write);

u32 allo_spi_read_direct_u32(int module)
{
	int ret;
        u32 word;

	if(!wc.spidev[module]) {
		printk("allospi: read dir: spi module:%d  not registered \n",module);
		return -1;
	}
        wc.spidev[module]->bits_per_word = 32;
        spi_setup(wc.spidev[module]);

	ret = spi_read(wc.spidev[module], &word, sizeof(word));

        wc.spidev[module]->bits_per_word = 8;
        spi_setup(wc.spidev[module]);

        return word;
}
EXPORT_SYMBOL(allo_spi_read_direct_u32);

int allo_spi_read_direct(int module)
{
	int ret;
        u8 word;

	if(!wc.spidev[module]) {
		printk("allospi: read dir: spi module:%d  not registered \n",module);
		return -1;
	}
	ret = spi_read(wc.spidev[module], &word, sizeof(word));

        return word;
}
EXPORT_SYMBOL(allo_spi_read_direct);

int allo_spi_read(u8 *word, unsigned int size, int module)
{
	if(!wc.spidev[module]) {
		printk("allospi: write dir: spi module:%d  not registered \n",module);
		return -1;
	}
		
#ifdef SPI_DMA
	printk("bpw:%d size:%d \n", wc.spidev[module]->bits_per_word, size);
	if(size < 64){
		spi_read(wc.spidev[module], word, sizeof(u8) * size);
	}else{
		int i=0;

		wc.spidev[module]->bits_per_word = 32;
        	spi_setup(wc.spidev[module]);
		
		spi_read(wc.spidev[module], (u8 *)word, sizeof(u8) * size);
		
		for(i=0; i<size; i=i+4){
			u8 tmp = word[i+3];
			word[i+3] = word[i];
			word[i] = tmp;
			tmp = word[i+2];
			word[i+2] = word[i+1];
			word[i+1] = tmp;
		}

		wc.spidev[module]->bits_per_word = 8;
        	spi_setup(wc.spidev[module]);
	}
#else
	spi_read(wc.spidev[module], word, sizeof(u8) * size);
#endif
return 0;
}
EXPORT_SYMBOL(allo_spi_read);

struct device * allo_spi_get_dev(int module)
{
	printk("allospi: allo_spi_get_dev\n");
        return &wc.spidev[module]->dev;
}
EXPORT_SYMBOL(allo_spi_get_dev);

static int allospi_probe(struct spi_device *spi)
{
	char *tmp;
	int i;
	printk("allospi_probe\n");

	spi->bits_per_word = 8;
	spi->max_speed_hz = MAX_SPEED;
	spi_setup(spi);
	printk("speed:%d cs:%d mode:%d modalias:%s bits_per_word:%d\n",spi->max_speed_hz,spi->chip_select,spi->mode,spi->modalias,spi->bits_per_word);

	for(i=0;i<MAX_SPIDEV;i++) {
		tmp = kasprintf(GFP_KERNEL,"allospi%d",i);
		printk("allospi_probe: ref:%s modalias:%s \n",tmp,spi->modalias);
		if((strcmp(tmp,spi->modalias) == 0)) {
			printk("allospi_probe: match\n");
			wc.spidev[i] = spi;
			break;
		}
		printk("allospi_probe: NO match\n");
	}
return 0;
}

static int allospi_remove(struct spi_device *spi)
{
	printk("allospi_remove\n");

	return 0;
}

static struct spi_driver allospi_driver0 = {
        .probe          = allospi_probe,
        .remove         = allospi_remove,
        .driver = {
                .name   = "allospi0",
                .owner  = THIS_MODULE,
        },
};
static struct spi_driver allospi_driver1 = {
        .probe          = allospi_probe,
        .remove         = allospi_remove,
        .driver = {
                .name   = "allospi1",
                .owner  = THIS_MODULE,
        },
};
static struct spi_driver allospi_driver2 = {
        .probe          = allospi_probe,
        .remove         = allospi_remove,
        .driver = {
                .name   = "allospi2",
                .owner  = THIS_MODULE,
        },
};


static int allospi_init(void)
{
	printk(KERN_ALERT "allospi module inserted.\n");
	spi_register_driver(&allospi_driver0);
	spi_register_driver(&allospi_driver1);
	spi_register_driver(&allospi_driver2);
return 0;
}
static void allospi_exit(void)
{
	printk(KERN_ALERT "allospi module removed.\n");
	spi_unregister_driver(&allospi_driver0);
	spi_unregister_driver(&allospi_driver1);
	spi_unregister_driver(&allospi_driver2);
}

module_init(allospi_init);
module_exit(allospi_exit);

MODULE_LICENSE("Dual BSD/GPL");

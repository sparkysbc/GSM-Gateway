#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/of_gpio.h>
#include <mach/gpio.h>
#include <mach/hardware.h>
#include <linux/delay.h>

#define RESET_GPIO OWL_GPIO_PORTB(21)
#define INT_GPIO OWL_GPIO_PORTA(24)
#define CONFIG_RESET_GPIO()	\
	do { \
		int val = act_readl(MFP_CTL1); \
		val = val | (0x3 << 5) ; \
        	act_setl( val, MFP_CTL1); \
	}while(0)

#define CONFIG_INTR_GPIO()	\
	do { \
        val = act_readl(INTC_GPIOCTL);\
        val=(val| (1<<2)); \
        act_writel(val, INTC_GPIOCTL); \
	}while(0)


int s500_eint_init(void)
{
	int rc,val;
	int irq;

	CONFIG_INTR_GPIO();

    	rc = gpio_request(INT_GPIO, "ts-gpio");
  	if (rc < 0) {
        	pr_info("%s %d: gpio_request failed\n", __func__, __LINE__);
  	}
        gpio_direction_input(INT_GPIO);
	val = __gpio_get_value(INT_GPIO);
	printk("gpio_val :%x\n",val);
        irq = gpio_to_irq(INT_GPIO);
	return irq;
}

int s500_fpga_reset(int request_gpio){
	int rc;

	printk("RESETING FPGA %s requesting gpio...\n", request_gpio?"with":"without");
	if(request_gpio){
		/*** configure the GPIOB pins as Digital function before using them as GPIOs ***/
		CONFIG_RESET_GPIO();

    		rc = gpio_request(RESET_GPIO, "ts-gpio");
  		if (rc < 0) {
        		pr_info("%s %d: gpio_request failed or already registered\n", __func__, __LINE__);
  		}
	}

        gpio_direction_output(RESET_GPIO, 1);
        msleep(100);
        gpio_direction_output(RESET_GPIO, 0);
	/* delay after reset for gsm modules to initialise and ready*/
	for (rc=0; rc<5; rc++)
        	msleep(1000);

	printk("RESETING FPGA DONE...\n");
return 0;
} 

int s500_eint_exit(int irq)
{
	printk(KERN_INFO "%s \n",__FUNCTION__);
        gpio_free(INT_GPIO);
        gpio_free(RESET_GPIO);
return 0;
}

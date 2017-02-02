
#ifndef __EINT_H__
#define __EINT_H__
int s500_eint_init(void);
int s500_eint_exit(int irq);
int s500_fpga_reset(int request_gpio);
#endif

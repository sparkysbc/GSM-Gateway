#ifndef __PRIVATE_H__
#define __PRIVATE_H__

void init_interrupt_deps(void);
void init_fpga(int ms_per_irq);
void stop_fpga(void);
struct device *  __allo_gsm_get_spidev(unsigned long mem32);
void __allo_gsm_set_chunk(void *readchunk, void *writechunk,unsigned int frq);
void __allo_gsm_transmit(unsigned long mem32, unsigned char *writechunk, unsigned char **txbuf,unsigned int irq_frq , unsigned int order);
void __allo_gsm_receive(unsigned long mem32, unsigned char *readchunk, unsigned char **rxbuf,unsigned int irq_frq , unsigned int order);
void __g4_outl__(unsigned int regno, unsigned char value, int flag);
unsigned char __g4_inl__( unsigned int regno, int flag);
unsigned int __allo_gsm_signaling_write(u8 *txbuf, unsigned int size, unsigned int regno);
unsigned int __allo_gsm_signaling_read(u8 *rxbuf, unsigned int size, unsigned int regno);
unsigned int __allo_gsm_pcm_write_read(unsigned char **txbuf, unsigned char **rxbuf, unsigned int size);

#endif /*__PRIVATE_H__*/

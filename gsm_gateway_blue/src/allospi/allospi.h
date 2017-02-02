#ifndef _ALLOSPI_H_
#define _ALLOSPI_H_
int allo_spi_write_direct(u8,int);
int allo_spi_write_direct_u32(u32,int);
int allo_spi_write(u8 *word,unsigned int size, int module);
int allo_spi_read_direct(int);
u32 allo_spi_read_direct_u32(int);
//int allo_spi_read(long unsigned int word, unsigned int size, int module);
int allo_spi_read(u8 *word, unsigned int size, int module);
int allo_spi_write_read(u8 **txbuf, u8 **rxbuf, unsigned int size, int module);
struct device * allo_spi_get_dev(int module);
#endif

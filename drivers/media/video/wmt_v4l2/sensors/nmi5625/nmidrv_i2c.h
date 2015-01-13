
#ifndef _NMIDRV_I2C_H_
#define _NMIDRV_I2C_H_

#include "nmidrv_custom_mtk.h"

//enternal interface
#ifndef NMI_HW_I2C
int nmi_i2c_init(void);
void nmi_i2c_deinit(void);
int nmi_i2c_read(unsigned char, unsigned char *, unsigned long);
int nmi_i2c_write(unsigned char, unsigned char *, unsigned long); 
#endif
#endif

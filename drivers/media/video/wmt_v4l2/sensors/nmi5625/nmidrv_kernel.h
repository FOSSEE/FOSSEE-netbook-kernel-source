#ifndef _NMIDRV_KERNEL_H_
#define _NMIDRV_KERNEL_H_

/**************************************************************
	IO Control Command defination:
**************************************************************/
#define NMI_POWER_VDDIO_CTL					0x10000000
#define NMI_POWER_VCORE_CTL					0x20000000
#define NMI_RESET_CTL 						0x30000000
#define NMI_I2C_READ						0x40000000
#define NMI_I2C_WRITE						0x50000000
#define NMI_FM_POWER_VCORE_CTL					0x60000000

/**************************************************************
	Debug Trace Defination:
**************************************************************/

#define DEBUG

#define N_INIT		0x00000001
#define N_ERR		0x00000002
#define N_FUNC		0x00000004
#define N_TRACE		0x00000008
#define N_INFO		0x00000010

static unsigned int dflag = N_INIT|N_ERR|N_FUNC|N_INFO;

#ifdef DEBUG
#define dPrint(f, str...) if (dflag & f) printk (str)
#else
#define dPrint(f, str...) /* nothing */
#endif

#define func_enter() dPrint (N_TRACE, "nmi: %s...enter\n", __func__)
#define func_exit()  dPrint(N_TRACE, "nmi: %s...exit\n", __func__)


#ifdef NMI_HW_I2C
#define NMI_I2C_RW_LENGTH	256
#endif

#endif

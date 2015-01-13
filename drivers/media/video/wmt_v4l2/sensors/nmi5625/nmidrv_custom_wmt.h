
#ifndef _NMIDRV_CUSTOM_H_
#define _NMIDRV_CUSTOM_H_
/******************************************************************************
**
**	Copyright (c) Newport Media Inc.  All rights reserved.
**
** 	Module Name:  nmidrv_custom.h
**	
**		This module implements the porting interface for the NMI ATV driver.
**		It can be used as an example for the driver porting.      
**
** 
*******************************************************************************/

#include <mach/wmt_iomux.h>

//if pin is unuse,please define it as 0xff
#define NMI_PIN_UNUSE   0xFF   

//step 1: configure NMI600 power pin and reset pin .
//if VDDIO and VCORE use each pin, define NMI_POWER_VDDIO_PIN & NMI_POWER_VCORE_PIN
//if VDDIO and VCORE use same pin,  only define NMI_POWER_VDDIO_PIN,
//and define NMI_POWER_VCORE_PIN as 0xff !!!
#define NMI_POWER_VDDIO_PIN            NMI_PIN_UNUSE
#define NMI_POWER_VCORE_PIN            WMT_PIN_GP0_GPIO2   

#define NMI_RESET_PIN                  WMT_PIN_GP62_SUSGPIO0


// step 2: configure NMI600 i2c bus .
//if use hardware I2C , please define NMI_HW_I2C
#define NMI_HW_I2C
#define NMI_HW_I2C_PORT 4
#define NMI_I2C_RW_LENGTH	256

//if use gpio i2c , please modify the defination below 
// note: when nmi600 power off, nmi600 iic sda and scl will be set to input mode. 
//       so if other devices share iic with nmi600, maybe i2c needs initilize again. 
#define NMI_SCL_PIN                    NMI_PIN_UNUSE
#define NMI_SDA_PIN                    NMI_PIN_UNUSE 

/******************************************************************************
**
**	(B) Nmi Function Prototype (for porting on MTK 65XX) 
**
*******************************************************************************/
#define NMI_SET_GPIO_MODE_ENABLE(PIN)  if(PIN!=NMI_PIN_UNUSE) \
	gpio_request(PIN, "ATV,DTV")

#define NMI_SET_GPIO_DIR(PIN,DIR)			\
do {							\
	if (PIN!=NMI_PIN_UNUSE) {			\
		if (DIR)				\
			gpio_direction_output(PIN, 1);	\
		else					\
			gpio_direction_input(PIN);	\
	}						\
} while (0)

#define NMI_SET_GPIO_PULL_DISABLE(PIN) if(PIN!=NMI_PIN_UNUSE) \
	wmt_gpio_setpull(PIN, WMT_GPIO_PULL_NONE)
#define NMI_SET_GPIO_PULL_ENABLE(PIN)  if(PIN!=NMI_PIN_UNUSE) \
	wmt_gpio_setpull(PIN, WMT_GPIO_PULL_UP)
#define NMI_SET_GPIO_LEVEL(PIN,LEVEL)  if(PIN!=NMI_PIN_UNUSE) \
	gpio_set_value(PIN, LEVEL)
#define NMI_GET_GPIO_LEVEL(PIN)  	   gpio_get_value(PIN)
										     

#endif  /*_NMIDRV_CUSTOM_H_*/

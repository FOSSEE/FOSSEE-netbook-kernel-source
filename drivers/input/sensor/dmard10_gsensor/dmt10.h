/* @version 1.03
 * Copyright 2011 Domintech Technology Co., Ltd
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */
 
#ifndef DMT10_H
#define DMT10_H
#include <linux/types.h>
#include <linux/ioctl.h>
#include <linux/cdev.h>
#include <linux/mutex.h>
#include <linux/syscalls.h>
#include <linux/wait.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
//#include <linux/earlysuspend.h>
#define AUTO_CALIBRATION	0
#define SW_FILTER				/* Enable or Disable Software filter */
#define SENSOR_DATA_AVG		8	/* AVG sensor data */

//#define DMT_DEBUG_DATA
#define GSE_TAG                  "[DMT_Gsensor]"
#ifdef DMT_DEBUG_DATA
#define GSE_ERR(fmt, args...)    printk(KERN_ERR GSE_TAG"%s %d : "fmt, __FUNCTION__, __LINE__, ##args)
#define GSE_LOG(fmt, args...)    printk(KERN_INFO GSE_TAG fmt, ##args)
#define GSE_FUN(f)               printk(KERN_INFO GSE_TAG" %s: %s: %i\n", __FILE__, __func__, __LINE__)
#define DMT_DATA(dev, ...)		 dev_dbg((dev), ##__VA_ARGS__)
#else
#define GSE_ERR(fmt, args...)
#define GSE_LOG(fmt, args...)
#define GSE_FUN(f)
#define DMT_DATA(dev, format, ...)
#endif

#define GSENSOR_ID				"DMARD10"
#define INPUT_NAME_ACC			"g-sensor"//"DMT_accel"//"g-sensor"//	/* Input Device Name  */
#define SENSOR_I2C_NAME 		"dmard10"//"dmt"//		/* Device name for DMARD10 misc. device */
#define SENSOR_I2C_ADDR			0x18
#define REG_ACTR 				0x00
#define REG_WDAL 				0x01
#define REG_TAPNS				0x0f
#define REG_MISC2				0x1f
#define REG_AFEM 				0x0c
#define REG_CKSEL 				0x0d
#define REG_INTC 				0x0e
#define REG_STADR 				0x12
#define REG_STAINT 				0x1C
#define REG_PD					0x21
#define REG_TCGYZ				0x26
#define REG_X_OUT 				0x41

#define MODE_Off				0x00
#define MODE_ResetAtOff			0x01
#define MODE_Standby			0x02
#define MODE_ResetAtStandby		0x03
#define MODE_Active				0x06
#define MODE_Trigger			0x0a
#define MODE_ReadOTP			0x12
#define MODE_WriteOTP			0x22
#define MODE_WriteOTPBuf		0x42
#define MODE_ResetDataPath		0x82

#define VALUE_STADR					0x55
#define VALUE_STAINT 				0xAA
#define VALUE_AFEM_AFEN_Normal		0x8f// AFEN set 1 , ATM[2:0]=b'000(normal),EN_Z/Y/X/T=1
#define VALUE_AFEM_Normal			0x0f// AFEN set 0 , ATM[2:0]=b'000(normal),EN_Z/Y/X/T=1
#define VALUE_INTC					0x00// INTC[6:5]=b'00 
#define VALUE_INTC_Interrupt_En		0x20// INTC[6:5]=b'01 (Data ready interrupt enable, active high at INT0)
#define VALUE_CKSEL_ODR_0_204		0x04// ODR[3:0]=b'0000 (0.78125Hz), CCK[3:0]=b'0100 (204.8kHZ)
#define VALUE_CKSEL_ODR_1_204		0x14// ODR[3:0]=b'0001 (1.5625Hz), CCK[3:0]=b'0100 (204.8kHZ)
#define VALUE_CKSEL_ODR_3_204		0x24// ODR[3:0]=b'0010 (3.125Hz), CCK[3:0]=b'0100 (204.8kHZ)
#define VALUE_CKSEL_ODR_6_204		0x34// ODR[3:0]=b'0011 (6.25Hz), CCK[3:0]=b'0100 (204.8kHZ)
#define VALUE_CKSEL_ODR_12_204		0x44// ODR[3:0]=b'0100 (12.5Hz), CCK[3:0]=b'0100 (204.8kHZ)
#define VALUE_CKSEL_ODR_25_204		0x54// ODR[3:0]=b'0101 (25Hz), CCK[3:0]=b'0100 (204.8kHZ)
#define VALUE_CKSEL_ODR_50_204		0x64// ODR[3:0]=b'0110 (50Hz), CCK[3:0]=b'0100 (204.8kHZ)
#define VALUE_CKSEL_ODR_100_204		0x74// ODR[3:0]=b'0111 (100Hz), CCK[3:0]=b'0100 (204.8kHZ)

#define VALUE_TAPNS_NoFilter	0x00	// TAP1/TAP2	NO FILTER
#define VALUE_TAPNS_Ave_2		0x11	// TAP1/TAP2	Average 2
#define VALUE_TAPNS_Ave_4		0x22	// TAP1/TAP2	Average 4
#define VALUE_TAPNS_Ave_8		0x33	// TAP1/TAP2	Average 8
#define VALUE_TAPNS_Ave_16		0x44	// TAP1/TAP2	Average 16
#define VALUE_TAPNS_Ave_32		0x55	// TAP1/TAP2	Average 32
#define VALUE_MISC2_OSCA_EN		0x08
#define VALUE_PD_RST			0x52

#define CONFIG_GSEN_CALIBRATION_GRAVITY_ON_Z_NEGATIVE 1
#define CONFIG_GSEN_CALIBRATION_GRAVITY_ON_Z_POSITIVE 2
#define CONFIG_GSEN_CALIBRATION_GRAVITY_ON_Y_NEGATIVE 3
#define CONFIG_GSEN_CALIBRATION_GRAVITY_ON_Y_POSITIVE 4
#define CONFIG_GSEN_CALIBRATION_GRAVITY_ON_X_NEGATIVE 5
#define CONFIG_GSEN_CALIBRATION_GRAVITY_ON_X_POSITIVE 6

#define AVG_NUM 				16
#define SENSOR_DATA_SIZE 		3 
#define DEFAULT_SENSITIVITY 	1024

#define IOCTL_MAGIC  0x09
#define SENSOR_RESET    		_IO(IOCTL_MAGIC, 0)
#define SENSOR_CALIBRATION   	_IOWR(IOCTL_MAGIC,  1, int[SENSOR_DATA_SIZE])
#define SENSOR_GET_OFFSET  		_IOR(IOCTL_MAGIC,  2, int[SENSOR_DATA_SIZE])
#define SENSOR_SET_OFFSET  		_IOWR(IOCTL_MAGIC,  3, int[SENSOR_DATA_SIZE])
#define SENSOR_READ_ACCEL_XYZ  	_IOR(IOCTL_MAGIC,  4, int[SENSOR_DATA_SIZE])
#define SENSOR_SETYPR  			_IOW(IOCTL_MAGIC,  5, int[SENSOR_DATA_SIZE])
#define SENSOR_GET_OPEN_STATUS	_IO(IOCTL_MAGIC,  6)
#define SENSOR_GET_CLOSE_STATUS	_IO(IOCTL_MAGIC,  7)
#define SENSOR_GET_DELAY		_IOR(IOCTL_MAGIC,  8, unsigned int*)
#define SENSOR_MAXNR 8
/* Default sensorlayout parameters */
#define D10_DEFAULT_POSITION	6

/* Transformation matrix for chip mounting position */
static const int dmt_position_map[][3][3] = {
    {	{ 1, 0,	0},	{ 0,-1,	0},	{ 0, 0,-1},	}, /* top/upper-left	*/
    {	{ 0, 1,	0},	{ 1, 0,	0},	{ 0, 0,-1},	}, /* top/lower-left	*/
    {	{-1, 0,	0},	{ 0, 1,	0}, { 0, 0,-1},	}, /* top/lower-right	*/
    {	{ 0,-1,	0},	{-1, 0,	0}, { 0, 0,-1},	}, /* top/upper-right	*/
    {	{-1, 0,	0},	{ 0,-1,	0}, { 0, 0, 1},	}, /* bottom/upper-right*/
    {	{ 0,-1,	0},	{-1, 0,	0}, { 0, 0, 1},	}, /* bottom/upper-left	*/
    {	{ 1, 0,	0},	{ 0, 1,	0}, { 0, 0, 1},	}, /* bottom/lower-right*/
    {	{ 0, 1,0},	{ 1, 0,	0}, { 0, 0, 1},	}, /* bottom/lower-left	*/
};

typedef union {
	struct {
		int	x;
		int	y;
		int	z;
	} u;
	int	v[SENSOR_DATA_SIZE];
} raw_data;

struct dmt_data {
	struct platform_device  *pdevice;
	struct device			*class_dev;
  	struct class 			*class;
  	struct input_dev 		*input;
	struct i2c_client 		*client;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend 	early_suspend;
#endif
	struct delayed_work 	delaywork;	
	struct work_struct 		work;	
	struct mutex 			data_mutex; 
	struct mutex			enable_mutex;	/* for suspend */
	raw_data 				last;			/* RawData */
	raw_data 				offset;			/* Offset */
#ifdef SW_FILTER
	int 					sum[SENSOR_DATA_SIZE];	/* SW_FILTER sum */
	int 					bufferave[3][32];
	s8 						aveflag;				/* FULL bufferave[][] */
	s8 						pointer;				/* last update data */
#endif
	wait_queue_head_t		open_wq;
	atomic_t				active;
	atomic_t 				delay;
	atomic_t 				enable;
	int		 				filter;
	int						position; /* must int type ,for Kconfig setup */
	atomic_t				addr;
#ifdef DMT_DEBUG_DATA
	struct mutex 			suspend_mutex;
	int 					suspend;
#endif
};

#define ACC_DATA_FLAG		0
#define MAG_DATA_FLAG		1
#define ORI_DATA_FLAG		2
#define DMT_NUM_SENSORS		3

/* ABS axes parameter range [um/s^2] (for input event) */
#define GRAVITY_EARTH		9806550
#define ABSMAX				(GRAVITY_EARTH * 2)
#define ABSMIN				(-GRAVITY_EARTH * 2)

#endif               

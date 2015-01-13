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
 
#ifndef DMT09_H
#define DMT09_H
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
#define SENSOR_DATA_AVG		4//8	/* AVG sensor data */

#define STABLE_VALUE_FUNCTION
#define RANGE_XYZ					40

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

#define GSENSOR_ID				"DMARD09"
#define INPUT_NAME_ACC			"g-sensor"//"DMT_accel"//"g-sensor"//	/* Input Device Name  */
#define SENSOR_I2C_NAME 		"dmard09"//"dmt"//		/* Device name for DMARD09 misc. device */
#define DEVICE_I2C_ADDR			0x1d
#define REG_ACTR 				0x00
#define REG_STAT 				0x0A
#define REG_DX					0x0C
#define REG_DY					0x0E
#define REG_DZ	 				0x10
#define REG_DT	 				0x12
#define REG_INL 				0x16
#define REG_DC	 				0x18
#define REG_CNT_L1 				0x1B
#define REG_CNT_L2				0x1C
#define REG_CNT_L3				0x1D
#define REG_INC 				0x1E
#define REG_ODF 				0x20
#define REG_THR1 				0x62
#define REG_THR2 				0x64

#define MODE_ACTIVE				0x61	/* active */
#define MODE_POWERDOWN			0x60	/* powerdown */

#define VALUE_WHO_AM_I			0x95	/* D09 WMI */
#define VALUE_ODR_200			0x9C	/* conversion rate 200Hz	*/
#define VALUE_ODR_100			0x98	/* conversion rate 100Hz	*/
#define VALUE_ODR_50			0x94	/* conversion rate 50Hz	*/
#define VALUE_ODR_20			0x90	/* conversion rate 20Hz	*/
#define VALUE_ODR_10			0x8C	/* conversion rate 10Hz	*/
#define VALUE_ODR_5				0x88	/* conversion rate 5Hz	*/
#define VALUE_ODR_1				0x84	/* conversion rate 1Hz	*/
#define VALUE_ODR_0_5			0x80	/* conversion rate 0.5Hz	*/
#define VALUE_CNT_L2			0xE4	/* Disable IEN	*/
/* Optional Digital Filter [Low Byte and High Byte Order] */
#define	ODF_NoFilter	0x00	/* No filter */
#define	ODF_Ave_4		0x03	/* smooth filter 1/4 Bandwidth */
#define	ODF_Ave_8		0x07	/* smooth filter 1/8 Bandwidth */
#define	ODF_Ave_16		0x0f	/* smooth filter 1/16 Bandwidth */


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
#define D09_DEFAULT_POSITION	6

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

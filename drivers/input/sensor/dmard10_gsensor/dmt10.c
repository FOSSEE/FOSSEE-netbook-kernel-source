/*
 *	@file drivers/misc/dmt10.c
 *	@brief DMT g-sensor Linux device driver
 *	@author Domintech Technology Co., Ltd (http://www.domintech.com.tw)
 *	@version 1.06
 *	@date 2013/08/14
 *	@section LICENSE
 *
 *  Copyright 2012 Domintech Technology Co., Ltd
 *
 * 	This software is licensed under the terms of the GNU General Public
 * 	License version 2, as published by the Free Software Foundation, and
 * 	may be copied, distributed, and modified under those terms.
 *
 * 	This program is distributed in the hope that it will be useful,
 * 	but WITHOUT ANY WARRANTY; without even the implied warranty of
 * 	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * 	GNU General Public License for more details.
 *
 */
#include "dmt10.h"
#include <linux/module.h>
#include <linux/input.h>
#include <linux/i2c.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/wakelock.h>
#include <asm/uaccess.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/miscdevice.h>
#include <linux/clk.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/string.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>

#include "../sensor.h"

//////////////////////////////////////////////////////////
static struct wmt_gsensor_data l_sensorconfig = {
	.op = 0,
	.int_gpio = 3,
	.samp = 5,
	.xyz_axis = {
		{ABS_X, -1},
		{ABS_Y, 1},
		{ABS_Z, -1},
		},
	.sensor_proc = NULL,
	.isdbg = 0,
	.sensor_samp = 10,  // 1 sample/second
	.sensor_enable = 1, // enable sensor
	.test_pass = 0, // for test program
	.offset={0,0,0},
};

static struct class* l_dev_class = NULL;
static void update_var(void);

////////////////////////////////////////////////////////////


static unsigned int interval;
static int D10_write_offset_to_file(struct i2c_client *client);
void D10_read_offset_from_file(struct i2c_client *client);
#define DMT_BROADCAST_APK_ENABLE
char D10_OffsetFileName[] = "/data/misc/gsensor_offset.txt";	/* FILE offset.txt */
char DmtXXFileName[] = "/data/misc/dmt_sensor.txt";
static int create_devidfile(void);
static struct dmt_data *s_dmt;
static int device_init(void);
static void device_exit(void);

static int device_open(struct inode*, struct file*);
static long device_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
static int device_close(struct inode*, struct file*);

static int dmard10_suspend(struct platform_device *pdev, pm_message_t state);
static int dmard10_resume(struct platform_device *pdev);

/*static int device_i2c_suspend(struct i2c_client *client, pm_message_t mesg);
static int device_i2c_resume(struct i2c_client *client);
static int __devinit device_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int __devexit device_i2c_remove(struct i2c_client *client);*/
static int D10_i2c_read_xyz(struct i2c_client *client, int *xyz);
static int device_i2c_rxdata(struct i2c_client *client, unsigned char *rxDat, int length);
static int device_i2c_txdata(struct i2c_client *client, unsigned char *txData, int length);

static int dmt_get_filter(struct i2c_client *client);
static int dmt_set_filter(struct i2c_client *client,int);
static int dmt_get_position(struct i2c_client *client);
static int dmt_set_position(struct i2c_client *client,int);
static int DMT_GetOpenStatus(struct i2c_client *client){
	struct dmt_data *dmt = i2c_get_clientdata(client);	
	GSE_LOG("start active=%d\n",dmt->active.counter);
	wait_event_interruptible(dmt->open_wq, (atomic_read(&dmt->active) != 0));
	return 0;
}

static int DMT_GetCloseStatus(struct i2c_client *client){
	struct dmt_data *dmt = i2c_get_clientdata(client);
	GSE_LOG("start active=%d\n",dmt->active.counter);
	wait_event_interruptible(dmt->open_wq, (atomic_read(&dmt->active) <= 0));
	return 0;
}

static void DMT_sysfs_update_active_status(struct dmt_data *dmt , int en){
	unsigned long dmt_delay;
	if(en){
		dmt_delay=msecs_to_jiffies(atomic_read(&dmt->delay));
		if(dmt_delay<1)
			dmt_delay=1;

		GSE_LOG("schedule_delayed_work start with delay time=%lu\n",dmt_delay);
		schedule_delayed_work(&dmt->delaywork,dmt_delay);
	}
	else 
		cancel_delayed_work_sync(&dmt->delaywork);
}

static bool get_value_as_int(char const *buf, size_t size, int *value){
	long tmp;
	if (size == 0)
		return false;
	/* maybe text format value */
	if ((buf[0] == '0') && (size > 1)) {
		if ((buf[1] == 'x') || (buf[1] == 'X')) {
			/* hexadecimal format */
			if (0 != strict_strtol(buf, 16, &tmp))
				return false;
		} else {
			/* octal format */
			if (0 != strict_strtol(buf, 8, &tmp))
				return false;
		}
	} else {
		/* decimal format */
		if (0 != strict_strtol(buf, 10, &tmp))
			return false;
	}

	if (tmp > INT_MAX)
		return false;

	*value = tmp;
	return true;
}
static bool get_value_as_int64(char const *buf, size_t size, long long *value)
{
	long long tmp;
	if (size == 0)
		return false;
	/* maybe text format value */
	if ((buf[0] == '0') && (size > 1)) {
		if ((buf[1] == 'x') || (buf[1] == 'X')) {
			/* hexadecimal format */
			if (0 != strict_strtoll(buf, 16, &tmp))
				return false;
		} else {
			/* octal format */
			if (0 != strict_strtoll(buf, 8, &tmp))
				return false;
		}
	} else {
		/* decimal format */
		if (0 != strict_strtoll(buf, 10, &tmp))
			return false;
	}

	if (tmp > LLONG_MAX)
		return false;

	*value = tmp;
	return true;
}
/* sysfs enable show & store */
static ssize_t dmt_sysfs_enable_show(
	struct dmt_data *dmt, char *buf, int pos)
{
	char str[2][16]={"ACC enable OFF","ACC enable ON"};
	int flag;
	flag=atomic_read(&dmt->enable); 	
	return sprintf(buf, "%s\n", str[flag]);
}

static ssize_t dmt_sysfs_enable_store(
	struct dmt_data *dmt, char const *buf, size_t count, int pos)
{
	int en = 0;
	if (NULL == buf)
		return -EINVAL;
	//GSE_LOG("buf=%x %x\n", buf[0], buf[1]);
	if (0 == count)
		return 0;

	if (false == get_value_as_int(buf, count, &en))
		return -EINVAL;

	en = en ? 1 : 0;

	atomic_set(&dmt->enable,en);
	DMT_sysfs_update_active_status(dmt , en);
	return count;
}

static ssize_t dmt_enable_show(struct device *dev, struct device_attribute *attr, char *buf){
	return dmt_sysfs_enable_show( dev_get_drvdata(dev), buf, ACC_DATA_FLAG);
}

static ssize_t dmt_enable_store( struct device *dev, struct device_attribute *attr, char const *buf, size_t count){
	return dmt_sysfs_enable_store( dev_get_drvdata(dev), buf, count, ACC_DATA_FLAG);
}

/* sysfs delay show & store*/
static ssize_t dmt_sysfs_delay_show( struct dmt_data *dmt, char *buf, int pos){
	return sprintf(buf, "%d\n", atomic_read(&dmt->delay));
}

static ssize_t dmt_sysfs_delay_store( struct dmt_data *dmt, char const *buf, size_t count, int pos){
	long long val = 0;

	if (NULL == buf)
		return -EINVAL;

	if (0 == count)
		return 0;

	if (false == get_value_as_int64(buf, count, &val))
		return -EINVAL;

	atomic_set(&dmt->delay, (unsigned int) val);
	GSE_LOG("Driver attribute set delay =%lld\n", val);

	return count;
}

static ssize_t dmt_delay_show( struct device *dev,
					struct device_attribute *attr, 
					char *buf)
{
	return dmt_sysfs_delay_show( dev_get_drvdata(dev), buf, ACC_DATA_FLAG);
}

static ssize_t dmt_delay_store( struct device *dev,
					struct device_attribute *attr,
					char const *buf,
					size_t count)
{
	return dmt_sysfs_delay_store( dev_get_drvdata(dev), buf, count, ACC_DATA_FLAG);
}
/* sysfs position show & store */
static ssize_t dmt_position_show(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct dmt_data *dmt = input_get_drvdata(input);

	return sprintf(buf, "%d\n", dmt_get_position(dmt->client));
}

static ssize_t dmt_position_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf,
				      size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct dmt_data *dmt = input_get_drvdata(input);
	unsigned long position;
	int ret;

	ret = strict_strtoul(buf, 10, &position);
	if (ret < 0)
		return count;

	dmt_set_position(dmt->client, position);
	return count;
}
/* sysfs offset show & store */
static ssize_t dmt_offset_show(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct dmt_data *dmt = input_get_drvdata(input);
	return sprintf(buf, "( %d %d %d )\n", dmt->offset.u.x, dmt->offset.u.y, dmt->offset.u.z);
}

static ssize_t dmt_offset_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf,
				    size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct dmt_data *dmt = input_get_drvdata(input);
	sscanf(buf, "%d %d %d", (int *)&dmt->offset.v[0], (int *)&dmt->offset.v[1], (int *)&dmt->offset.v[2]);
	D10_write_offset_to_file(dmt->client);
	update_var();
	return count;
}
/* sysfs filter show & store */
static ssize_t dmt_filter_show(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct dmt_data *dmt = input_get_drvdata(input);

	return sprintf(buf, "%d\n", dmt_get_filter(dmt->client));
}

static ssize_t dmt_filter_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf,
				      size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct dmt_data *dmt = input_get_drvdata(input);
	unsigned long filter;
	int ret;

	ret = strict_strtoul(buf, 10, &filter);
	if (ret < 0)
		return count;

	dmt_set_filter(dmt->client, filter);
	return count;
}

/* sysfs data show */
static ssize_t dmt_acc_private_data_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct dmt_data *dmt = input_get_drvdata(input);
	raw_data accel;
	
	mutex_lock(&dmt->data_mutex);
	accel = dmt->last;
	mutex_unlock(&dmt->data_mutex);

	return sprintf(buf, "( %d %d %d )\n", dmt->last.v[0], dmt->last.v[1], dmt->last.v[2]);
}
/* sysfs id show */
static ssize_t dmt_id_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	char str[8]={GSENSOR_ID};	
	return sprintf(buf, "%s\n", str);
}
/* sysfs debug_suspend show & store */
#ifdef DMT_DEBUG_DATA
static ssize_t dmt_debug_suspend_show(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct dmt_data *dmt = input_get_drvdata(input);
	int suspend = dmt->suspend;

	mutex_lock(&dmt->suspend_mutex);
	suspend = sprintf(buf, "%d\n", dmt->suspend);
	mutex_unlock(&dmt->suspend_mutex);
	return suspend;
}

static ssize_t dmt_debug_suspend_store(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct dmt_data *dmt = input_get_drvdata(input);
	unsigned long suspend;
	pm_message_t msg;
	int ret;

	ret = strict_strtoul(buf, 10, &suspend);
	if (ret < 0)
		return count;

	memset(&msg, 0, sizeof(pm_message_t));

	mutex_lock(&dmt->suspend_mutex);

	if (suspend) {
		dmard10_suspend(dmt->pdevice, msg);
		dmt->suspend = 1;
	} else {
		dmard10_resume(dmt->pdevice);
		dmt->suspend = 0;
	}

	mutex_unlock(&dmt->suspend_mutex);

	return count;
}
/* sysfs reg_read show & store */
static ssize_t dmt_reg_read_show(struct device *dev,
						struct device_attribute *attr,
						char *buf)
{
	struct dmt_data *dmt = dev_get_drvdata(dev);
	int err;
	unsigned char i2c[1];

	i2c[0] = (unsigned char)atomic_read(&dmt->addr);
	err = device_i2c_rxdata(dmt->client, i2c, 1);
	if (err < 0)
		return err;

	return sprintf(buf, "0x%02X\n", i2c[0]);
}

static ssize_t dmt_reg_read_store(struct device *dev,
						struct device_attribute *attr,
						char const *buf,
						size_t count)
{
	struct dmt_data *dmt = dev_get_drvdata(dev);
	int addr = 0;

	if (NULL == buf)
		return -EINVAL;

	if (0 == count)
		return 0;

	if (false == get_value_as_int(buf, count, &addr))
		return -EINVAL;

	if (addr < 0 || 128 < addr)
		return -EINVAL;

	atomic_set(&dmt->addr, addr);

	return 1;
}
#endif /* DEBUG */
/*********************************************************************
 *
 * SysFS attribute functions
 *
 * directory : /sys/class/accelemeter/dmardXX/
 * files :
 *  - enable_acc	[rw]	[t] : enable flag for accelerometer
 *  - delay_acc		[rw]	[t] : delay in nanosecond for accelerometer
 *  - position		[rw]	[t] : chip mounting position
 *  - offset		[rw]	[t] : offset
 *  - data			[r]		[t] : raw data
 *  - id			[r]		[t] : chip id
 *
 * debug :
 *  - debug_suspend	[w]		[t] : suspend test
 *  - reg_read		[rw] 	[t] : Read register
 *  - reg_write		[rw] 	[t] : Weite register
 *
 * [rw]= read/write
 * [r] = read only
 * [w] = write only
 * [b] = binary format
 * [t] = text format
 */

static struct device_attribute DMT_attributes[] = {
	__ATTR(enable_acc,		0660, dmt_enable_show,				dmt_enable_store),
	__ATTR(delay_acc,		0660, dmt_delay_show,				dmt_delay_store),
	__ATTR(position,		0660, dmt_position_show,			dmt_position_store),
	__ATTR(offset,			0660, dmt_offset_show,				dmt_offset_store),
	__ATTR(filter,			0660, dmt_filter_show,				dmt_filter_store),
	__ATTR(data,			0660, dmt_acc_private_data_show,	NULL),
	__ATTR(id,				0660, dmt_id_show,  				NULL),
#ifdef DMT_DEBUG_DATA
	__ATTR(debug_suspend,	0660, dmt_debug_suspend_show,dmt_debug_suspend_store),
	__ATTR(reg_read,		0660, dmt_reg_read_show, dmt_reg_read_store),
	__ATTR(reg_write,		0660, NULL, NULL),
#endif // DEBUG 	
	__ATTR_NULL,
};

static char const *const ACCELEMETER_CLASS_NAME = "accelemeter";
static char const *const GSENSOR_DEVICE_NAME = SENSOR_I2C_NAME;
static char const *const device_link_name = "i2c";
static dev_t const dmt_device_dev_t = MKDEV(MISC_MAJOR, MISC_DYNAMIC_MINOR);

// dmt sysfs functions 
static int create_device_attributes(struct device *dev,	struct device_attribute *attrs){
	int i;
	int err = 0;
	for (i = 0 ; NULL != attrs[i].attr.name ; ++i) {
		err = device_create_file(dev, &attrs[i]);
		if (0 != err)
			break;
	}

	if (0 != err) {
		for (; i >= 0 ; --i)
			device_remove_file(dev, &attrs[i]);
	}
	return err;
}

static void remove_device_attributes(
	struct device *dev,
	struct device_attribute *attrs)
{
	int i;

	for (i = 0 ; NULL != attrs[i].attr.name ; ++i)
		device_remove_file(dev, &attrs[i]);
}

static int create_sysfs_interfaces(struct dmt_data *dmt)
{
	int err;

	if (NULL == dmt)
		return -EINVAL;

	err = 0;
	dmt->class = class_create(THIS_MODULE, ACCELEMETER_CLASS_NAME);
	if (IS_ERR(dmt->class)) {
		err = PTR_ERR(dmt->class);
		goto exit_class_create_failed;
	}

	dmt->class_dev = device_create(
						dmt->class,
						NULL,
						dmt_device_dev_t,
						dmt,
						GSENSOR_DEVICE_NAME);
	if (IS_ERR(dmt->class_dev)) {
		err = PTR_ERR(dmt->class_dev);
		goto exit_class_device_create_failed;
	}

	err = sysfs_create_link(
			&dmt->class_dev->kobj,
			&dmt->client->dev.kobj,
			device_link_name);
	if (0 > err)
		goto exit_sysfs_create_link_failed;

	err = create_device_attributes(
			dmt->class_dev,
			DMT_attributes);
	if (0 > err)
		goto exit_device_attributes_create_failed;
#if 0
	err = create_device_binary_attributes(
			&dmt->class_dev->kobj,
			dmt_bin_attributes);
	if (0 > err)
		goto exit_device_binary_attributes_create_failed;
#endif

	return err;

#if 0
exit_device_binary_attributes_create_failed:
	remove_device_attributes(dmt->class_dev, dmt_attributes);
#endif
exit_device_attributes_create_failed:
	sysfs_remove_link(&dmt->class_dev->kobj, device_link_name);
exit_sysfs_create_link_failed:
	device_destroy(dmt->class, dmt_device_dev_t);
exit_class_device_create_failed:
	dmt->class_dev = NULL;
	class_destroy(dmt->class);
exit_class_create_failed:
	dmt->class = NULL;
	return err;
}

static void remove_sysfs_interfaces(struct dmt_data *dmt){
	if (NULL == dmt)
		return;

	if (NULL != dmt->class_dev) {

		remove_device_attributes(
			dmt->class_dev,
			DMT_attributes);
		sysfs_remove_link(
			&dmt->class_dev->kobj,
			device_link_name);
		dmt->class_dev = NULL;
	}
	if (NULL != dmt->class) {
		device_destroy(
			dmt->class,
			dmt_device_dev_t);
		class_destroy(dmt->class);
		dmt->class = NULL;
	}
}

int D10_input_init(struct i2c_client *client){
	struct dmt_data *dmt = i2c_get_clientdata(client);
	int err = 0;
	dmt->input = input_allocate_device();
	if (!dmt->input){
		GSE_ERR("input device allocate ERROR !!\n");
		return -ENOMEM;
	}
	else
		GSE_LOG("input device allocate Success !!\n");
	/* Setup input device */
	//dmt->input->name = SENSOR_I2C_NAME;
	set_bit(EV_ABS, dmt->input->evbit);
	/* Accelerometer [-78.5, 78.5]m/s2 in Q16 */
	input_set_abs_params(dmt->input, ABS_X, ABSMIN, ABSMAX, 0, 0);
	input_set_abs_params(dmt->input, ABS_Y, ABSMIN, ABSMAX, 0, 0);
	input_set_abs_params(dmt->input, ABS_Z, ABSMIN, ABSMAX, 0, 0);
	/* Set InputDevice Name */
	dmt->input->name = INPUT_NAME_ACC;
	/* Register */
	err = input_register_device(dmt->input);
	if (err) {
		GSE_ERR("input_register_device ERROR !!\n");
		input_free_device(dmt->input);
		return err;
	}
	GSE_LOG("input_register_device SUCCESS %d !! \n",err);

	return err;
}

int D10_calibrate(struct i2c_client *client)
{	
	struct dmt_data *dmt = i2c_get_clientdata(client);
	raw_data avg;
	int i, j;
	long xyz_acc[SENSOR_DATA_SIZE];   
  	int xyz[SENSOR_DATA_SIZE];
	/* initialize the offset value */
	for(i = 0; i < SENSOR_DATA_SIZE; ++i)
		dmt->offset.v[i] = 0;
	/* initialize the accumulation buffer */
  	for(i = 0; i < SENSOR_DATA_SIZE; ++i) 
		xyz_acc[i] = 0;

	for(i = 0; i < AVG_NUM; i++) {      
		D10_i2c_read_xyz(client, (int *)&xyz);
		for(j = 0; j < SENSOR_DATA_SIZE; ++j) 
			xyz_acc[j] += xyz[j];
  	}
	/* calculate averages */
  	for(i = 0; i < SENSOR_DATA_SIZE; ++i) 
		avg.v[i] = xyz_acc[i] / AVG_NUM;
		
	if(avg.v[2] < 0){
		dmt->offset.u.x =  avg.v[0] ;    
		dmt->offset.u.y =  avg.v[1] ;
		dmt->offset.u.z =  avg.v[2] + DEFAULT_SENSITIVITY;
		return CONFIG_GSEN_CALIBRATION_GRAVITY_ON_Z_POSITIVE;
	}
	else{	
		dmt->offset.u.x =  avg.v[0] ;    
		dmt->offset.u.y =  avg.v[1] ;
		dmt->offset.u.z =  avg.v[2] - DEFAULT_SENSITIVITY;
		return CONFIG_GSEN_CALIBRATION_GRAVITY_ON_Z_NEGATIVE;
	}
	return 0;
}

int dmard10_init(struct i2c_client *client){
	unsigned char buffer[7], buffer2[2];
	/* 1. check D10 , VALUE_STADR = 0x55 , VALUE_STAINT = 0xAA */
	buffer[0] = REG_STADR;
	buffer2[0] = REG_STAINT;
	
	device_i2c_rxdata(client, buffer, 2);
	device_i2c_rxdata(client, buffer2, 2);
		
	if( buffer[0] == VALUE_STADR || buffer2[0] == VALUE_STAINT){
		GSE_LOG(" REG_STADR_VALUE = %d , REG_STAINT_VALUE = %d\n", buffer[0], buffer2[0]);
	}
	else{
		GSE_LOG(" REG_STADR_VALUE = %d , REG_STAINT_VALUE = %d \n", buffer[0], buffer2[0]);
		return -1;
	}
	/* 2. Powerdown reset */
	buffer[0] = REG_PD;
	buffer[1] = VALUE_PD_RST;
	device_i2c_txdata(client, buffer, 2);
	/* 3. ACTR => Standby mode => Download OTP to parameter reg => Standby mode => Reset data path => Standby mode */
	buffer[0] = REG_ACTR;
	buffer[1] = MODE_Standby;
	buffer[2] = MODE_ReadOTP;
	buffer[3] = MODE_Standby;
	buffer[4] = MODE_ResetDataPath;
	buffer[5] = MODE_Standby;
	device_i2c_txdata(client, buffer, 6);
	/* 4. OSCA_EN = 1 ,TSTO = b'000(INT1 = normal, TEST0 = normal) */
	buffer[0] = REG_MISC2;
	buffer[1] = VALUE_MISC2_OSCA_EN;
	device_i2c_txdata(client, buffer, 2);
	/* 5. AFEN = 1(AFE will powerdown after ADC) */
	buffer[0] = REG_AFEM;
	buffer[1] = VALUE_AFEM_AFEN_Normal;	
	buffer[2] = VALUE_CKSEL_ODR_100_204;	
	buffer[3] = VALUE_INTC;	
	buffer[4] = VALUE_TAPNS_Ave_4;
	buffer[5] = 0x00;	// DLYC, no delay timing
	buffer[6] = 0x07;	// INTD=1 (push-pull), INTA=1 (active high), AUTOT=1 (enable T)
	device_i2c_txdata(client, buffer, 7);
	/* 6. write TCGYZ & TCGX */
	buffer[0] = REG_WDAL;	// REG:0x01
	buffer[1] = 0x00;		// set TC of Y,Z gain value
	buffer[2] = 0x00;		// set TC of X gain value
	buffer[3] = 0x03;		// Temperature coefficient of X,Y,Z gain
	device_i2c_txdata(client, buffer, 4);
	
	buffer[0] = REG_ACTR;			// REG:0x00
	buffer[1] = MODE_Standby;		// Standby
	buffer[2] = MODE_WriteOTPBuf;	// WriteOTPBuf 
	buffer[3] = MODE_Standby;		// Standby
	device_i2c_txdata(client, buffer, 4);	
	//buffer[0] = REG_TCGYZ;
	//device_i2c_rxdata(client, buffer, 2);
	//GSE_LOG(" TCGYZ = %d, TCGX = %d  \n", buffer[0], buffer[1]);
	
	/* 7. Activation mode */
	buffer[0] = REG_ACTR;
	buffer[1] = MODE_Active;
	device_i2c_txdata(client, buffer, 2);
	return 0;
}

void D10_set_offset(struct i2c_client *client, int val[3]){
	struct dmt_data *dmt = i2c_get_clientdata(client);
	int i;
	for(i = 0; i < SENSOR_DATA_SIZE; ++i)
		dmt->offset.v[i] = val[i];
}

struct file_operations sensor_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = device_ioctl,
	.open = device_open,
	.release = device_close,
};

static struct miscdevice dmt_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = SENSOR_I2C_NAME,
	.fops = &sensor_fops,
};

static int sensor_close_dev(struct i2c_client *client){    	
	char buffer[3];
	GSE_FUN();
	buffer[0] = REG_AFEM;
	buffer[1] = 0x0f;
	device_i2c_txdata(client,buffer, 2);
	buffer[0] = REG_ACTR;
	buffer[1] = MODE_Standby;
	buffer[2] = MODE_Off;
	device_i2c_txdata(client,buffer, 3);	
	return 0;
}

static void dmard10_shutdown(struct platform_device *pdev)
{
	flush_delayed_work_sync(&s_dmt->delaywork);
	DMT_sysfs_update_active_status(s_dmt , 0);	
}


//static int device_i2c_suspend(struct i2c_client *client, pm_message_t mesg){
static int dmard10_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct dmt_data *dmt = i2c_get_clientdata(s_dmt->client);
	flush_delayed_work_sync(&dmt->delaywork);
	DMT_sysfs_update_active_status(dmt , 0);
	return sensor_close_dev(dmt->client);
}

//static int device_i2c_resume(struct i2c_client *client){
static int dmard10_resume(struct platform_device *pdev)
{
	struct dmt_data *dmt = i2c_get_clientdata(s_dmt->client);
	int en = 1;
	GSE_FUN();
	printk("dmt->enable=%d",dmt->enable);
	dmard10_init(dmt->client);
	atomic_set(&dmt->enable,en);
	DMT_sysfs_update_active_status(dmt , en);
	return 0;
}
/*
static int __devexit device_i2c_remove(struct i2c_client *client){
	return 0;
}

static const struct i2c_device_id device_i2c_ids[] = {
	{ SENSOR_I2C_NAME, 0},
	{ }   
};

static struct i2c_driver device_i2c_driver = {
	.driver	= {
		.owner = THIS_MODULE,
		.name = SENSOR_I2C_NAME,
		},
	.class = I2C_CLASS_HWMON,
	.id_table = device_i2c_ids,
	.probe = device_i2c_probe,
	.remove	= __devexit_p(device_i2c_remove),
#ifdef CONFIG_HAS_EARLYSUSPEND
	.suspend = device_i2c_suspend,
	.resume	= device_i2c_resume,
#endif	
};
*/
static int device_open(struct inode *inode, struct file *filp){
	return 0; 
}

static long device_ioctl(struct file *file, unsigned int cmd, unsigned long arg){
	//struct i2c_client *client = (struct i2c_client *)file->private_data;
	//struct dmt_data *dmt = (struct dmt_data*)i2c_get_clientdata(client);	
	
	int err = 0, ret = 0, i;
	int intBuf[SENSOR_DATA_SIZE], xyz[SENSOR_DATA_SIZE];
	/* check type */
	if (_IOC_TYPE(cmd) != IOCTL_MAGIC) return -ENOTTY;

	/* check user space pointer is valid */
	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		err =  !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
	if (err) return -EFAULT;
	
	switch(cmd) {
		case SENSOR_RESET:
			ret = dmard10_init(s_dmt->client);
			return ret;

		case SENSOR_CALIBRATION:
			/* get orientation info */
			//if(copy_from_user(&intBuf, (int*)arg, sizeof(intBuf))) return -EFAULT;
			D10_calibrate(s_dmt->client);
			GSE_LOG("Sensor_calibration:%d %d %d\n", s_dmt->offset.u.x, s_dmt->offset.u.y, s_dmt->offset.u.z);
			/* save file */
			D10_write_offset_to_file(s_dmt->client);
			update_var();
			
			/* return the offset */
			for(i = 0; i < SENSOR_DATA_SIZE; ++i)
				intBuf[i] = s_dmt->offset.v[i];

			ret = copy_to_user((int *)arg, &intBuf, sizeof(intBuf));
			return ret;
		
		case SENSOR_GET_OFFSET:
			/* get data from file */
			D10_read_offset_from_file(s_dmt->client);
			for(i = 0; i < SENSOR_DATA_SIZE; ++i)
				intBuf[i] = s_dmt->offset.v[i];

			ret = copy_to_user((int *)arg, &intBuf, sizeof(intBuf));
			return ret;

		case SENSOR_SET_OFFSET:
			ret = copy_from_user(&intBuf, (int *)arg, sizeof(intBuf));
			D10_set_offset(s_dmt->client , intBuf);
			/* write into file */
			D10_write_offset_to_file(s_dmt->client);
			update_var();
			return ret;
		
		case SENSOR_READ_ACCEL_XYZ:
			D10_i2c_read_xyz(s_dmt->client, (int *)&xyz);
			for(i = 0; i < SENSOR_DATA_SIZE; ++i)
				intBuf[i] = xyz[i] - s_dmt->offset.v[i];
			
		  	ret = copy_to_user((int*)arg, &intBuf, sizeof(intBuf));
			return ret;
			
		case SENSOR_SETYPR:
			if(copy_from_user(&intBuf, (int*)arg, sizeof(intBuf))) {
				GSE_LOG("%s: -EFAULT\n",__func__);			
				return -EFAULT;
			}
			input_report_abs(s_dmt->input, ABS_X, intBuf[0]);
			input_report_abs(s_dmt->input, ABS_Y, intBuf[1]);
			input_report_abs(s_dmt->input, ABS_Z, intBuf[2]);
			input_sync(s_dmt->input);
			GSE_LOG("SENSOR_SETYPR OK! x=%d,y=%d,z=%d\n",intBuf[0],intBuf[1],intBuf[2]);
			return ret;
			
		case SENSOR_GET_OPEN_STATUS:
			GSE_LOG("Going into DMT_GetOpenStatus()\n");
			ret = DMT_GetOpenStatus(s_dmt->client);
			return ret;
			
		case SENSOR_GET_CLOSE_STATUS:
			GSE_LOG("Going into DMT_GetCloseStatus()\n");
			ret = DMT_GetCloseStatus(s_dmt->client);	
			return ret;
				
		case SENSOR_GET_DELAY:
		  	ret = copy_to_user((int*)arg, &interval, sizeof(interval));
			return ret;
		
		default:  /* redundant, as cmd was checked against MAXNR */
			return -ENOTTY;
	}
	
	return 0;
}
	
static int device_close(struct inode *inode, struct file *filp){
	return 0;
}

/***** I2C I/O function ***********************************************/
static int device_i2c_rxdata( struct i2c_client *client, unsigned char *rxData, int length){
	struct i2c_msg msgs[] = {
		{.addr = client->addr, .flags = 0, .len = 1, .buf = rxData,}, 
		{.addr = client->addr, .flags = I2C_M_RD, .len = length, .buf = rxData,},
	};
	//unsigned char addr = rxData[0];
	if (i2c_transfer(client->adapter, msgs, 2) < 0) {
		dev_err(&client->dev, "%s: transfer failed.", __func__);
		return -EIO;
	}
	//DMT_DATA(&client->dev, "RxData: len=%02x, addr=%02x, data=%02x\n",
		//length, addr, rxData[0]);

	return 0;
}

static int device_i2c_txdata( struct i2c_client *client, unsigned char *txData, int length){
	struct i2c_msg msg[] = {
		{.addr = client->addr, .flags = 0, .len = length, .buf = txData,}, 
	};

	if (i2c_transfer(client->adapter, msg, 1) < 0) {
		dev_err(&client->dev, "%s: transfer failed.", __func__);
		return -EIO;
	}
	//DMT_DATA(&client->dev, "TxData: len=%02x, addr=%02x data=%02x\n",
		//length, txData[0], txData[1]);
	return 0;
}

static int D10_i2c_read_xyz(struct i2c_client *client, int *xyz_p){
	struct dmt_data *dmt = i2c_get_clientdata(client);
	u8 buffer[11];
	s16 xyzTmp[SENSOR_DATA_SIZE];
	int pos = dmt->position;
	int i, j , k;
	/* get xyz high/low bytes, 0x12 */
	buffer[0] = REG_STADR;
	/* Read acceleration data */
	if (device_i2c_rxdata(client, buffer, 10)!= 0)
		for(i = 0; i < SENSOR_DATA_SIZE; ++i)
			xyz_p[i] = 0;
	else
		for(i = 0; i < SENSOR_DATA_SIZE; ++i){
			xyz_p[i] = 0;
			/* merge xyz high/low bytes & 1g = 128 becomes 1g = 1024 */
			mutex_lock(&dmt->data_mutex);
			xyzTmp[i] = ((int16_t)((buffer[2*(i+1)+1] << 8)) | buffer[2*(i+1)] ) << 3;
			mutex_unlock(&dmt->data_mutex);
		}
#ifdef SW_FILTER
	if( dmt->aveflag >= dmt->filter){
		for(i = 0; i < SENSOR_DATA_SIZE; ++i){
			dmt->sum[i] = dmt->sum[i] - dmt->bufferave[i][dmt->pointer] + xyzTmp[i];
		}
		/* transfer to the default layout */
		for(i = 0; i < SENSOR_DATA_SIZE; ++i){
			for(j = 0; j < SENSOR_DATA_SIZE; j++)
				xyz_p[i] += (int)(dmt->sum[j]/dmt->filter * dmt_position_map[pos][i][j]);
		}
	}
	else{
	/* init dmt->sum */
		for(i = 0; i < SENSOR_DATA_SIZE; ++i)
			dmt->sum[i] = xyzTmp[i];
#endif
		/* transfer to the default layout */
		for(i = 0; i < SENSOR_DATA_SIZE; ++i){
			for(j = 0; j < SENSOR_DATA_SIZE; j++){
				xyz_p[i] += (int)(xyzTmp[j] * dmt_position_map[pos][i][j]);
			//GSE_LOG("%04d, %04d,%d \n", xyz_p[i], xyzTmp[j], dmt_position_map[pos][i][j]);
			}
		}
		//GSE_LOG("xyz_p: %04d , %04d , %04d\n", xyz_p[0], xyz_p[1], xyz_p[2]);
#ifdef SW_FILTER
		dmt->aveflag++;
	}
	/* init dmt->sum */
	for(i = 0; i < SENSOR_DATA_SIZE; ++i){
		dmt->sum[i] = 0;
	}
	dmt->pointer++;
	dmt->pointer %= dmt->filter;
	for(i = 0; i < SENSOR_DATA_SIZE; ++i){
		dmt->bufferave[i][dmt->pointer] = xyzTmp[i];
	}
    for(i = 0; i < SENSOR_DATA_SIZE; ++i){
		for(k = 0; k < dmt->filter; ++k){ 
			dmt->sum[i] += dmt->bufferave[i][k];
			}
	}
#endif
	return 0;
}

static void DMT_work_func(struct work_struct *delaywork){
	struct dmt_data *dmt = container_of(delaywork, struct dmt_data, delaywork.work);
	int i;
	//static bool firsttime=true;
	raw_data xyz;
  	unsigned long dmt_delay = msecs_to_jiffies(atomic_read(&dmt->delay));
	
	
  	D10_i2c_read_xyz(dmt->client, (int *)&xyz.v);
  	/* dmt->last = RawData - Offset */
  	for(i = 0; i < SENSOR_DATA_SIZE; ++i)
     		dmt->last.v[i] = xyz.v[i] - dmt->offset.v[i];
	//GSE_LOG("@DMTRaw    @ X/Y/Z axis: %04d , %04d , %04d\n", xyz.v[0], xyz.v[1], xyz.v[2]);
	//GSE_LOG("@Offset    @ X/Y/Z axis: %04d , %04d , %04d\n", dmt->offset.u.x, dmt->offset.u.y, dmt->offset.u.z);
	//GSE_LOG("@Raw-Offset@ X/Y/Z axis: %04d , %04d , %04d ,dmt_delay=%d\n", dmt->last.u.x, dmt->last.u.y, dmt->last.u.z, atomic_read(&dmt->delay));
	

	input_report_abs(dmt->input, ABS_X, dmt->last.v[l_sensorconfig.xyz_axis[0][0]]*l_sensorconfig.xyz_axis[0][1]);//dmt->last.v[0]);
	input_report_abs(dmt->input, ABS_Y, dmt->last.v[l_sensorconfig.xyz_axis[1][0]]*l_sensorconfig.xyz_axis[1][1]);//dmt->last.v[1]);
	input_report_abs(dmt->input, ABS_Z, dmt->last.v[l_sensorconfig.xyz_axis[2][0]]*l_sensorconfig.xyz_axis[2][1]);//dmt->last.v[2]);
	input_sync(dmt->input);
		
	if(dmt_delay < 1)
		dmt_delay = 1;
	schedule_delayed_work(&dmt->delaywork, dmt_delay);
}

static int mma10_open(struct inode *node, struct file *fle)
{
    GSE_LOG("open...\n");
	return 0;
}

static int mma10_close(struct inode *node, struct file *fle)
{
    GSE_LOG("close...\n");
	return 0;
}

static long mma10_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int err = 0;
	//unsigned char data[6];
	void __user *argp = (void __user *)arg;
	short delay, enable;
	unsigned int uval = 0;
	
	
	/* cmd mapping */
	switch(cmd)
	{
		case ECS_IOCTL_APP_SET_DELAY:
			// set the rate of g-sensor
			if (copy_from_user(&delay, argp, sizeof(short)))
			{
				printk(KERN_ALERT "Can't get set delay!!!\n");
				return -EFAULT;
			}
			klog("Get delay=%d\n", delay);
			
			if ((delay >=0) && (delay < 20))
			{
				delay = 20;
			} else if (delay > 200) 
			{
				delay = 200;
			}
			l_sensorconfig.sensor_samp = 1000/delay;			
			atomic_set(&s_dmt->delay, 1000/delay);
			break;
		case ECS_IOCTL_APP_SET_AFLAG:
			// enable/disable sensor
			if (copy_from_user(&enable, argp, sizeof(short)))
			{
				printk(KERN_ERR "Can't get enable flag!!!\n");
				return -EFAULT;
			}
			klog("enable=%d\n",enable);
			if ((enable >=0) && (enable <=1))
			{
				//KMSGINF("driver: disable/enable(%d) gsensor.\n", enable);
				
				l_sensorconfig.sensor_enable = enable;
				atomic_set(&s_dmt->enable,enable);
				DMT_sysfs_update_active_status(s_dmt , enable);
				
			} else {
				printk(KERN_ERR "Wrong enable argument in %s !!!\n", __FUNCTION__);
				return -EINVAL;
			}
			break;
		case WMT_IOCTL_SENSOR_GET_DRVID:
			uval = DMARD10_DRVID;
			if (copy_to_user((unsigned int*)arg, &uval, sizeof(unsigned int)))
			{
				return -EFAULT;
			}
			GSE_LOG("dmard10_driver_id:%d\n",uval);
			break;
		case WMT_IOCTL_SENOR_GET_RESOLUTION:		
			uval = (10<<8) | 1; 
			if (copy_to_user((unsigned int *)arg, &uval, sizeof(unsigned int)))
			{
				return -EFAULT;
			}
			printk("<<<<<<<resolution:0x%x\n",uval);
		default:
			err = -1;
			break;
	}

	return err;
}

static const struct file_operations d10_fops = {
	.owner = THIS_MODULE,
	.open = mma10_open,
	.release = mma10_close,
	.unlocked_ioctl = mma10_ioctl,
};


static struct miscdevice d10_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = GSENSOR_DEV_NODE,
	.fops = &d10_fops,
};

static int sensor_writeproc( struct file   *file,
                           const char    *buffer,
                           unsigned long count,
                           void          *data )
{

	//int inputval = -1;
	int enable, sample = -1;
	char tembuf[8];
	//unsigned int amsr = 0;
	int test = 0;

	//mutex_lock(&sense_data_mutex);
	memset(tembuf, 0, sizeof(tembuf));
	// get sensor level and set sensor level
	if (sscanf(buffer, "isdbg=%d\n", &l_sensorconfig.isdbg))
	{
		// only set the dbg flag
	} else if (sscanf(buffer, "samp=%d\n", &sample))
	{
		if (sample > 0)
		{
			if (sample != l_sensorconfig.sensor_samp)
			{
				// should do sth
			}			
			//printk(KERN_ALERT "sensor samp=%d(amsr:%d) has been set.\n", sample, amsr);
		} else {
			klog("Wrong sample argumnet of sensor.\n");
		}
	} else if (sscanf(buffer, "enable=%d\n", &enable))
	{
		if ((enable < 0) || (enable > 1))
		{
			klog("The argument to enable/disable g-sensor should be 0 or 1  !!!\n");
		} else if (enable != l_sensorconfig.sensor_enable)
		{
			//mma_enable_disable(enable);
			l_sensorconfig.sensor_enable = enable;
		}
	} else 	if (sscanf(buffer, "sensor_test=%d\n", &test))
	{ // for test begin
		l_sensorconfig.test_pass = 0;
		atomic_set(&s_dmt->enable,1);
		DMT_sysfs_update_active_status(s_dmt , 1);
	} else if (sscanf(buffer, "sensor_testend=%d\n", &test))
	{	// Don nothing only to be compatible the before testing program	
		atomic_set(&s_dmt->enable,0);
		DMT_sysfs_update_active_status(s_dmt , 0);
	}
	//mutex_unlock(&sense_data_mutex);
	return count;
}

static int sensor_readproc(char *page, char **start, off_t off,
			  int count, int *eof, void *data)
{
	int len = 0;

	len = sprintf(page, 
			"test_pass=%d\nisdbg=%d\nrate=%d\nenable=%d\n",
				l_sensorconfig.test_pass,
				l_sensorconfig.isdbg,
				l_sensorconfig.sensor_samp,
				l_sensorconfig.sensor_enable
				);
	return len;
}


//static int __devinit device_i2c_probe(struct i2c_client *client,const struct i2c_device_id *id){
static int __devinit dmard10_probe(struct platform_device *pdev)
{
	int i, k, ret = 0;
	//struct dmt_data *s_dmt = i2c_get_clientdata(client);
	//struct dmt_data *s_dmt;
	GSE_FUN();
/*	
	if(!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)){
		GSE_ERR("check_functionality failed.\n");
		ret = -ENODEV;
		goto exit0;
  	}
  	
  	// Allocate memory for driver data 
	s_dmt = kzalloc(sizeof(struct dmt_data), GFP_KERNEL);
	memset(s_dmt, 0, sizeof(struct dmt_data));
	if (s_dmt == NULL) {
		GSE_ERR("alloc data failed.\n");
		ret = -ENOMEM;
		goto exit1;
	}
*/	
	/*for(i = 0; i < SENSOR_DATA_SIZE; ++i)
		s_dmt->offset.v[i] = 0;*/
#ifdef SW_FILTER
	s_dmt->pointer = 0;
	s_dmt->aveflag = 0;
	for(i = 0; i < SENSOR_DATA_SIZE; ++i){
		s_dmt->sum[i] = 0;
		for(k = 0; k < SENSOR_DATA_AVG; ++k){ 
			s_dmt->bufferave[i][k] = 0;
			}
	}
	s_dmt->filter = SENSOR_DATA_AVG;
	GSE_LOG("D10_DEFAULT_FILTER: %d\n", s_dmt->filter);
#endif
	/* I2C initialization */
	//s_dmt->client = client;
	
	/* set client data */
	i2c_set_clientdata(s_dmt->client, s_dmt);
	/*ret = dmard10_init(client);
	if (ret < 0)
		goto exit2;
	*/	
	/* input */
	ret = D10_input_init(s_dmt->client);
	if (ret){
		GSE_ERR("D10_input_init fail, error code= %d\n",ret);
		goto exit3;
	}
	
	/* initialize variables in dmt_data */
	mutex_init(&s_dmt->data_mutex);
	mutex_init(&s_dmt->enable_mutex);
#ifdef DMT_DEBUG_DATA
	mutex_init(&s_dmt->suspend_mutex);
#endif
	init_waitqueue_head(&s_dmt->open_wq);
	atomic_set(&s_dmt->active, 0);
	atomic_set(&s_dmt->enable, 0);
	atomic_set(&s_dmt->delay, 0);
	atomic_set(&s_dmt->addr, 0);
	/* DMT Acceleration Sensor Mounting Position on Board */
	s_dmt->position = D10_DEFAULT_POSITION;
	GSE_LOG("D10_DEFAULT_POSITION: %d\n", s_dmt->position);
	//s_dmt->position = (CONFIG_INPUT_DMT_ACCELEROMETER_POSITION);
	//GSE_LOG("CONFIG_INPUT_DMT_ACCELEROMETER_POSITION: %d\n", s_dmt->position);
	/* Misc device */
	if (misc_register(&dmt_device) < 0){
		GSE_ERR("dmt_dev register failed");
		goto exit4;
	}

	/* Setup sysfs */
    if (create_sysfs_interfaces(s_dmt) < 0){
        GSE_ERR("create sysfs failed.");
        goto exit5;
    }
#ifdef CONFIG_HAS_EARLYSUSPEND
	s_dmt->early_suspend.suspend = device_i2c_suspend;
	s_dmt->early_suspend.resume = device_i2c_resume;
	register_early_suspend(&s_dmt->early_suspend);
#endif
	/* Setup driver interface */
	INIT_DELAYED_WORK(&s_dmt->delaywork, DMT_work_func);
	GSE_LOG("DMT: INIT_DELAYED_WORK\n");

	//register ctrl dev
	ret = misc_register(&d10_device);
	if (ret !=0) {
		errlog("Can't register d10_device!\n");
		return -1;
	}
	// register rd/wr proc
	l_sensorconfig.sensor_proc = create_proc_entry(GSENSOR_PROC_NAME, 0666, NULL/*&proc_root*/);
	if (l_sensorconfig.sensor_proc != NULL)
	{
		l_sensorconfig.sensor_proc->write_proc = sensor_writeproc;
		l_sensorconfig.sensor_proc->read_proc = sensor_readproc;
	}

	//create offset file after factory reset
	D10_read_offset_from_file(s_dmt->client);
	
	return 0;

exit5:
	misc_deregister(&dmt_device);
exit4:
	input_unregister_device(s_dmt->input);
exit3:
	kfree(s_dmt);
/*exit2:
exit1:	
exit0:*/
	return ret;
}
/*
static struct i2c_board_info dmard10_board_info={
    .type = SENSOR_I2C_NAME, 
    .addr = SENSOR_I2C_ADDR,
};
*/
//static struct i2c_client *client;

extern int wmt_getsyspara(char *varname, unsigned char *varval, int *varlen);
static int get_axisset(void)
{
	char varbuf[64];
	int n;
	int varlen;

	memset(varbuf, 0, sizeof(varbuf));
	varlen = sizeof(varbuf);
	if (wmt_getsyspara("wmt.io.dm10sensor", varbuf, &varlen)) {
		errlog("Can't get gsensor config in u-boot!!!!\n");
		return -1; //open it for no env just,not insmod such module 2014-6-30
	} else {
		n = sscanf(varbuf, "%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d",
				&l_sensorconfig.op,
				&l_sensorconfig.int_gpio,
				&l_sensorconfig.samp,
				&(l_sensorconfig.xyz_axis[0][0]),
				&(l_sensorconfig.xyz_axis[0][1]),
				&(l_sensorconfig.xyz_axis[1][0]),
				&(l_sensorconfig.xyz_axis[1][1]),
				&(l_sensorconfig.xyz_axis[2][0]),
				&(l_sensorconfig.xyz_axis[2][1]),
				&(l_sensorconfig.offset[0]),
				&(l_sensorconfig.offset[1]),
				&(l_sensorconfig.offset[2])
			);
		if (n != 12) {
			errlog("gsensor format is error in u-boot!!!\n");
			return -1;
		}
		l_sensorconfig.sensor_samp = l_sensorconfig.samp;

		dbg("get the sensor config: %d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d\n",
			l_sensorconfig.op,
			l_sensorconfig.int_gpio,
			l_sensorconfig.samp,
			l_sensorconfig.xyz_axis[0][0],
			l_sensorconfig.xyz_axis[0][1],
			l_sensorconfig.xyz_axis[1][0],
			l_sensorconfig.xyz_axis[1][1],
			l_sensorconfig.xyz_axis[2][0],
			l_sensorconfig.xyz_axis[2][1],
			l_sensorconfig.offset[0],
			l_sensorconfig.offset[1],
			l_sensorconfig.offset[2]
		);
	}
	return 0;
}

static void dmard10_platform_release(struct device *device)
{
	GSE_LOG("...\n");
    return;
}

static int __devexit dmard10_remove(struct platform_device *pdev)
{
	if (l_sensorconfig.sensor_proc != NULL)
	{
		remove_proc_entry(GSENSOR_PROC_NAME, NULL);
		l_sensorconfig.sensor_proc = NULL;
	}
	//misc_deregister(&d10_device);
	return 0;
}


static struct platform_device dmard10_device = {
    .name           = SENSOR_I2C_NAME,
    .id             = 0,
    .dev            = {
    	.release = dmard10_platform_release,
    },
};

static struct platform_driver dmard10_driver = {
	.probe = dmard10_probe,
	.remove = dmard10_remove,
	.shutdown = dmard10_shutdown,
	.suspend	= dmard10_suspend,
	.resume		= dmard10_resume,
	.driver = {
		   .name = SENSOR_I2C_NAME,
		   },
};


static int __init device_init(void){
	//struct device *device;
	struct i2c_client *this_client;
	int ret = 0;

	// parse g-sensor u-boot arg
	ret = get_axisset();
	if (ret < 0)
	{
		printk("<<<<<%s user choose to no sensor chip!\n", __func__);
		return ret;
	}
    GSE_LOG("D10 gsensor driver: initialize.\n");
	
	if (!(this_client = sensor_i2c_register_device(0, SENSOR_I2C_ADDR, SENSOR_I2C_NAME)))
	{
		printk(KERN_ERR"Can't register gsensor i2c device!\n");
		return -1;
	}
	
	if (dmard10_init(this_client))
	{
		GSE_ERR("Failed to init dmard10!\n");
		sensor_i2c_unregister_device(this_client);
		return -1;
	}

	/* Allocate memory for driver data */
	s_dmt = kzalloc(sizeof(struct dmt_data), GFP_KERNEL);
	//memset(s_dmt, 0, sizeof(struct dmt_data));
	if (s_dmt == NULL) {
		GSE_ERR("alloc data failed.\n");
		return -ENOMEM;
	}

	s_dmt->client = this_client;
	s_dmt->pdevice = &dmard10_device;
	s_dmt->offset.u.x = l_sensorconfig.offset[0];
	s_dmt->offset.u.y = l_sensorconfig.offset[1];
	s_dmt->offset.u.z = l_sensorconfig.offset[2];

	
	// create the platform device
	l_dev_class = class_create(THIS_MODULE, SENSOR_I2C_NAME);
	if (IS_ERR(l_dev_class)){
		ret = PTR_ERR(l_dev_class);
		printk(KERN_ERR "Can't class_create gsensor device !!\n");
		return ret;
	}
    if((ret = platform_device_register(&dmard10_device)))
    {
    	GSE_ERR("Can't register dmard10 platform devcie!!!\n");
    	return ret;
    }
    if ((ret = platform_driver_register(&dmard10_driver)) != 0)
    {
    	GSE_ERR("Can't register dmard10 platform driver!!!\n");
    	return ret;
    }

	return 0;
}

static void __exit device_exit(void){
	//i2c_unregister_device(client);
	//i2c_del_driver(&device_i2c_driver);
	GSE_LOG("D10 gsensor driver: release.\n");

	flush_delayed_work_sync(&s_dmt->delaywork);
	cancel_delayed_work_sync(&s_dmt->delaywork);
	
	input_unregister_device(s_dmt->input);
	input_free_device(s_dmt->input);
	misc_deregister(&dmt_device);
	misc_deregister(&d10_device);
	platform_driver_unregister(&dmard10_driver);
	platform_device_unregister(&dmard10_device);
	sensor_i2c_unregister_device(s_dmt->client);
	class_destroy(l_dev_class);
	
	remove_sysfs_interfaces(s_dmt);
	kfree(s_dmt);
}

static int dmt_get_filter(struct i2c_client *client){
	struct dmt_data *dmt = i2c_get_clientdata(client);
	return dmt->filter;
}

static int dmt_set_filter(struct i2c_client *client, int filter){
	struct dmt_data *dmt = i2c_get_clientdata(client);
	if (!((filter >= 1) && (filter <= 32)))
		return -1;
	dmt->filter = filter;
	return 0;
}

static int dmt_get_position(struct i2c_client *client){
	struct dmt_data *dmt = i2c_get_clientdata(client);
	return dmt->position;
}

static int dmt_set_position(struct i2c_client *client, int position){
	struct dmt_data *dmt = i2c_get_clientdata(client);
	if (!((position >= 0) && (position <= 7)))
		return -1;
	dmt->position = position;
	return 0;
}

extern int wmt_setsyspara(char *varname, char *varval);
static void update_var(void)
{
	char varbuf[64];
	int varlen;

	memset(varbuf, 0, sizeof(varbuf));
	varlen = sizeof(varbuf);

	sprintf(varbuf, "%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d",
				l_sensorconfig.op,
				l_sensorconfig.int_gpio,
				l_sensorconfig.samp,
				l_sensorconfig.xyz_axis[0][0],
				l_sensorconfig.xyz_axis[0][1],
				l_sensorconfig.xyz_axis[1][0],
				l_sensorconfig.xyz_axis[1][1],
				l_sensorconfig.xyz_axis[2][0],
				l_sensorconfig.xyz_axis[2][1],
				s_dmt->offset.u.x,
				s_dmt->offset.u.y,
				s_dmt->offset.u.z
			);

	wmt_setsyspara("wmt.io.dm10sensor",varbuf);
}

static int D10_write_offset_to_file(struct i2c_client *client){
	struct dmt_data *dmt = i2c_get_clientdata(client);
	char r_buf[18] = {0};
	char w_buf[18] = {0};	
	//unsigned int orgfs;
	struct file *fp;
	mm_segment_t fs;	
	ssize_t ret;
	//int8_t i;
	
	sprintf(w_buf,"%5d %5d %5d", dmt->offset.u.x, dmt->offset.u.y, dmt->offset.u.z);
	/* Set segment descriptor associated to kernel space */
	fp = filp_open(D10_OffsetFileName, O_RDWR | O_CREAT, 0777);
	if(IS_ERR(fp)){
		GSE_ERR("filp_open %s error!!.\n",D10_OffsetFileName);
		return -1;
	}
	else{
		fs = get_fs();
		//set_fs(KERNEL_DS);
		set_fs(get_ds());
		GSE_LOG("filp_open %s SUCCESS!!.\n",D10_OffsetFileName);
		//fp->f_op->write(fp,data,18, &fp->f_pos);
 		//filp_close(fp,NULL);
 		ret = fp->f_op->write(fp,w_buf,18,&fp->f_pos);
		if(ret != 18)
		{
			printk(KERN_ERR "%s: write error!\n", __func__);
			filp_close(fp,NULL);
			return -EIO;
		}
		//fp->f_pos=0x00;
		ret = fp->f_op->read(fp,r_buf, 18,&fp->f_pos);
		if(ret < 0)
		{
			printk(KERN_ERR "%s: read error!\n", __func__);
			filp_close(fp,NULL);
			return -EIO;
		}		
		set_fs(fs);

		//
		//printk(KERN_INFO "%s: read ret=%d!", __func__, ret);
	/*	for(i=0; i<18 ;i++)
		{
			if(r_buf[i] != w_buf[i])
			{
				printk(KERN_ERR "%s: read back error, r_buf[%x](0x%x) != w_buf[%x](0x%x)\n", 
					__func__, i, r_buf[i], i, w_buf[i]);				
				filp_close(fp,NULL);
				return -EIO;
			}
		}
	*/
		
	}
	filp_close(fp,NULL);
	return 0;	
}

void D10_read_offset_from_file(struct i2c_client *client){
	struct dmt_data *dmt = i2c_get_clientdata(client);
	unsigned int orgfs;
	char data[18];
	struct file *fp;
	int ux,uy,uz;
	orgfs = get_fs();
	/* Set segment descriptor associated to kernel space */
	set_fs(KERNEL_DS);

	fp = filp_open(D10_OffsetFileName, O_RDWR , 0);
	GSE_FUN();
	if(IS_ERR(fp)){
		GSE_ERR("Sorry,file open ERROR !\n");
		if(l_sensorconfig.op){			//first time
			l_sensorconfig.op=0;
#if AUTO_CALIBRATION
			/* get acceleration average reading */
			D10_calibrate(client);
			update_var();
			D10_write_offset_to_file(client);
#endif	
#ifdef DMT_BROADCAST_APK_ENABLE
			create_devidfile();
			return;
#endif	 	
		}
		D10_write_offset_to_file(client);
	}
	else{
		GSE_LOG("filp_open %s SUCCESS!!.\n",D10_OffsetFileName);
		fp->f_op->read(fp,data,18, &fp->f_pos);
		GSE_LOG("filp_read result %s\n",data);
		sscanf(data,"%d %d %d",&ux,&uy,&uz);
		dmt->offset.u.x=ux;
		dmt->offset.u.y=uy;
		dmt->offset.u.z=uz;
	}
	set_fs(orgfs);
}
static int create_devidfile(void)
{
	char data[18];
	unsigned int orgfs;
	struct file *fp;

	sprintf(data,"%5d %5d %5d",0,0,0);
	orgfs = get_fs();
	/* Set segment descriptor associated to kernel space */
	set_fs(KERNEL_DS);
	GSE_FUN();
	fp = filp_open(DmtXXFileName, O_RDWR | O_CREAT, 0777);
	if(IS_ERR(fp)){		
		GSE_ERR("Sorry,file open ERROR !\n");
		return -1;
	}
	fp->f_op->write(fp,data,18, &fp->f_pos);
	set_fs(orgfs);
	filp_close(fp,NULL);
	return 0;
}
//*********************************************************************************************************
MODULE_AUTHOR("DMT_RD");
MODULE_DESCRIPTION("DMT Gsensor Driver");
MODULE_LICENSE("GPL");

module_init(device_init);
module_exit(device_exit);

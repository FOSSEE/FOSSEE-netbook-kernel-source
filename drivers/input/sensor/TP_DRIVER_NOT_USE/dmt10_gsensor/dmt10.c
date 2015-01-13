/*
 * @file drivers/misc/dmt10.c
 * @brief DMT g-sensor Linux device driver
 * @author Domintech Technology Co., Ltd (http://www.domintech.com.tw)
 * @version 1.03
 *
 * @section LICENSE
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
 *  V1.00	D10 First Release											date 2012/09/21
 *  V1.01	static struct dmt_data s_dmt Refresh to device_i2c_probe	date 2012/11/23
 *  V1.02	0x0D cck : adjustment 204.8KHz core clock					date 2012/11/30
 *  V1.03	write TCGYZ & TCGX : set value to 0x00						date 2012/12/10 
 *
 * @DMT Package version D10_General_driver v1.4
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
#include "../gsensor.h"

static struct gsensor_conf gs_conf;

static unsigned int interval;
void gsensor_write_offset_to_file(void);
void gsensor_read_offset_from_file(void);
char OffsetFileName[] = "/data/misc/dmt/offset.txt";	/* FILE offset.txt */
static raw_data offset;
static struct dmt_data *s_dmt;
static int device_init(void);
static void device_exit(void);

static int device_open(struct inode*, struct file*);
static long device_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
static int device_close(struct inode*, struct file*);

static int device_i2c_suspend(struct i2c_client *client, pm_message_t mesg);
static int device_i2c_resume(struct i2c_client *client);
static int __devinit device_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int __devexit device_i2c_remove(struct i2c_client *client);
void device_i2c_read_xyz(struct i2c_client *client, s16 *xyz);
static int device_i2c_rxdata(struct i2c_client *client, unsigned char *rxDat, int length);
static int device_i2c_txdata(struct i2c_client *client, unsigned char *txData, int length);

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
		dmt_delay = msecs_to_jiffies(atomic_read(&dmt->delay));
		if(dmt_delay < 1)
			dmt_delay = 1;

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
	GSE_LOG("buf=%x %x\n", buf[0], buf[1]);
	if (0 == count)
		return 0;

	if (false == get_value_as_int(buf, count, &en))
		return -EINVAL;

	en = en ? 1 : 0;

	atomic_set(&dmt->enable,en);
	DMT_sysfs_update_active_status(dmt , en);
	return count;
}

/***** Acceleration ***/
static ssize_t DMT_enable_acc_show(struct device *dev, struct device_attribute *attr, char *buf){
	return dmt_sysfs_enable_show( dev_get_drvdata(dev), buf, ACC_DATA_FLAG);
}

static ssize_t DMT_enable_acc_store( struct device *dev, struct device_attribute *attr, char const *buf, size_t count){
	return dmt_sysfs_enable_store( dev_get_drvdata(dev), buf, count, ACC_DATA_FLAG);
}

/***** sysfs delay **************************************************/
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

/***** Accelerometer ***/
static ssize_t DMT_delay_acc_show( struct device *dev, struct device_attribute *attr, char *buf){
	return dmt_sysfs_delay_show( dev_get_drvdata(dev), buf, ACC_DATA_FLAG);
}

static ssize_t DMT_delay_acc_store( struct device *dev, struct device_attribute *attr,char const *buf, size_t count){
	return dmt_sysfs_delay_store( dev_get_drvdata(dev), buf, count, ACC_DATA_FLAG);
}

static struct device_attribute DMT_attributes[] = {
	__ATTR(enable_acc, 0755, DMT_enable_acc_show, DMT_enable_acc_store),
	__ATTR(delay_acc,  0755, DMT_delay_acc_show,  DMT_delay_acc_store),
	__ATTR_NULL,
};

static char const *const ACCELEMETER_CLASS_NAME = "accelemeter";
static char const *const GSENSOR_DEVICE_NAME = "dmard10";
static char const *const device_link_name = "i2c";
static dev_t const dmt_device_dev_t = MKDEV(MISC_MAJOR, 240);

/***** dmt sysfs functions ******************************************/
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

static void remove_sysfs_interfaces(struct dmt_data *dmt)
{
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

int input_init(struct i2c_client *client){
	struct dmt_data *dmt = i2c_get_clientdata(client);
	int err=0;
	dmt->input=input_allocate_device();
	if (!dmt->input){
		GSE_ERR("input device allocate ERROR !!\n");
		return -ENOMEM;
	}
	else
		GSE_LOG("input device allocate Success !!\n");
	/* Setup input device */
	set_bit(EV_ABS, dmt->input->evbit);
	/* Accelerometer [-78.5, 78.5]m/s2 in Q16 */
	input_set_abs_params(dmt->input, ABS_X, -1024, 1024, 0, 0);
	input_set_abs_params(dmt->input, ABS_Y, -1024, 1024, 0, 0);
	input_set_abs_params(dmt->input, ABS_Z, -1024, 1024, 0, 0);
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

int gsensor_calibrate(void)
{	
	//struct dmt_data *dmt = i2c_get_clientdata(client);
	raw_data avg;
	int i, j;
	long xyz_acc[SENSOR_DATA_SIZE];   
  	s16 xyz[SENSOR_DATA_SIZE];
	
	offset.u.x=0;
	offset.u.y=0;
	offset.u.z=0;
	/* initialize the accumulation buffer */
  	for(i = 0; i < SENSOR_DATA_SIZE; ++i) 
		xyz_acc[i] = 0;

	for(i = 0; i < AVG_NUM; i++) {      
		device_i2c_read_xyz(s_dmt->client, (s16 *)&xyz);
		for(j = 0; j < SENSOR_DATA_SIZE; ++j) 
			xyz_acc[j] += xyz[j];
  	}
	/* calculate averages */
  	for(i = 0; i < SENSOR_DATA_SIZE; ++i) 
		avg.v[i] = (s16) (xyz_acc[i] / AVG_NUM);
		
	if(avg.v[2] < 0){
		offset.u.x =  avg.v[0] ;    
		offset.u.y =  avg.v[1] ;
		offset.u.z =  avg.v[2] + DEFAULT_SENSITIVITY;
		return CONFIG_GSEN_CALIBRATION_GRAVITY_ON_Z_POSITIVE;
	}
	else{	
		offset.u.x =  avg.v[0] ;    
		offset.u.y =  avg.v[1] ;
		offset.u.z =  avg.v[2] - DEFAULT_SENSITIVITY;
		return CONFIG_GSEN_CALIBRATION_GRAVITY_ON_Z_NEGATIVE;
	}
	return 0;
}

int gsensor_reset(struct i2c_client *client){
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
	buffer[4] = VALUE_TAPNS_Ave_2;
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

void gsensor_set_offset(int val[3]){
	int i;
	for(i = 0; i < SENSOR_DATA_SIZE; ++i)
		offset.v[i] = (s16) val[i];
}

struct file_operations dmt_g_sensor_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = device_ioctl,
	.open = device_open,
	.release = device_close,
};

static struct miscdevice dmt_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = DEVICE_I2C_NAME,
	.fops = &dmt_g_sensor_fops,
};

static int sensor_close_dev(struct i2c_client *client){    	
	char buffer[3];
	buffer[0] = REG_ACTR;
	buffer[1] = MODE_Standby;
	buffer[2] = MODE_Off;
	GSE_FUN();	
	return device_i2c_txdata(client,buffer, 3);
}

static int device_i2c_suspend(struct i2c_client *client, pm_message_t mesg){
	GSE_FUN();
	return sensor_close_dev(client);
}

static int device_i2c_resume(struct i2c_client *client){
	GSE_FUN();
	return gsensor_reset(client);
}

static void device_i2c_shutdown(struct i2c_client *client)
{
	struct dmt_data *dmt = i2c_get_clientdata(client);
	flush_delayed_work_sync(&dmt->delaywork);
	cancel_delayed_work_sync(&dmt->delaywork);
}

static int __devexit device_i2c_remove(struct i2c_client *client){
	return 0;
}

static const struct i2c_device_id device_i2c_ids[] = {
	{DEVICE_I2C_NAME, 0},
	{}   
};

//MODULE_DEVICE_TABLE(i2c, device_i2c_ids);

static struct i2c_driver device_i2c_driver = 
{
	.driver	= {
		.owner = THIS_MODULE,
		.name = DEVICE_I2C_NAME,
		},
	.class = I2C_CLASS_HWMON,
	.id_table = device_i2c_ids,
	.probe = device_i2c_probe,
	.remove	= __devexit_p(device_i2c_remove),
	.shutdown = device_i2c_shutdown,
#ifndef CONFIG_ANDROID_POWER 
	.suspend = device_i2c_suspend,
	.resume	= device_i2c_resume,
#endif	
	
};

static int device_open(struct inode *inode, struct file *filp)
{
	return 0; 
}

static long device_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	//struct i2c_client *client = (struct i2c_client*)filp->private_data;
	//struct dmt_data *dmt = (struct dmt_data*)i2c_get_clientdata(client);	
	
	int err = 0, ret = 0, i;
	int intBuf[SENSOR_DATA_SIZE];
	s16 xyz[SENSOR_DATA_SIZE];
	/* check type */
	if (_IOC_TYPE(cmd) != IOCTL_MAGIC) return -ENOTTY;

	/* check user space pointer is valid */
	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		err =  !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
	if (err) return -EFAULT;
	
	switch(cmd) 
	{
		case SENSOR_RESET:
			gsensor_reset(s_dmt->client);
			return ret;

		case SENSOR_CALIBRATION:
			/* get orientation info */
			//if(copy_from_user(&intBuf, (int*)arg, sizeof(intBuf))) return -EFAULT;
			gsensor_calibrate();
			GSE_LOG("Sensor_calibration:%d %d %d\n",offset.u.x,offset.u.y,offset.u.z);
			/* save file */
			gsensor_write_offset_to_file();
			
			/* return the offset */
			for(i = 0; i < SENSOR_DATA_SIZE; ++i)
				intBuf[i] = offset.v[i];

			ret = copy_to_user((int *)arg, &intBuf, sizeof(intBuf));
			return ret;
		
		case SENSOR_GET_OFFSET:
			/* get data from file */
			gsensor_read_offset_from_file();
			for(i = 0; i < SENSOR_DATA_SIZE; ++i)
				intBuf[i] = offset.v[i];

			ret = copy_to_user((int *)arg, &intBuf, sizeof(intBuf));
			return ret;

		case SENSOR_SET_OFFSET:
			ret = copy_from_user(&intBuf, (int *)arg, sizeof(intBuf));
			gsensor_set_offset(intBuf);
			/* write in to file */
			gsensor_write_offset_to_file();
			return ret;
		
		case SENSOR_READ_ACCEL_XYZ:
			device_i2c_read_xyz(s_dmt->client, (s16 *)&xyz);
			for(i = 0; i < SENSOR_DATA_SIZE; ++i)
				intBuf[i] = xyz[i] - offset.v[i];
			
		  	ret = copy_to_user((int*)arg, &intBuf, sizeof(intBuf));
			return ret;
			
		case SENSOR_SETYPR:
			if(copy_from_user(&intBuf, (int*)arg, sizeof(intBuf))) {
				GSE_LOG("%s:copy_from_user(&intBuf, (int*)arg, sizeof(intBuf)) ERROR, -EFAULT\n",__func__);			
				return -EFAULT;
			}
			input_report_abs(s_dmt->input, ABS_X, intBuf[0]);
			input_report_abs(s_dmt->input, ABS_Y, -intBuf[1]);
			input_report_abs(s_dmt->input, ABS_Z, -intBuf[2]);
			input_sync(s_dmt->input);
			GSE_LOG(KERN_INFO "%s:SENSOR_SETYPR OK! x=%d,y=%d,z=%d\n",__func__,intBuf[0],intBuf[1],intBuf[2]);
			return 1;
			
		case SENSOR_GET_OPEN_STATUS:
			GSE_LOG(KERN_INFO "%s:Going into DMT_GetOpenStatus()\n",__func__);
			DMT_GetOpenStatus(s_dmt->client);
			GSE_LOG(KERN_INFO "%s:DMT_GetOpenStatus() finished\n",__func__);
			return 1;
			break;
			
		case SENSOR_GET_CLOSE_STATUS:
			GSE_LOG(KERN_INFO "%s:Going into DMT_GetCloseStatus()\n",__func__);
			DMT_GetCloseStatus(s_dmt->client);	
			GSE_LOG(KERN_INFO "%s:DMT_GetCloseStatus() finished\n",__func__);
			return 1;
			break;	
				
		case SENSOR_GET_DELAY:
		  	ret = copy_to_user((int*)arg, &interval, sizeof(interval));
			return 1;
			break;
		
		default:  /* redundant, as cmd was checked against MAXNR */
			return -ENOTTY;
	}
	
	return 0;
}
	
static int device_close(struct inode *inode, struct file *filp)
{
	return 0;
}

/***** I2C I/O function ***********************************************/
static int device_i2c_rxdata( struct i2c_client *client, unsigned char *rxData, int length)
{
	struct i2c_msg msgs[] = 
	{
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

static int device_i2c_txdata( struct i2c_client *client, unsigned char *txData, int length)
{
	struct i2c_msg msg[] = 
	{
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
/* 1g = 128 becomes 1g = 1024 */
static inline void device_i2c_correct_accel_sign(s16 *val){
	*val<<= 3;
}

void device_i2c_merge_register_values(struct i2c_client *client, s16 *val, u8 msb, u8 lsb){
	*val = (((u16)msb) << 8) | (u16)lsb; 
	device_i2c_correct_accel_sign(val);
}

void device_i2c_read_xyz(struct i2c_client *client, s16 *xyz_p){	
	u8 buffer[11];
	s16 xyzTmp[SENSOR_DATA_SIZE];
	int i, j;
	/* get xyz high/low bytes, 0x12 */
	buffer[0] = REG_STADR;
	device_i2c_rxdata(client, buffer, 10);
    
	/* merge to 10-bits value */
	for(i = 0; i < SENSOR_DATA_SIZE; ++i){
		xyz_p[i] = 0;
		device_i2c_merge_register_values(client, (xyzTmp + i), buffer[2*(i+1)+1], buffer[2*(i+1)]);
	/* transfer to the default layout */
		for(j = 0; j < 3; j++)
			xyz_p[i] += sensorlayout[i][j] * xyzTmp[j];
	}
	GSE_LOG("xyz_p: %04d , %04d , %04d\n", xyz_p[0], xyz_p[1], xyz_p[2]);
}

static void DMT_work_func(struct work_struct *delaywork)
{
	struct dmt_data *dmt = container_of(delaywork, struct dmt_data, delaywork.work);
	int i;
	static int firsttime=0;
	s16 xyz[SENSOR_DATA_SIZE];

	unsigned long t=atomic_read(&dmt->delay);
  	unsigned long dmt_delay = msecs_to_jiffies(t);
	if(!firsttime){
		//gsensor_read_offset_from_file();	
	 	firsttime=1;
	}
	
	GSE_LOG("t=%lu , dmt_delay=%lu\n", t, dmt_delay);
  	device_i2c_read_xyz(dmt->client, (s16 *)&xyz);
  	for(i = 0; i < SENSOR_DATA_SIZE; ++i)
     		xyz[i] -= offset.v[i];

	GSE_LOG("@DMTRaw@ X/Y/Z axis: %04d , %04d , %04d\n", xyz[0], xyz[1], xyz[2]);
	GSE_LOG("@Offset@ X/Y/Z axis: %04d , %04d , %04d\n", offset.u.x, offset.u.y, offset.u.z);
	input_report_abs(dmt->input, ABS_X, xyz[gs_conf.xyz_axis[ABS_X][0]]*gs_conf.xyz_axis[ABS_X][1]);
	input_report_abs(dmt->input, ABS_Y, xyz[gs_conf.xyz_axis[ABS_Y][0]]*gs_conf.xyz_axis[ABS_Y][1]);
	input_report_abs(dmt->input, ABS_Z, xyz[gs_conf.xyz_axis[ABS_Z][0]]*gs_conf.xyz_axis[ABS_Z][1]);
	input_sync(dmt->input);

	if(dmt_delay < 1)
		dmt_delay = 1;
	schedule_delayed_work(&dmt->delaywork, dmt_delay);
}

static int __devinit device_i2c_probe(struct i2c_client *client,const struct i2c_device_id *id){
	int i, ret = 0;
	//struct dmt_data *s_dmt;

	GSE_FUN();
	for(i = 0; i < SENSOR_DATA_SIZE; ++i)
		offset.v[i] = 0;
		
	if(!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)){
		GSE_ERR("check_functionality failed.\n");
		ret = -ENODEV;
		goto exit0;
  	}
  	
  	/* Allocate memory for driver data */
	s_dmt = kzalloc(sizeof(struct dmt_data), GFP_KERNEL);
	memset(s_dmt, 0, sizeof(struct dmt_data));
	if (s_dmt == NULL) {
		GSE_ERR("alloc data failed.\n");
		ret = -ENOMEM;
		goto exit1;
	}
	
	/***** I2C initialization *****/
	s_dmt->client = client;
	/* set client data */
	i2c_set_clientdata(client, s_dmt);
	ret = gsensor_reset(client);
	if (ret < 0)
		goto exit2;
		
	/***** input *****/
	ret = input_init(client);
	if (ret){
		GSE_ERR("input_init fail, error code= %d\n",ret);
		goto exit3;
	}
	
	/**** initialize variables in dmt_data *****/
	init_waitqueue_head(&s_dmt->open_wq);
	atomic_set(&s_dmt->active, 0);
	atomic_set(&s_dmt->enable, 0);
	atomic_set(&s_dmt->delay, 0);
	mutex_init(&s_dmt->sensor_mutex);
	/***** misc *****/ 
	/* we have been register miscdevice in device_init, and 
	 * marked by Eason 2013/2/4*/
	/*ret = misc_register(&dmt_device);
	if (ret){
		GSE_ERR("dmt_dev register failed");
		goto exit5;
	}*/

	/***** sysfs *****/
    ret = create_sysfs_interfaces(s_dmt);
    if (ret < 0){
        GSE_ERR("create sysfs failed.");
        goto exit6;
    }

	INIT_DELAYED_WORK(&s_dmt->delaywork, DMT_work_func);
	GSE_LOG("DMT: INIT_DELAYED_WORK\n");
	return 0;

exit6:
	//misc_deregister(&dmt_device);
exit5:
	input_unregister_device(s_dmt->input);
exit3:
	kfree(s_dmt);
exit2:
exit1:	
exit0:
	return ret;
}

int dmt10_enable(int en)
{
	printk(KERN_DEBUG "%s: enable = %d\n", __func__, en);
	DMT_sysfs_update_active_status(s_dmt,en);
	return 0;
}

int dmt10_setDelay(int mdelay)
{
	printk(KERN_DEBUG "%s: delay = %d\n", __func__, mdelay);
	atomic_set(&s_dmt->delay, mdelay);
	return 0;
}

int dmt10_getLSG(int *lsg)
{
    *lsg = 1024;
	return 0;
}

struct gsensor_data dmt10_gs_data = {
	.i2c_addr = DMT10_I2C_ADDR,
	.enable = dmt10_enable,
	.setDelay = dmt10_setDelay,
	.getLSG = dmt10_getLSG,
};

static int __init device_init(void){
	int ret = 0;

	if (get_gsensor_conf(&gs_conf))
		return -1;

	if (gs_conf.op != 3)
		return -1;
	printk("G-Sensor dmt10 init\n");

	if (gsensor_register(&dmt10_gs_data))
		return -1;

	if (gsensor_i2c_register_device() < 0)
		return -1;

	return i2c_add_driver(&device_i2c_driver);
}

static void __exit device_exit(void){
	i2c_del_driver(&device_i2c_driver);
}

void gsensor_write_offset_to_file(void){
	char data[18];
	unsigned int orgfs;
	struct file *fp;

	sprintf(data,"%5d %5d %5d",offset.u.x,offset.u.y,offset.u.z);
	orgfs = get_fs();
	/* Set segment descriptor associated to kernel space */
	set_fs(KERNEL_DS);
	fp = filp_open(OffsetFileName, O_RDWR | O_CREAT, 0777);
	if(IS_ERR(fp)){
		GSE_ERR("filp_open %s error!!.\n",OffsetFileName);
	}
	else{
		GSE_LOG("filp_open %s SUCCESS!!.\n",OffsetFileName);
		fp->f_op->write(fp,data,18, &fp->f_pos);
 		filp_close(fp,NULL);
	}
	set_fs(orgfs);
}

void gsensor_read_offset_from_file(void){
	unsigned int orgfs;
	char data[18];
	struct file *fp;
	int ux,uy,uz;
	orgfs = get_fs();
	/* Set segment descriptor associated to kernel space */
	set_fs(KERNEL_DS);

	fp = filp_open(OffsetFileName, O_RDWR , 0);
	GSE_FUN();
	if(IS_ERR(fp)){
		GSE_ERR("Sorry,file open ERROR !\n");
		offset.u.x=0;offset.u.y=0;offset.u.z=0;
#if AUTO_CALIBRATION
		/* get acceleration average reading */
		gsensor_calibrate();
		gsensor_write_offset_to_file();
#endif
	}
	else{
		GSE_LOG("filp_open %s SUCCESS!!.\n",OffsetFileName);
		fp->f_op->read(fp,data,18, &fp->f_pos);
		GSE_LOG("filp_read result %s\n",data);
		sscanf(data,"%d %d %d",&ux,&uy,&uz);
		offset.u.x=ux;
		offset.u.y=uy;
		offset.u.z=uz;
		filp_close(fp,NULL);
	}
	set_fs(orgfs);
}
//*********************************************************************************************************
MODULE_AUTHOR("DMT_RD");
MODULE_DESCRIPTION("DMT Gsensor Driver");
MODULE_LICENSE("GPL");

module_init(device_init);
module_exit(device_exit);

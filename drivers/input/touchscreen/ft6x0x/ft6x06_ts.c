/* drivers/input/touchscreen/ft5x06_ts.c
 *
 * FocalTech ft6x06 TouchScreen driver.
 *
 * Copyright (c) 2010  Focal tech Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include "ft6x06_ts.h"
//#include <linux/earlysuspend.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <mach/irqs.h>
#include <linux/kernel.h>
#include <linux/semaphore.h>
#include <linux/mutex.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/syscalls.h>
#include <linux/unistd.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/timer.h>

//#define FTS_CTL_FACE_DETECT
#define FTS_CTL_IIC
#define SYSFS_DEBUG
#define FTS_APK_DEBUG
//#define FT6X06_DOWNLOAD

#ifdef FTS_CTL_IIC
#include "focaltech_ctl.h"
#endif
#ifdef FTS_CTL_FACE_DETECT
#include "ft_psensor_drv.h"
#endif
#ifdef SYSFS_DEBUG
#include "ft6x06_ex_fun.h"
#endif

#if 0
struct ts_event {
	u16 au16_x[CFG_MAX_TOUCH_POINTS];	/*x coordinate */
	u16 au16_y[CFG_MAX_TOUCH_POINTS];	/*y coordinate */
	u8 au8_touch_event[CFG_MAX_TOUCH_POINTS];	/*touch event:
					0 -- down; 1-- up; 2 -- contact */
	u8 au8_finger_id[CFG_MAX_TOUCH_POINTS];	/*touch ID */
	u16 pressure;
	u8 touch_point;
};

struct ft6x06_ts_data {
	unsigned int irq;
	unsigned int x_max;
	unsigned int y_max;
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct ts_event event;
	struct ft6x06_platform_data *pdata;
#ifdef CONFIG_PM
	struct early_suspend *early_suspend;
#endif
};

#define FTS_POINT_UP		0x01
#define FTS_POINT_DOWN		0x00
#define FTS_POINT_CONTACT	0x02
#endif

/*
*ft6x06_i2c_Read-read data and write data by i2c
*@client: handle of i2c
*@writebuf: Data that will be written to the slave
*@writelen: How many bytes to write
*@readbuf: Where to store data read from slave
*@readlen: How many bytes to read
*
*Returns negative errno, else the number of messages executed
*
*
*/
int ft6x06_i2c_Read(struct i2c_client *client, char *writebuf,
		    int writelen, char *readbuf, int readlen)
{
	int ret;

	if (writelen > 0) {
		struct i2c_msg msgs[] = {
			{
			 .addr = client->addr,
			 .flags = 0,
			 .len = writelen,
			 .buf = writebuf,
			 },
			{
			 .addr = client->addr,
			 .flags = I2C_M_RD,
			 .len = readlen,
			 .buf = readbuf,
			 },
		};
		ret = i2c_transfer(client->adapter, msgs, 2);
		if (ret < 0)
			dev_err(&client->dev, "f%s: i2c read error.\n",
				__func__);
	} else {
		struct i2c_msg msgs[] = {
			{
			 .addr = client->addr,
			 .flags = I2C_M_RD,
			 .len = readlen,
			 .buf = readbuf,
			 },
		};
		ret = i2c_transfer(client->adapter, msgs, 1);
		if (ret < 0)
			dev_err(&client->dev, "%s:i2c read error.\n", __func__);
	}
	return ret;
}
/*write data by i2c*/
int ft6x06_i2c_Write(struct i2c_client *client, char *writebuf, int writelen)
{
	int ret;

	struct i2c_msg msg[] = {
		{
		 .addr = client->addr,
		 .flags = 0,
		 .len = writelen,
		 .buf = writebuf,
		 },
	};

	ret = i2c_transfer(client->adapter, msg, 1);
	if (ret < 0)
		dev_err(&client->dev, "%s i2c write error.\n", __func__);

	return ret;
}

#if 0
/*Read touch point information when the interrupt  is asserted.*/
static int ft6x06_read_Touchdata(struct ft6x06_ts_data *data)
{
	struct ts_event *event = &data->event;
	u8 buf[POINT_READ_BUF] = { 0 };
	int ret = -1;
	int i = 0;
	u8 pointid = FT_MAX_ID;

	ret = ft6x06_i2c_Read(data->client, buf, 1, buf, POINT_READ_BUF);
	if (ret < 0) {
		dev_err(&data->client->dev, "%s read touchdata failed.\n",
			__func__);
		return ret;
	}
	memset(event, 0, sizeof(struct ts_event));

	//event->touch_point = buf[2] & 0x0F;

	//event->touch_point = 0;
	
	for (i = 0; i < CFG_MAX_TOUCH_POINTS; i++)
	{
		pointid = (buf[FT_TOUCH_ID_POS + FT_TOUCH_STEP * i]) >> 4;
		if (pointid >= FT_MAX_ID)
			break;
		else
			event->touch_point++;
		event->au16_x[i] =
		    (s16) (buf[FT_TOUCH_X_H_POS + FT_TOUCH_STEP * i] & 0x0F) <<
		    8 | (s16) buf[FT_TOUCH_X_L_POS + FT_TOUCH_STEP * i];
		event->au16_y[i] =
		    (s16) (buf[FT_TOUCH_Y_H_POS + FT_TOUCH_STEP * i] & 0x0F) <<
		    8 | (s16) buf[FT_TOUCH_Y_L_POS + FT_TOUCH_STEP * i];
		event->au8_touch_event[i] =
		    buf[FT_TOUCH_EVENT_POS + FT_TOUCH_STEP * i] >> 6;
		event->au8_finger_id[i] =
		    (buf[FT_TOUCH_ID_POS + FT_TOUCH_STEP * i]) >> 4;
	}
	
	//event->pressure = FT_PRESS;

	return 0;
}

/*
*report the point information
*/
static void ft6x06_report_value(struct ft6x06_ts_data *data)
{
	struct ts_event *event = &data->event;
	int i = 0;
	int up_point = 0;

	for (i = 0; i < event->touch_point; i++) 
	{
		input_mt_slot(data->input_dev, event->au8_finger_id[i]);

		if (event->au8_touch_event[i]== 0 || event->au8_touch_event[i] == 2)
			{
				input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER,
					true);
				//input_report_abs(data->input_dev, ABS_MT_TRACKING_ID,
					 //event->au8_finger_id[i]);
				input_report_abs(data->input_dev, ABS_MT_PRESSURE,
					0x3f);
				input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR,
					0x05);
				input_report_abs(data->input_dev, ABS_MT_POSITION_X,
					event->au16_x[i]);
				input_report_abs(data->input_dev, ABS_MT_POSITION_Y,
					event->au16_y[i]);

			}
			else
			{
				up_point++;
				input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER,
					false);
			}				
		
	}

	if(event->touch_point == up_point)
		input_report_key(data->input_dev, BTN_TOUCH, 0);
	else
		input_report_key(data->input_dev, BTN_TOUCH, 1);

	input_sync(data->input_dev);

}

/*The ft6x06 device will signal the host about TRIGGER_FALLING.
*Processed when the interrupt is asserted.
*/
static irqreturn_t ft6x06_ts_interrupt(int irq, void *dev_id)
{
	struct ft6x06_ts_data *ft6x06_ts = dev_id;
	int ret = 0;
	disable_irq_nosync(ft6x06_ts->irq);

	ret = ft6x06_read_Touchdata(ft6x06_ts);
	if (ret == 0)
		ft6x06_report_value(ft6x06_ts);

	enable_irq(ft6x06_ts->irq);

	//printk(KERN_WARNING "interrupt \n");

	return IRQ_HANDLED;
}

static int ft6x06_ts_probe(struct i2c_client *client,
			   const struct i2c_device_id *id)
{
	struct ft6x06_platform_data *pdata =
	    (struct ft6x06_platform_data *)client->dev.platform_data;
	struct ft6x06_ts_data *ft6x06_ts;
	struct input_dev *input_dev;
	int err = 0;
	unsigned char uc_reg_value;
	unsigned char uc_reg_addr;


	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		err = -ENODEV;
		goto exit_check_functionality_failed;
	}

	ft6x06_ts = kzalloc(sizeof(struct ft6x06_ts_data), GFP_KERNEL);

	if (!ft6x06_ts) {
		err = -ENOMEM;
		goto exit_alloc_data_failed;
	}
	
	i2c_set_clientdata(client, ft6x06_ts);
	ft6x06_ts->irq = client->irq;
	ft6x06_ts->client = client;
	ft6x06_ts->pdata = pdata;
	ft6x06_ts->x_max = pdata->x_max - 1;
	ft6x06_ts->y_max = pdata->y_max - 1;
	ft6x06_ts->pdata->irq = ft6x06_ts->irq;
	client->irq = ft6x06_ts->irq;
	pr_info("irq = %d\n", client->irq);
	
#ifdef CONFIG_PM
	#if 0
	err = gpio_request(pdata->reset, "ft6x06 reset");
	if (err < 0) {
		dev_err(&client->dev, "%s:failed to set gpio reset.\n",
			__func__);
		goto exit_request_reset;
	}
	#endif
#endif

	err = request_threaded_irq(client->irq, NULL, ft6x06_ts_interrupt,
				   IRQF_TRIGGER_FALLING, client->dev.driver->name,
				   ft6x06_ts);
	
	if (err < 0) {
		dev_err(&client->dev, "ft6x06_probe: request irq failed\n");
		goto exit_irq_request_failed;
	}
	disable_irq(client->irq);

	input_dev = input_allocate_device();
	if (!input_dev) {
		err = -ENOMEM;
		dev_err(&client->dev, "failed to allocate input device\n");
		goto exit_input_dev_alloc_failed;
	}

	ft6x06_ts->input_dev = input_dev;

	__set_bit(EV_ABS, input_dev->evbit);
	__set_bit(EV_KEY, input_dev->evbit);
	__set_bit(BTN_TOUCH, input_dev->keybit);
	__set_bit(INPUT_PROP_DIRECT, input_dev->propbit);
	
	input_mt_init_slots(input_dev, MT_MAX_TOUCH_POINTS);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR,
			     0, PRESS_MAX, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_X,
			     0, ft6x06_ts->x_max, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y,
			     0, ft6x06_ts->y_max, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_PRESSURE,
					 0, PRESS_MAX, 0, 0);

	input_dev->name = FT6X06_NAME;
	err = input_register_device(input_dev);
	if (err) {
		dev_err(&client->dev,
			"ft6x06_ts_probe: failed to register input device: %s\n",
			dev_name(&client->dev));
		goto exit_input_register_device_failed;
	}
	/*make sure CTP already finish startup process */
	msleep(150);

	/*get some register information */
	uc_reg_addr = FT6x06_REG_FW_VER;
	ft6x06_i2c_Read(client, &uc_reg_addr, 1, &uc_reg_value, 1);
	dev_dbg(&client->dev, "[FTS] Firmware version = 0x%x\n", uc_reg_value);

	uc_reg_addr = FT6x06_REG_POINT_RATE;
	ft6x06_i2c_Read(client, &uc_reg_addr, 1, &uc_reg_value, 1);
	dev_dbg(&client->dev, "[FTS] report rate is %dHz.\n",
		uc_reg_value * 10);

	uc_reg_addr = FT6x06_REG_THGROUP;
	ft6x06_i2c_Read(client, &uc_reg_addr, 1, &uc_reg_value, 1);
	dev_dbg(&client->dev, "[FTS] touch threshold is %d.\n",
		uc_reg_value * 4);

#ifdef SYSFS_DEBUG
	ft6x06_create_sysfs(client);
#endif

#ifdef FTS_CTL_IIC
	if (ft_rw_iic_drv_init(client) < 0)
		dev_err(&client->dev, "%s:[FTS] create fts control iic driver failed\n",
				__func__);
#endif

#ifdef FTS_APK_DEBUG
	ft6x06_create_apk_debug_channel(client);
#endif

#ifdef FTS_CTL_FACE_DETECT
		if (ft_psensor_drv_init(client) < 0)
			dev_err(&client->dev, "%s:[FTS] create fts control psensor driver failed\n",
					__func__);
#endif

	enable_irq(client->irq);
	return 0;

exit_input_register_device_failed:
	input_free_device(input_dev);

exit_input_dev_alloc_failed:
	free_irq(client->irq, ft6x06_ts);
#ifdef CONFIG_PM
exit_request_reset:
	gpio_free(ft6x06_ts->pdata->reset);
#endif

exit_irq_request_failed:
	i2c_set_clientdata(client, NULL);
	kfree(ft6x06_ts);

exit_alloc_data_failed:
exit_check_functionality_failed:
	return err;
}

#ifdef CONFIG_PM
static void ft6x06_ts_suspend(struct early_suspend *handler)
{
	struct ft6x06_ts_data *ts = container_of(handler, struct ft6x06_ts_data,
						early_suspend);

	dev_dbg(&ts->client->dev, "[FTS]ft6x06 suspend\n");
	disable_irq(ts->pdata->irq);
}

static void ft6x06_ts_resume(struct early_suspend *handler)
{
	struct ft6x06_ts_data *ts = container_of(handler, struct ft6x06_ts_data,
						early_suspend);

	dev_dbg(&ts->client->dev, "[FTS]ft6x06 resume.\n");
	gpio_set_value(ts->pdata->reset, 0);
	msleep(20);
	gpio_set_value(ts->pdata->reset, 1);
	enable_irq(ts->pdata->irq);
}
#else
#define ft6x06_ts_suspend	NULL
#define ft6x06_ts_resume		NULL
#endif

static int __devexit ft6x06_ts_remove(struct i2c_client *client)
{
	struct ft6x06_ts_data *ft6x06_ts;
	ft6x06_ts = i2c_get_clientdata(client);
	input_unregister_device(ft6x06_ts->input_dev);
	#ifdef CONFIG_PM
	gpio_free(ft6x06_ts->pdata->reset);
	#endif

	#ifdef SYSFS_DEBUG
	ft6x06_release_sysfs(client);
	#endif
	#ifdef FTS_CTL_IIC
	ft_rw_iic_drv_exit();
	#endif
	#ifdef FTS_CTL_FACE_DETECT
	ft_psensor_drv_exit();
	#endif
	free_irq(client->irq, ft6x06_ts);
	kfree(ft6x06_ts);
	i2c_set_clientdata(client, NULL);
	return 0;
}

static const struct i2c_device_id ft6x06_ts_id[] = {
	{FT6X06_NAME, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, ft6x06_ts_id);

static struct i2c_driver ft6x06_ts_driver = {
	.probe = ft6x06_ts_probe,
	.remove = __devexit_p(ft6x06_ts_remove),
	.id_table = ft6x06_ts_id,
	.suspend = ft6x06_ts_suspend,
	.resume = ft6x06_ts_resume,
	.driver = {
		   .name = FT6X06_NAME,
		   .owner = THIS_MODULE,
		   },
};

static int __init ft6x06_ts_init(void)
{
	int ret;
	ret = i2c_add_driver(&ft6x06_ts_driver);
	if (ret) {
		printk(KERN_WARNING "Adding ft6x06 driver failed "
		       "(errno = %d)\n", ret);
	} else {
		pr_info("Successfully added driver %s\n",
			ft6x06_ts_driver.driver.name);
	}
	return ret;
}

static void __exit ft6x06_ts_exit(void)
{
	i2c_del_driver(&ft6x06_ts_driver);
}

module_init(ft6x06_ts_init);
module_exit(ft6x06_ts_exit);

MODULE_AUTHOR("<luowj>");
MODULE_DESCRIPTION("FocalTech ft6x06 TouchScreen driver");
MODULE_LICENSE("GPL");

#endif

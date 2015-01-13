/****************************************************************
 *
 * touch.c : I2C Touchscreen driver
 *
 * Copyright (c) 2013 SEMISENS Co.,Ltd
 *      http://www.semisens.com
 *
 ****************************************************************/
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/slab.h>
#include <linux/hrtimer.h>
#include <asm/unaligned.h>
#include <linux/firmware.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/i2c.h>

//----------------------------------------------
#if defined(CONFIG_HAS_EARLYSUSPEND)
	#include <linux/wakelock.h>
	#include <linux/earlysuspend.h>
	#include <linux/suspend.h>
#endif

//----------------------------------------------
#include <linux/input/mt.h>
#include "sn310m-touch-pdata.h"
#include "sn310m-touch.h"

//----------------------------------------------
#include "touch.h"
#include <linux/gpio.h>
#include <mach/wmt_iomux.h>


extern int wmt_getsyspara(char *varname, unsigned char *varval, int *varlen);
extern int wmt_setsyspara(char *varname, unsigned char *varval);

#define SN310M_NATIVE_INTERFACE /* This is to debug semisens TSC */ 

#if defined(SN310M_NATIVE_INTERFACE)
#include <linux/miscdevice.h>
#include <linux/syscalls.h>
struct touch* g_ts;
int g_MiscInitialize = 0;
static int P_SN310M_Dist_Probe(struct touch* ts);
static int P_SN310M_Dist_Open(struct inode *inode, struct file *file);
static long P_SN310M_Dist_Ioctl(struct file *file, unsigned int cmd, unsigned long arg);
static void P_SN310M_Dist_Remove(void);
#endif

// function prototype define
//----------------------------------------------
#ifdef	CONFIG_HAS_EARLYSUSPEND
	static void	touch_suspend(struct early_suspend *h);
	static void	touch_resume(struct early_suspend *h);
#endif

irqreturn_t	touch_irq(int irq, void *handle);
#if 0 /* unused */
	static void	touch_work(struct touch *ts);
#endif
static void touch_work_q(struct work_struct *work);
static void	touch_key_report(struct touch *ts, unsigned char button_data);
static void	touch_report_protocol_a(struct touch *ts);
static void	touch_report_protocol_b(struct touch *ts);
static void	touch_event_clear(struct	touch *ts);
#if 0 /* unused */
	static void	touch_enable(struct touch *ts);
	static void	touch_disable(struct touch *ts);
#endif
static void touch_input_close(struct input_dev *input);
static int touch_input_open(struct input_dev *input);
static int touch_check_functionality	(struct touch_pdata *pdata);
void touch_hw_reset(struct touch *ts);
int	touch_info_display(struct touch *ts);
int	touch_probe(struct i2c_client *client, const struct i2c_device_id *client_id);
int	touch_remove(struct i2c_client *client);


// Kinsey:
#define WMT_TS_I2C_NAME    "wmt-ts"
static struct i2c_client *l_client;




//----------------------------------------------
irqreturn_t touch_irq(int irq, void *handle)
{
	struct touch *ts = handle;
	if (gpio_irqstatus(ts->pdata->irq_gpio)){
		wmt_gpio_ack_irq(ts->pdata->irq_gpio);
		if (is_gpio_irqenable(ts->pdata->irq_gpio)){
		    wmt_gpio_mask_irq(ts->pdata->irq_gpio);
			#ifdef CONFIG_HAS_EARLYSUSPEND
		    	if(!ts->earlysus) 
					queue_work(ts->work_queue, &ts->work);
			#else
		    	queue_work(ts->work_queue, &ts->work);
			#endif
		}
		return IRQ_HANDLED;
    }
	return IRQ_NONE;
	
}

//----------------------------------------------
static void touch_work_q(struct work_struct *work)
{
	struct touch *ts = container_of(work, struct touch, work);
	ts->pdata->touch_work(ts);
}

//----------------------------------------------
static void touch_key_report(struct touch *ts, unsigned char button_data)
{
	static button_u	button_old; 
	button_u button_new;

	button_new.ubyte = button_data;
	if(button_old.ubyte != button_new.ubyte) {
		if((button_old.bits.bt0_press != button_new.bits.bt0_press) && (ts->pdata->keycnt > 0)) {
			if(button_new.bits.bt0_press)	input_report_key(ts->input, ts->pdata->keycode[0], true);
			else							input_report_key(ts->input, ts->pdata->keycode[0], false);
			#if defined(DEBUG_TOUCH_KEY)
				dbg("keycode[0](0x%04X) %s\n", ts->pdata->keycode[0], button_new.bits.bt0_press ? "press":"release");
			#endif
		}
		if((button_old.bits.bt1_press != button_new.bits.bt1_press) && (ts->pdata->keycnt > 1)) {
			if(button_new.bits.bt1_press)	input_report_key(ts->input, ts->pdata->keycode[1], true);
			else							input_report_key(ts->input, ts->pdata->keycode[1], false);
			#if defined(DEBUG_TOUCH_KEY)
				dbg("keycode[1](0x%04X) %s\n", ts->pdata->keycode[1], button_new.bits.bt1_press ? "press":"release");
			#endif
		}
		if((button_old.bits.bt2_press != button_new.bits.bt2_press) && (ts->pdata->keycnt > 2)) {
			if(button_new.bits.bt2_press)	input_report_key(ts->input, ts->pdata->keycode[2], true);
			else							input_report_key(ts->input, ts->pdata->keycode[2], false);
			#if defined(DEBUG_TOUCH_KEY)
				dbg("keycode[2](0x%04X) %s\n", ts->pdata->keycode[2], button_new.bits.bt2_press ? "press":"release");
			#endif
		}
		if((button_old.bits.bt3_press != button_new.bits.bt3_press) && (ts->pdata->keycnt > 3)) {
			if(button_new.bits.bt3_press)	input_report_key(ts->input, ts->pdata->keycode[3], true);
			else							input_report_key(ts->input, ts->pdata->keycode[3], false);
			#if defined(DEBUG_TOUCH_KEY)
				dbg("keycode[3](0x%04X) %s\n", ts->pdata->keycode[3], button_new.bits.bt3_press ? "press":"release");
			#endif
		}
		if((button_old.bits.bt4_press != button_new.bits.bt4_press) && (ts->pdata->keycnt > 4)) {
			if(button_new.bits.bt4_press)	input_report_key(ts->input, ts->pdata->keycode[4], true);
			else							input_report_key(ts->input, ts->pdata->keycode[4], false);
			#if defined(DEBUG_TOUCH_KEY)
				dbg("keycode[4](0x%04X) %s\n", ts->pdata->keycode[4], button_new.bits.bt4_press ? "press":"release");
			#endif
		}
		if((button_old.bits.bt5_press != button_new.bits.bt5_press) && (ts->pdata->keycnt > 5)) {
			if(button_new.bits.bt5_press)	input_report_key(ts->input, ts->pdata->keycode[5], true);
			else							input_report_key(ts->input, ts->pdata->keycode[5], false);
			#if defined(DEBUG_TOUCH_KEY)
				dbg("keycode[5](0x%04X) %s\n", ts->pdata->keycode[5], button_new.bits.bt5_press ? "press":"release");
			#endif
		}
		if((button_old.bits.bt6_press != button_new.bits.bt6_press) && (ts->pdata->keycnt > 6)) {
			if(button_new.bits.bt6_press)	input_report_key(ts->input, ts->pdata->keycode[6], true);
			else							input_report_key(ts->input, ts->pdata->keycode[6], false);
			#if defined(DEBUG_TOUCH_KEY)
				dbg("keycode[6](0x%04X) %s\n", ts->pdata->keycode[6], button_new.bits.bt6_press ? "press":"release");
			#endif
		}
		if((button_old.bits.bt7_press != button_new.bits.bt7_press) && (ts->pdata->keycnt > 7)) {
			if(button_new.bits.bt7_press)	input_report_key(ts->input, ts->pdata->keycode[7], true);
			else							input_report_key(ts->input, ts->pdata->keycode[7], false);
			#if defined(DEBUG_TOUCH_KEY)
				dbg("keycode[7](0x%04X) %s\n", ts->pdata->keycode[7], button_new.bits.bt7_press ? "press":"release");
			#endif
		}
		button_old.ubyte = button_new.ubyte;
	}
}

//----------------------------------------------
static void touch_report_protocol_a(struct touch *ts)
{
	int id;
	
	for(id = 0; id < ts->pdata->max_fingers; id++) {

		if(ts->finger[id].event == TS_EVENT_UNKNOWN)		continue;
		
		if(ts->finger[id].event != TS_EVENT_RELEASE) {
			if(ts->pdata->id_max)		input_report_abs(ts->input, ABS_MT_TRACKING_ID, ts->finger[id].id);
			if(ts->pdata->area_max)		input_report_abs(ts->input, ABS_MT_TOUCH_MAJOR, ts->finger[id].area ? ts->finger[id].area : 10);
			if(ts->pdata->press_max)	input_report_abs(ts->input, ABS_MT_PRESSURE, ts->finger[id].pressure);
	
			input_report_abs(ts->input, ABS_MT_POSITION_X, 	ts->finger[id].x);
			input_report_abs(ts->input, ABS_MT_POSITION_Y,	ts->finger[id].y);
			dbg("%s : id = %d, x = %d, y = %d\n", __func__, ts->finger[id].id, ts->finger[id].x, ts->finger[id].y);
		}
		else {
			ts->finger[id].event = TS_EVENT_UNKNOWN;
			dbg("%s : release id = %d\n", __func__, ts->finger[id].id);
		}

		input_mt_sync(ts->input);
	}

	input_sync(ts->input);
}

//----------------------------------------------
static void touch_report_protocol_b(struct touch *ts)
{
	int	id;
#if defined(DEBUG_TOUCH)
	char *event_str[] = {"unknown", "press", "move", "release"};
#endif

	for(id = 0; id < ts->pdata->max_fingers; id++) {
		if((ts->finger[id].event == TS_EVENT_UNKNOWN) || (ts->finger[id].status == false))	
			continue;

		input_mt_slot(ts->input, id);	
		ts->finger[id].status = false;	

		if(ts->finger[id].event != TS_EVENT_RELEASE) {
			input_mt_report_slot_state(ts->input, MT_TOOL_FINGER, true);

			input_report_abs(ts->input, ABS_MT_TRACKING_ID, ts->finger[id].id);
			input_report_abs(ts->input, ABS_MT_POSITION_X, 	ts->finger[id].x);
			input_report_abs(ts->input, ABS_MT_POSITION_Y,	ts->finger[id].y);

			if(ts->pdata->area_max)		input_report_abs(ts->input, ABS_MT_TOUCH_MAJOR, ts->finger[id].area ? ts->finger[id].area : 10);
			if(ts->pdata->press_max)	input_report_abs(ts->input, ABS_MT_PRESSURE, ts->finger[id].pressure);
	
#if defined(DEBUG_TOUCH)
			dbg("%s : event = %s, slot = %d, id = %d, x = %d, y = %d\n", __func__, event_str[ts->finger[id].event],  id, ts->finger[id].id, ts->finger[id].x, ts->finger[id].y);
#endif
		}
		else {
#if defined(DEBUG_TOUCH)
			dbg("%s : event = %s, slot = %d, id = %d\n", __func__, event_str[ts->finger[id].event], id, ts->finger[id].id);
#endif
			ts->finger[id].event = TS_EVENT_UNKNOWN;
			input_mt_report_slot_state(ts->input, MT_TOOL_FINGER, false);
		}
	}
	input_sync(ts->input);
}

//----------------------------------------------
static void touch_event_clear(struct touch *ts)
{
	unsigned char id;
	
	for(id = 0; id < ts->pdata->max_fingers; id++) {
		if(ts->finger[id].event == TS_EVENT_MOVE) {
			ts->finger[id].status	= true;
			ts->finger[id].event 	= TS_EVENT_RELEASE;
		}
	}
	ts->pdata->report(ts);
	if(ts->pdata->keycode)
		ts->pdata->key_report(ts, 0x00);
}

//----------------------------------------------
static int touch_input_open(struct input_dev *input)
{
	struct touch *ts = input_get_drvdata(input);

	ts->pdata->enable(ts);

	dbg("%s\n", __func__);
	
	return 0;
}

//----------------------------------------------
static void touch_input_close(struct input_dev *input)
{
	struct touch *ts = input_get_drvdata(input);

	ts->pdata->disable(ts);

	dbg("%s\n", __func__);
}

//----------------------------------------------
static int touch_check_functionality(struct touch_pdata *pdata)
{
	if(!pdata) {
		errlog("Error : Platform data is NULL pointer!\n");	return	-1;
	}

	pdata->i2c_read			= sn310m_i2c_read;
	pdata->i2c_write		= sn310m_i2c_write;
		
	pdata->i2c_boot_read	= sn310m_i2c_read;
	pdata->i2c_boot_write	= sn310m_i2c_write;

	pdata->enable			= sn310m_enable;
	pdata->disable			= sn310m_disable;
	pdata->probe			= sn310m_probe;

	if(!pdata->report) {
		if(pdata->id_max)	pdata->report = touch_report_protocol_b;
		else				pdata->report = touch_report_protocol_a;
	}
	if(!pdata->key_report)	pdata->key_report = touch_key_report;

	pdata->touch_work = sn310m_work;
	
	if(!pdata->irq_func) pdata->irq_func = touch_irq;

	if(!pdata->event_clear) pdata->event_clear = touch_event_clear;
		
#ifdef	CONFIG_HAS_EARLYSUSPEND
	if(!pdata->resume)	pdata->resume	= touch_resume;
	if(!pdata->suspend)	pdata->suspend	= touch_suspend;
#endif

	//pdata->irq_gpio = 7;

	return	0;
}

//----------------------------------------------
void touch_hw_reset(struct touch *ts)
{
	if(ts->pdata->reset_gpio) {
		if(gpio_request(ts->pdata->reset_gpio, "touch reset")) {
			errlog("--------------------------------------------------------\n");
			errlog("%s : request port error!\n", "touch reset");
			errlog("--------------------------------------------------------\n");
		}
		else {
			if(ts->pdata->power) {
				/* power sequence: reset low -> power on -> reset high */
				gpio_direction_output(ts->pdata->reset_gpio, 0);
				gpio_set_value(ts->pdata->reset_gpio, 0); 
				mdelay(15);
				ts->pdata->power(1);
				mdelay(50);
				gpio_set_value(ts->pdata->reset_gpio, 1); 
				mdelay(15);
			}
			else {
				/* if there is no power control for touch, then just do reset (high -> low -> high) */
				gpio_direction_output(ts->pdata->reset_gpio, 1);
				gpio_set_value(ts->pdata->reset_gpio, 1); 
				mdelay(15);
				gpio_set_value(ts->pdata->reset_gpio, 0); 
				mdelay(20);
				gpio_set_value(ts->pdata->reset_gpio, 1); 
				mdelay(15);
			}
		}
	}	
}

//----------------------------------------------
int	touch_info_display(struct touch *ts)
{
	errlog("--------------------------------------------------------\n");
	errlog("           TOUCH SCREEN INFORMATION\n");
	errlog("--------------------------------------------------------\n");
	if(ts->pdata->irq_gpio)	{
		errlog("TOUCH INPUT Name = %s\n", ts->pdata->name);
		
		switch(ts->pdata->irq_mode)	{
			default	:
			case	IRQ_MODE_THREAD:	errlog("TOUCH IRQ Mode   = %s\n", "IRQ_MODE_THREAD");	break;
			case	IRQ_MODE_NORMAL:	errlog("TOUCH IRQ Mode   = %s\n", "IRQ_MODE_NORMAL");	break;
			case	IRQ_MODE_POLLING:	errlog("TOUCH IRQ Mode   = %s\n", "IRQ_MODE_POLLING");	break;
		}
		errlog("TOUCH F/W Version = %d.%02d\n", ts->fw_version / 100, ts->fw_version % 100);
		errlog("TOUCH FINGRES MAX = %d\n", ts->pdata->max_fingers);
		errlog("TOUCH ABS X MAX = %d, TOUCH ABS X MIN = %d\n", ts->pdata->abs_max_x, ts->pdata->abs_min_x);
		errlog("TOUCH ABS Y MAX = %d, TOUCH ABS Y MIN = %d\n", ts->pdata->abs_max_y, ts->pdata->abs_min_y);
	
		if(ts->pdata->area_max)
			errlog("TOUCH MAJOR MAX = %d, TOUCH MAJOR MIN = %d\n", ts->pdata->area_max, ts->pdata->area_min);
		
		if(ts->pdata->press_max)
			errlog("TOUCH PRESS MAX = %d, TOUCH PRESS MIN = %d\n", ts->pdata->press_max, ts->pdata->press_min);
	
		if(ts->pdata->id_max) {
			errlog("TOUCH ID MAX = %d, TOUCH ID MIN = %d\n", ts->pdata->id_max, ts->pdata->id_min);
			errlog("Mulit-Touch Protocol-B Used.\n");
		}
		else
			errlog("Mulit-Touch Protocol-A Used.\n");
	
		if(ts->pdata->gpio_init)	
			errlog("GPIO early-init function implemented\n");
			
		if(ts->pdata->reset_gpio)	
			errlog("H/W Reset function implemented\n");
	
	#ifdef	CONFIG_HAS_EARLYSUSPEND
			errlog("Early-suspend function implemented\n");
	#endif
		if(ts->pdata->fw_control)
			errlog("Firmware update function(sysfs control) implemented\n");
		
		/* flashing sample is not implemented yet */	
		if(ts->pdata->flash_firmware)
			errlog("Firmware update function(udev control) implemented\n");
	
		if(ts->pdata->calibration)
			errlog("Calibration function implemented\n");
	}
	else {
		errlog("TOUCH INPUT Name = %s\n", ts->pdata->name);
		errlog("Dummy Touchscreen driver!\n");
	}
	errlog("--------------------------------------------------------\n");
	return	0;
}

//----------------------------------------------
int touch_probe(struct i2c_client *client, const struct i2c_device_id *client_id)
{
	return -1;
}

//----------------------------------------------
//
// Power Management function
//
//----------------------------------------------
#ifdef	CONFIG_HAS_EARLYSUSPEND
static void touch_suspend(struct early_suspend *h)
{
	struct touch *ts = container_of(h, struct touch, power);

	dbg("%s++\n", __func__);
	
	/* TSC enters deep sleep mode */
	dbg("[%s] touch reset goes low!\n", __func__);
	gpio_direction_output(ts->pdata->reset_gpio, 0);
	gpio_set_value(ts->pdata->reset_gpio, 0); 

	ts->pdata->disable(ts);
}

//----------------------------------------------
static void touch_resume(struct early_suspend *h)
{
	struct touch *ts = container_of(h, struct touch, power);

	dbg("%s++\n", __func__);
	
	/* TSC enters active mode */
	dbg("[%s] touch reset goes high!\n", __func__);
	gpio_direction_output(ts->pdata->reset_gpio, 1);
	gpio_set_value(ts->pdata->reset_gpio, 1); 

	ts->pdata->enable(ts);
}
#endif

//----------------------------------------------
int touch_remove(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct touch *ts = dev_get_drvdata(dev);

	dbg("touch_remove++");

	if(ts->irq)	free_irq(ts->irq, ts);

	if(ts->pdata->reset_gpio) gpio_free(ts->pdata->reset_gpio);

	if(ts->pdata->irq_gpio) gpio_free(ts->pdata->irq_gpio);

	input_unregister_device(ts->input);
	
	dev_set_drvdata(dev, NULL);

#if defined(SN310M_NATIVE_INTERFACE)
	P_SN310M_Dist_Remove();
#endif

	kfree(ts->finger); ts->finger = NULL;
	kfree(ts); ts = NULL;

	return 0;
}

#if defined(SN310M_NATIVE_INTERFACE)
#define	SN310M_DIST_MINOR	250

typedef struct {
	unsigned int	addr;
	short			*buf;
	unsigned int	size;
} packet_t;

static const struct file_operations SN310M_Dist_Fops =
{
	.owner = THIS_MODULE,
	.open = P_SN310M_Dist_Open,
	.unlocked_ioctl = P_SN310M_Dist_Ioctl,
};


static struct miscdevice SN310M_Dist_MiscDev =
{
	.minor = SN310M_DIST_MINOR,
	.name = "sn310m_dist",
	.fops = &SN310M_Dist_Fops,
	.mode = 0x666,
};


static long P_SN310M_Dist_Ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	packet_t* packet = (packet_t*)arg;
	int i;

	mutex_lock(&g_ts->mutex);
	switch(cmd)	{
		case 0: // write data
			if(packet->size) {
				unsigned short addr = (packet->addr >> 8) | (packet->addr & 0x00ff) << 8;
				g_ts->pdata->i2c_write(g_ts->client, (unsigned char *)&addr, sizeof(addr), (unsigned char *)packet->buf, packet->size*2);
				dbg("Request I2C Write\n");
			}
			break;

		case 1: // read data
			if(packet->size) {			
				unsigned short addr = (packet->addr >> 8) | (packet->addr & 0x00ff) << 8;
				short buffer[500] = {0, };	

				g_ts->pdata->i2c_read(g_ts->client, (unsigned char *)&addr, sizeof(addr), (unsigned char *)buffer, packet->size*2);
				for(i = 0; (i < packet->size) && (i < 500); i++) {
					packet->buf[i] = buffer[i];
				}
				dbg("Request I2C Read\n");
			}
			break;

		default:
			mutex_unlock(&g_ts->mutex);
			return -ENOIOCTLCMD;
	}

	mutex_unlock(&g_ts->mutex);
	return 0;
}

static int P_SN310M_Dist_Open(struct inode *inode, struct file *file)
{
	return 0;
}

static int P_SN310M_Dist_Probe(struct touch* ts)
{
	int result = 0;

	g_ts = ts;
	result = misc_register(&SN310M_Dist_MiscDev);
	if(result == 0) {
		dbg("succeeded to register sn310m_misc_device \n");
	}
	else {
		errlog("failed to register sn310m_misc_device \n");
	}

	return result;
}

static void P_SN310M_Dist_Remove(void)
{
	misc_deregister(&SN310M_Dist_MiscDev);
	g_ts = NULL;
}
#endif
static const struct i2c_device_id sample_ts_id[] = {
	{ I2C_TOUCH_NAME, 0 },
	{},	
};



#define	TS_DRIVER_NAME	"wmt-touch"

static void wmt_ts_platform_release(struct device *device)
{
	dbg("wmt_ts_platform_release\n");
	return;
}

static struct platform_device wmt_ts_plt_device = {
    .name           = TS_DRIVER_NAME,
    .id             = 0,
    .dev            = {
        .release = wmt_ts_platform_release,
    },
};

static	int	sn310m_keycode[] = {
	KEY_HOME,	KEY_MENU,	KEY_BACK,	KEY_SEARCH
};

struct touch_pdata	sn310m_touch_pdata = {
	
	.name 			= "sn310m",			// input drv name
	.irq_gpio 		= 7,//SAMPLE_GPIO_0,	// irq gpio define
	.reset_gpio		= 4,//SAMPLE_GPIO_1,	// reset gpio define
	.reset_level	= 0,				// reset level setting (1 = High reset, 0 = Low reset)

	.irq_mode		= IRQ_MODE_NORMAL,	// IRQ_MODE_THREAD, IRQ_MODE_NORMAL, IRQ_MODE_POLLING
	.irq_flags		= IRQF_SHARED ,//IRQF_TRIGGER_FALLING | IRQF_DISABLED,

	.abs_max_x		= 600, 
	.abs_max_y		= 1024,
	
	.area_max		= 10, 
	.press_max		= 255,
	
	.id_max			= 10 + 1, 
	.id_min			= 0,
	
	.vendor			= 0x16B4, 
	.product		= 0x0310, 
	.version		= 0x0001,

	.max_fingers	= 5,
	
	.keycnt			= 4,
	.keycode		= sn310m_keycode, 
	.lcd_exchg		= 0,

	//--------------------------------------------
	// Control function 
	//--------------------------------------------
	.touch_work		= sn310m_work,
	.enable			= sn310m_enable,
	.disable		= sn310m_disable,
	.early_probe	= sn310m_early_probe,
	.probe			= sn310m_probe,

	//--------------------------------------------
	// I2C control function
	//--------------------------------------------
	.i2c_write		= sn310m_i2c_write,
	.i2c_read		= sn310m_i2c_read,

	//--------------------------------------------
	// Calibration function
	//--------------------------------------------
	.calibration	= sn310m_calibration,

	//--------------------------------------------
	// Firmware update control function
	//--------------------------------------------
	.fw_filename	= "sn310m_fw.bin",
	.fw_filesize	= (10 * 1024),	// 10K bytes
	.input_open		= sn310m_input_open,
	.flash_firmware	= sn310m_flash_firmware,
};


int temp;
static int wmt_ts_probe(struct platform_device *pdev)
{
		int rc = -1;
		struct i2c_client *client = l_client;
		struct device *dev = &client->dev;
		struct touch *ts;
		

		dbg("wmt_ts_probe\n");
	
		if(!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
			dev_err(&client->dev, "i2c byte data not supported\n");
			return -EIO;
		}


		client->dev.platform_data = &sn310m_touch_pdata;
		
		if(touch_check_functionality(client->dev.platform_data) < 0) {
			dev_err(&client->dev, "Platform data is not available!\n");
			return -EINVAL;
		}
	
		if(!(ts = kzalloc(sizeof(struct touch), GFP_KERNEL))) {
			errlog("touch struct malloc error!\n");
			return	-ENOMEM;
		}
		ts->client	= client;
		ts->pdata	= client->dev.platform_data;


		/* by limst, setting gpio for IRQ */
		if(ts->pdata->irq_gpio) {
			int ret;
	
			ts->irq = IRQ_GPIO;//MSM_GPIO_TO_INT(ts->pdata->irq_gpio); 
			dbg("IRQ_GPIO(%d) IRQ(%d) REG\n", ts->pdata->irq_gpio, ts->irq);
	
			ret = gpio_request(ts->pdata->irq_gpio, "touch_int");
			if(ret < 0) 
				errlog("FAIL: touch_int gpio_request\n");
			else
				dbg("OK: touch_int gpio_request value(%d)\n", gpio_get_value(ts->pdata->irq_gpio));

			wmt_gpio_setpull(ts->pdata->irq_gpio,WMT_GPIO_PULL_UP);
			gpio_direction_input(ts->pdata->irq_gpio);
			wmt_gpio_set_irq_type(ts->pdata->irq_gpio, IRQ_TYPE_EDGE_FALLING);
		}
	
		i2c_set_clientdata(client, ts);

		if(ts->pdata->max_fingers) {
			if(!(ts->finger = kzalloc(sizeof(finger_t) * ts->pdata->max_fingers, GFP_KERNEL))) {
				kfree(ts);
				errlog("touch data struct malloc error!\n");	
				return	-ENOMEM;
			}
		}

		if(ts->pdata->gpio_init) ts->pdata->gpio_init();
	
		if(ts->pdata->early_probe) {
			if((rc = ts->pdata->early_probe(ts)) < 0)	
				goto err_free_mem;
		}


		dev_set_drvdata(dev, ts);

		if(!(ts->input = input_allocate_device())) 
			goto err_free_mem;
	
		snprintf(ts->phys, sizeof(ts->phys), "%s/input0", ts->pdata->name);
	
		if(!ts->pdata->input_open)	ts->input->open 	= touch_input_open;
		else						ts->input->open 	= ts->pdata->input_open;
		if(!ts->pdata->input_close) ts->input->close	= touch_input_close;
		else						ts->input->close	= ts->pdata->input_close;
	
		/* 
		 * by limst, for the test purpose, 
		 * input device's name is forcedly set to the name of android idc file
		 */
		ts->input->name = "qwerty";//idc's filename //"touch_dev";
		//ts->input->name = ts->pdata->name;
		ts->input->phys 		= ts->phys;
		ts->input->dev.parent	= dev;
		ts->input->id.bustype	= BUS_I2C;
		
		ts->input->id.vendor	= ts->pdata->vendor;
		ts->input->id.product	= ts->pdata->product;
		ts->input->id.version	= ts->pdata->version;
	
		set_bit(EV_SYN, ts->input->evbit);		
		set_bit(EV_ABS, ts->input->evbit);
	
		/* Register Touch Key Event */
		if(ts->pdata->keycode) {
			int key;
	
			set_bit(EV_KEY, ts->input->evbit);
	
			for(key = 0; key < ts->pdata->keycnt; key++) {
				if(ts->pdata->keycode[key] <= 0) continue;
				set_bit(ts->pdata->keycode[key] & KEY_MAX, ts->input->keybit);
			}
		}

		input_set_drvdata(ts->input, ts);

		if (sn310m_touch_pdata.lcd_exchg) {
			input_set_abs_params(ts->input, ABS_MT_POSITION_X, ts->pdata->abs_min_y, ts->pdata->abs_max_y, 0, 0);
			input_set_abs_params(ts->input, ABS_MT_POSITION_Y, ts->pdata->abs_min_x, ts->pdata->abs_max_x, 0, 0);
		} else {
			input_set_abs_params(ts->input, ABS_MT_POSITION_X, ts->pdata->abs_min_x, ts->pdata->abs_max_x, 0, 0);
			input_set_abs_params(ts->input, ABS_MT_POSITION_Y, ts->pdata->abs_min_y, ts->pdata->abs_max_y, 0, 0);
		}

		if(ts->pdata->area_max)
			input_set_abs_params(ts->input, ABS_MT_TOUCH_MAJOR, ts->pdata->area_min, ts->pdata->area_max,	0, 0);
			
		if(ts->pdata->press_max)
			input_set_abs_params(ts->input, ABS_MT_PRESSURE, ts->pdata->press_min, ts->pdata->press_max, 0, 0);
	
		if(ts->pdata->id_max) {
			input_set_abs_params(ts->input, ABS_MT_TRACKING_ID, ts->pdata->id_min, ts->pdata->id_max, 0, 0);
			input_mt_init_slots(ts->input, ts->pdata->max_fingers);
		}
		
	
		mutex_init(&ts->mutex);
		if(ts->irq) {
			switch(ts->pdata->irq_mode) {
				default :
				case	IRQ_MODE_THREAD:
					INIT_WORK(&ts->work, touch_work_q);
					if((ts->work_queue = create_singlethread_workqueue("work_queue")) == NULL) 
						goto err_free_input_mem;
	
					if((rc = request_threaded_irq(ts->irq, NULL, ts->pdata->irq_func,
							IRQF_TRIGGER_FALLING | IRQF_ONESHOT, ts->pdata->name, ts))) {
						dev_err(dev, "threaded irq %d request fail!\n", ts->irq);
						goto err_free_input_mem;
					}
					break;
				case	IRQ_MODE_NORMAL:
					INIT_WORK(&ts->work, touch_work_q);
					if((ts->work_queue = create_singlethread_workqueue("work_queue")) == NULL) 
						goto err_free_input_mem;

					if((rc = request_irq(ts->irq, ts->pdata->irq_func, ts->pdata->irq_flags, ts->pdata->name, ts))) {
						errlog("irq %d request fail!\n", ts->irq);
						goto err_free_input_mem;
					}
					dbg("irq %d request ok!\n", ts->irq);
					break;
				case	IRQ_MODE_POLLING:
					errlog("Error IRQ_MODE POLLING!! but defined irq_gpio\n");
					break;
			} /* end of switch */
		}
		ts->disabled = true;

		if((rc = input_register_device(ts->input))) {
			dev_err(dev, "(%s) input register fail!\n", ts->input->name);
			goto err_free_input_mem;
		}

		/* by limst, added to turn on the power and reset of Touch IC */ 
		touch_hw_reset(ts);
	
#if defined(CONFIG_HAS_EARLYSUSPEND)
		if(ts->pdata->suspend)	ts->power.suspend	= ts->pdata->suspend;
		if(ts->pdata->resume)	ts->power.resume	= ts->pdata->resume;
	
		ts->power.level = EARLY_SUSPEND_LEVEL_DISABLE_FB - 1;
		
		register_early_suspend(&ts->power);
#endif
	
		if(ts->pdata->probe) {
			ts->pdata->probe(ts);
		}
	
		touch_info_display(ts);
	
#if defined(SN310M_NATIVE_INTERFACE)
		if(P_SN310M_Dist_Probe(ts) < 0) {
			errlog("P_SN310M_Dist_Probe(), fail\n");
		}
#endif
		
		return 0;
	
		free_irq(ts->irq, ts);
		input_unregister_device(ts->input);
	err_free_input_mem:
		input_free_device(ts->input);
		ts->input = NULL;
	err_free_mem:
		kfree(ts->finger); 
		ts->finger = NULL;
		kfree(ts); 
		ts = NULL;
		return rc;
}

static int wmt_ts_remove(struct platform_device *pdev)
{
		struct i2c_client *client = l_client;
		struct device *dev = &client->dev;
		struct touch *ts = dev_get_drvdata(dev);
		
		dbg("wmt_ts_remove\n");
	
		if(ts->irq) free_irq(ts->irq, ts);
	
		if(ts->pdata->reset_gpio) gpio_free(ts->pdata->reset_gpio);
	
		if(ts->pdata->irq_gpio) gpio_free(ts->pdata->irq_gpio);
		
		input_unregister_device(ts->input);
		
		dev_set_drvdata(dev, NULL);
	
		#if defined(SN310M_NATIVE_INTERFACE)
			P_SN310M_Dist_Remove();
		#endif
	
		kfree(ts->finger); ts->finger = NULL;
		kfree(ts); ts = NULL;
	
		return 0;
}

static int wmt_ts_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct i2c_client *client = l_client;
	struct device *dev = &client->dev;
	struct touch *ts = dev_get_drvdata(dev);
	
	dbg("%s++\n", __func__);
	
	/* TSC enters deep sleep mode */
	dbg("[%s] touch reset goes low!\n", __func__);
	gpio_direction_output(ts->pdata->reset_gpio, 0);
	gpio_set_value(ts->pdata->reset_gpio, 0); 


	ts->pdata->disable(ts);
	
	return 0;
}
static int wmt_ts_resume(struct platform_device *pdev)
{
	struct i2c_client *client = l_client;
	struct device *dev = &client->dev;
	struct touch *ts = dev_get_drvdata(dev);

	dbg("%s++\n", __func__);
	
	/* TSC enters active mode */
	dbg("[%s] touch reset goes high!\n", __func__);
	gpio_direction_output(ts->pdata->reset_gpio, 1);
	gpio_set_value(ts->pdata->reset_gpio, 1); 
	
	ts->pdata->enable(ts);
	//touch_hw_reset(ts);

	return 0;
}


static struct platform_driver wmt_ts_plt_driver = {
	.driver = {
		.name = TS_DRIVER_NAME,
		.owner	= THIS_MODULE,
	 },
	.probe 		= wmt_ts_probe,
	.remove 	= wmt_ts_remove,
	.suspend        = wmt_ts_suspend,
	.resume         = wmt_ts_resume,
};



struct i2c_board_info ts_i2c_board_info = {
	.type          = WMT_TS_I2C_NAME,
	.flags         = 0x00,
	.platform_data = NULL,
	.archdata      = NULL,
	.irq           = -1,
};

static int ts_i2c_register_device (void)
{
	struct i2c_board_info *ts_i2c_bi;
	struct i2c_adapter *adapter = NULL;

	ts_i2c_board_info.addr =(unsigned short) 0x3c;
	ts_i2c_bi = &ts_i2c_board_info;
	adapter = i2c_get_adapter(1);/*in bus 1*/

	if (NULL == adapter) {
		errlog("can not get i2c adapter, client address error\n");
		return -1;
	}
	l_client = i2c_new_device(adapter, ts_i2c_bi);
	if (l_client == NULL) {
		errlog("allocate i2c client failed\n");
		return -1;
	}
	i2c_put_adapter(adapter);
	return 0;
}

static void ts_i2c_unregister_device(void)
{
	if (l_client != NULL)
	{
		i2c_unregister_device(l_client);
		l_client = NULL;
	}
}

static struct tp_info l_tpinfo;
static int wmt_check_touch_env(void)
{
	int ret = 0;
	int len = 127;
    char retval[200] = {0};
	char *p=NULL;
	char *s=NULL;
	int Enable=0;
	
    // Get u-boot parameter
	ret = wmt_getsyspara("wmt.io.touch", retval, &len);
	if(ret){
		errlog("Read wmt.io.touch Failed.\n");
		return -EIO;
	}
	memset(&l_tpinfo,0,sizeof(l_tpinfo));
	
	p = retval;
	sscanf(p,"%d:", &Enable);
	p = strchr(p,':');
	p++;
	s = strchr(p,':');
	strncpy(l_tpinfo.name,p, (s-p));
	p = s+1;
	//dbg("ts_name=%s\n", l_tpinfo.name);

	ret = sscanf(p,"%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d",
		&l_tpinfo.irq_gpio,&l_tpinfo.panelres_x,&l_tpinfo.panelres_y,&l_tpinfo.rst_gpio,
		&(l_tpinfo.xaxis),&(l_tpinfo.xdir),&(l_tpinfo.ydir),
		&(l_tpinfo.max_finger_num),&l_tpinfo.i2caddr,&l_tpinfo.low_Impendence_mode,&l_tpinfo.download_option);
	
	if (ret < 8){
		errlog("Wrong format ts u-boot param(%d)!\nwmt.io.touch=%s\n",ret,retval);
		return -ENODEV;
	}
	
	//check touch enable
	if(Enable == 0){
		errlog("Touch Screen Is Disabled.\n");
		return -ENODEV;
	}
	if (strstr(l_tpinfo.name, sn310m_touch_pdata.name) == NULL){
		errlog("Can't find %s in the wmt.io.touch\n", sn310m_touch_pdata.name);
		return -ENODEV;
	}

	errlog("p.x = %d, p.y = %d, gpio=%d, resetgpio=%d,xaxis=%d,xdir=%d,ydri=%d,maxfingernum=%d,,i2c_addr=0x%X,low_Impendence_mode=%d,s_download_option=%d\n",
	          l_tpinfo.panelres_x, l_tpinfo.panelres_y, l_tpinfo.irq_gpio, l_tpinfo.rst_gpio,
	          l_tpinfo.xaxis,l_tpinfo.xdir,l_tpinfo.ydir,
	          l_tpinfo.max_finger_num,l_tpinfo.i2caddr,l_tpinfo.low_Impendence_mode,l_tpinfo.download_option);

	sn310m_touch_pdata.irq_gpio = l_tpinfo.irq_gpio;
	sn310m_touch_pdata.reset_gpio = l_tpinfo.rst_gpio;
	sn310m_touch_pdata.abs_max_x = l_tpinfo.panelres_x;
	sn310m_touch_pdata.abs_max_y = l_tpinfo.panelres_y;

	memset(retval,0,sizeof(retval));
	ret = wmt_getsyspara("wmt.display.fb0", retval, &len);
	if (!ret) {
		int tmp[6];
		p = retval;
		sscanf(p, "%d:[%d:%d:%d:%d:%d", &tmp[0], &tmp[1], &tmp[2], &tmp[3], &tmp[4], &tmp[5]);
		if (tmp[4] > tmp[5])
			sn310m_touch_pdata.lcd_exchg = 1;
	}

	return 0;
}


static int __init sample_touch_init(void)
{
	int ret = 0;

	if(wmt_check_touch_env())
		return -ENODEV;


	if (ts_i2c_register_device()<0){
		errlog("Error to run ts_i2c_register_device()!\n");
		return -1;
	}
	
	ret = platform_device_register(&wmt_ts_plt_device);
	if(ret){
		errlog("wmt ts plat device register failed!\n");
		return ret;
	}
	ret = platform_driver_register(&wmt_ts_plt_driver);
	if(ret){
    	errlog("can not register platform_driver_register\n");
    	platform_device_unregister(&wmt_ts_plt_device);
    	return ret;
	}
	return 0;
}

static void sample_touch_exit(void)
{
	platform_driver_unregister(&wmt_ts_plt_driver);
	platform_device_unregister(&wmt_ts_plt_device);
	ts_i2c_unregister_device();
	
	return;
}


module_init(sample_touch_init);
module_exit(sample_touch_exit);

#ifndef MODULE
__initcall(sample_touch_init);
#endif



MODULE_AUTHOR("SEMISENS Co., Ltd.");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Touchscreen Driver for SN310M");

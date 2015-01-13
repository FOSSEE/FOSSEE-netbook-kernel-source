/****************************************************************
 *
 * touch.c : I2C Touchscreen driver
 *
 * Copyright (c) 2013 SEMISENS Co.,Ltd
 *      http://www.semisens.com
 *
 ****************************************************************/
#ifndef _TOUCH_H_
#define _TOUCH_H_

//----------------------------------------------
// extern function define
//----------------------------------------------
extern void touch_hw_reset(struct touch *ts);
extern int touch_info_display(struct touch *ts);
#if 0 /* depends on kernel version */
extern int touch_probe(struct i2c_client *client);
extern int touch_remove(struct device *dev);
#else
extern int touch_probe(struct i2c_client *client, const struct i2c_device_id *client_id);
extern int touch_remove(struct i2c_client *client);
#endif

struct tp_info
{
	char name[64];
	unsigned int xaxis; //0: x,  1: x swap with y
	unsigned int xdir; // 1: positive,-1: revert
	unsigned int ydir; // 1: positive,-1: revert
	unsigned int max_finger_num;
	unsigned int download_option;	// 0: disable 1:force download 2:force cancel download
	unsigned int low_Impendence_mode; //   0: High Impendence Mode 1: Low Impendence Mode
	unsigned int irq_gpio;
	unsigned int rst_gpio;
	unsigned int panelres_x;
	unsigned int panelres_y;
	unsigned int i2caddr;
	unsigned int lcd_exchg;
#if 0	
	struct input_dev *inputdev;
	struct work_struct int_work;
	struct i2c_client *i2cclient;
	struct workqueue_struct *wq;
#if SUPPORT_TS_KEY
	int key_num;
#endif
#endif

};



#endif /* _TOUCH_H_ */

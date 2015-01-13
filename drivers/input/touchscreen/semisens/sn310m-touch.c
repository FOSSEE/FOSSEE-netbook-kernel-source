/****************************************************************
 *
 * sn310m-touch.c : I2C Touchscreen driver (platform data struct)
 *
 * Copyright (c) 2013 SEMISENS Co.,Ltd
 *      http://www.semisens.com
 *
 ****************************************************************/
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

#include <linux/gpio.h>
#include <mach/wmt_iomux.h>

//----------------------------------------------
#include "sn310m-touch-pdata.h"
#include "sn310m-touch.h"
#include "touch.h"


//----------------------------------------------
unsigned char sn310m_id_tracking(struct	touch *ts, unsigned char find_id);

//----------------------------------------------
// Touch i2c control function
//----------------------------------------------
int	sn310m_i2c_read(struct i2c_client *client, unsigned char *cmd, unsigned int cmd_len, unsigned char *data, unsigned int len)
{
	struct i2c_msg msg[2];
	int ret = 0;
	unsigned char i = 0;
	unsigned char cmd_tmp[10] = {0, };

	if((len == 0) || (data == NULL)) {
		dev_err(&client->dev, "I2C read error: Null pointer or length == 0\n");	
		return 	-1;
	}
	
	memset(cmd_tmp, 0x00, sizeof(cmd_tmp));

	if(cmd_len)	{
		for(i = 0; i < cmd_len; i++) {
			cmd_tmp[i] = cmd[cmd_len -1 -i];
		}
	}

	memset(msg, 0x00, sizeof(msg));
	msg[0].addr		= client->addr;
	msg[0].flags	= client->flags & I2C_M_TEN;
	msg[0].len		= cmd_len;
	msg[0].buf		= cmd_tmp;

	msg[1].addr		= client->addr;
	msg[1].flags	= client->flags & I2C_M_TEN;
	msg[1].flags	|= I2C_M_RD;
	msg[1].len		= len;
	msg[1].buf		= data;

	if((ret = i2c_transfer(client->adapter, msg, 2)) != 2) {
		dev_err(&client->dev, "I2C read error: (%d) reg: 0x%X len: %d\n", ret, cmd_tmp[0], len);
		return -EIO;
	}

	return 	len;
}

int	sn310m_i2c_write(struct i2c_client *client, unsigned char *cmd, unsigned int cmd_len, unsigned char *data, unsigned int len)
{
	int ret = 0;
	unsigned char block_data[10] = {0, };
	unsigned char i = 0;
	unsigned char cmd_tmp[10] = {0, };

	if((cmd_len + len) >= sizeof(block_data)) {
		dev_err(&client->dev, "I2C write error: wdata overflow reg: 0x%X len: %d\n", cmd[0], cmd_len + len);
		return	-1;
	}

	memset(block_data, 0x00, sizeof(block_data));
	memset(cmd_tmp, 0x00, sizeof(cmd_tmp));

	if(cmd_len) {
		for(i = 0; i < cmd_len; i++) {
			cmd_tmp[i] = cmd[cmd_len -1 -i];
		}
	}

	if(cmd_len)
		memcpy(&block_data[0], &cmd_tmp[0], cmd_len);

	if(len)
		memcpy(&block_data[cmd_len], &data[0], len);

	if((ret = i2c_master_send(client, block_data, (cmd_len + len))) < 0) {
		dev_err(&client->dev, "I2C write error: (%d) reg: 0x%X len: %d\n", ret, cmd[0], len);
		return ret;
	}
	
	return len;
}

//----------------------------------------------
// Touch initialize & finalize function
//----------------------------------------------
int	sn310m_input_open(struct input_dev *input)
{
	struct touch *ts = input_get_drvdata(input);

	dbg("%s\n", __func__);
    	
	ts->pdata->enable(ts);

	return 0;
}

void sn310m_enable(struct touch *ts)
{
	unsigned short cmd = REG_TS_STATUS;
	unsigned int rdata = 0;
	dbg("sn310m_enable++\n");
	if(ts->disabled) {
		while(!gpio_get_value(ts->pdata->irq_gpio))		
			ts->pdata->i2c_read(ts->client, (unsigned char *)&cmd, sizeof(cmd), (unsigned char *)&rdata, sizeof(rdata));
		wmt_gpio_set_irq_type(ts->pdata->irq_gpio, IRQ_TYPE_EDGE_FALLING);
		wmt_gpio_unmask_irq(ts->pdata->irq_gpio);
		dbg("enable_irq (%d)\n",ts->irq);
		ts->disabled = false;
	}
	dbg("sn310m_enable--\n");
}

void sn310m_disable(struct touch *ts)
{
	dbg("sn310m_disable++\n");
	if(!ts->disabled) {
		//disable_irq(ts->irq);//wmt_gpio_mask_irq(ts->pdata->irq_gpio);//
		wmt_gpio_mask_irq(ts->pdata->irq_gpio);
		dbg("disable_irq(ts->irq);\n");
		ts->disabled = true;
		if(ts->pdata->event_clear){
			ts->pdata->event_clear(ts);
		}
	}
	dbg("sn310m_disable--");
}

int	sn310m_early_probe(struct touch *ts)
{
	// nothing to do...

	return	0;
}

int	sn310m_probe(struct touch *ts)
{
	unsigned short cmd = REG_FIRMWARE_VERSION;
	unsigned short rdata = 0;

	if(ts->pdata->i2c_read(ts->client, (unsigned char *)&cmd, sizeof(cmd), (unsigned char *)&rdata, sizeof(rdata)) < 0) {
		errlog("fail to get touch ic firmware version.\n");
		return	-1;
	}
	
	ts->fw_version = rdata;

	dbg("touch ic firmware version : %d \n", rdata);
	
	return 	0;
}

//----------------------------------------------
// calibration function
//----------------------------------------------
int	sn310m_calibration(struct touch *ts)
{
	// nothing to do...

	return	0;
}

#define SN310M_NATIVE_INTERFACE
#if defined(SN310M_NATIVE_INTERFACE)
#include <linux/syscalls.h>
extern int g_MiscInitialize;
#endif

//----------------------------------------------
// Touch data processing function
//----------------------------------------------
void sn310m_work(struct touch *ts)
{
	unsigned char find_slot = 0;
	unsigned short cmd = 0;
	status_reg_u status;
	data_reg_t data;
	button_u button;
	unsigned int ids = 0;
	int i = 0;
	
	mutex_lock(&ts->mutex);

	cmd = REG_TS_STATUS;
	ts->pdata->i2c_read(ts->client, (unsigned char *)&cmd, sizeof(cmd), (unsigned char *)&status.uint, sizeof(status_reg_u));
	
	if(status.bits.ts_cnt <= ts->pdata->max_fingers) {
		unsigned char cnt = 0;

		if(ts->pdata->keycode && (status.bits.ts_cnt == 0)) {
			button.bits.bt0_press = (status.bits.button & 0x01) ? 1 : 0;
			button.bits.bt1_press = (status.bits.button & 0x02) ? 1 : 0;
			button.bits.bt2_press = (status.bits.button & 0x04) ? 1 : 0;
			button.bits.bt3_press = (status.bits.button & 0x08) ? 1 : 0;
			
			ts->pdata->key_report(ts, button.ubyte);
		}

		for(cnt = 0; cnt < status.bits.ts_cnt; cnt++) {
			unsigned int id;
			unsigned int x;
			unsigned int y;
			unsigned int area;
			unsigned int pressure;

			cmd = REG_TS_DATA(cnt);
			ts->pdata->i2c_read(ts->client, (unsigned char *)&cmd, sizeof(cmd),	(unsigned char *)&data.packet0, sizeof(data_reg_t));

			id = data.packet0 >> 12;
			x = data.packet0 & 0xfff;
			y = data.packet1 & 0xfff;
			area = data.packet2 & 0xfff;
			pressure = ((data.packet1 >> 8) & 0x00f0) + (data.packet2 >> 12);	

			dbg("DEBUG(%s) : cmd=%d, id=%d, x=%d, y=%d, area=%d, pressure=%d \n", __func__, cmd, id, x, y, area, pressure);
			dbg("DEBUG(%s) : pkt0=%x pkt1=%x pkt2=%x \n", __func__, data.packet0, data.packet1, data.packet2);

			if((x >= ts->pdata->abs_max_x) || (y >= ts->pdata->abs_max_y)) {
				if(ts->pdata->event_clear)		
					ts->pdata->event_clear(ts);

				dbg("ERROR(%s) : x(%d) or y(%d) value overflow!\n", __func__, x, y);
				continue;
			}

			if(ts->pdata->id_max) {
				if((id >= ts->pdata->id_max) || (id < ts->pdata->id_min)) {
					if(ts->pdata->event_clear)
						ts->pdata->event_clear(ts);

					dbg("ERROR(%s) : id(%d) value overflow!\n", __func__, id);
					continue;
				}
				if((find_slot = sn310m_id_tracking(ts, id)) == 0xFF) {
					dbg("ERROR(%s) : Empty slot not found\n", __func__);
					continue;
				}
			}	
			else {
				if(id == 0)	
					continue;

				find_slot = cnt;
			}
			
			if(ts->finger[find_slot].event == TS_EVENT_UNKNOWN)
				ts->finger[find_slot].event	= TS_EVENT_PRESS;
			else if((ts->finger[find_slot].event == TS_EVENT_PRESS) || (ts->finger[find_slot].event == TS_EVENT_MOVE))
				ts->finger[find_slot].event = TS_EVENT_MOVE;

			if (ts->pdata->lcd_exchg) {
				int tmp;
				tmp = x;
				x = y;
				y = ts->pdata->abs_max_x - tmp;
			}

			ts->finger[find_slot].status	= true;
			ts->finger[find_slot].id		= id;
			ts->finger[find_slot].x			= x;
			ts->finger[find_slot].y			= y;
			ts->finger[find_slot].area		= (ts->pdata->area_max < area) ? ts->pdata->area_max : area;
			ts->finger[find_slot].pressure	= (ts->pdata->press_max < pressure) ? ts->pdata->press_max : pressure;
			ids |= 1 << find_slot;
		}
	}

	for(i = 0; i < ts->pdata->max_fingers; i++) {
		if(!(ids & (1 << i))) {
			if(ts->finger[i].event != TS_EVENT_UNKNOWN) {
				ts->finger[i].status	= true;
				ts->finger[i].event 	= TS_EVENT_RELEASE;
			}
		}
	}

	ts->pdata->report(ts);	
	mutex_unlock(&ts->mutex);
	wmt_gpio_unmask_irq(ts->pdata->irq_gpio);
}

unsigned char sn310m_id_tracking(struct touch *ts, unsigned char find_id)
{
	unsigned char find_slot = 0xFF;
	int i = 0;

	for(i = 0; i < ts->pdata->max_fingers; i++) {
		if(ts->finger[i].id == find_id)
			find_slot = i;
			
		if((ts->finger[i].event == TS_EVENT_UNKNOWN) && (find_slot == 0xFF))	
			find_slot = i;
	}
	return	find_slot;
}

//----------------------------------------------
// Firmware update Control function
//----------------------------------------------
int sn310m_flash_firmware(struct device *dev, const char *fw_name)
{
	// nothing to do...
	return 0;
}

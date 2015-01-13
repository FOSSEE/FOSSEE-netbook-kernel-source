/*
 *	vt1603_mt_i2c.c: VT1603A Touch-Panel Controller and SAR-ADC Driver
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 *  History: 2011.Jan.21st, version: 1.00
 *
 */

#include <linux/init.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
//#include <linux/spinlock.h>
#include <linux/input.h>
#include <linux/random.h>
#include <linux/cdev.h>
#include <asm/uaccess.h>
#include <linux/i2c.h>
#include <linux/spi/spi.h>
#include <linux/platform_device.h>
#include <mach/hardware.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <mach/wmt_iomux.h>

#include "vt1609_ts.h"


//#define VT1609_DEBUG

#undef dbg 
#ifdef  VT1609_DEBUG
#define dbg(fmt, args...) printk(KERN_ERR "[%s][%d]: " fmt, __func__ , __LINE__, ##args)
#else
#define dbg(fmt, args...)
#endif

static struct vt1603_fifo px;
static struct vt1603_fifo py;
static struct class *vt1603_ts_class;
static struct vt1603_ts_pos pre_pos;
static struct vt1603_ts_cal_info g_CalcParam;
struct vt1603_ts_drvdata *pContext = NULL;

static int vt1603_ts_isPendown(struct vt1603_ts_drvdata *ts_drv);
static void vt1603_ts_dev_cleanup(struct vt1603_ts_drvdata *ts_drv);

#ifdef TOUCH_KEY
static unsigned int key_codes[TOUCH_KEY_NUM] = {  
		[0] = KEY_SEARCH,
		[1] = KEY_BACK,
		[2] = KEY_HOME,
		[3] = KEY_MENU,
};
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
static void vt1603ts_early_suspend(struct early_suspend *h);
static void vt1603ts_late_resume(struct early_suspend *h);
#endif

#ifdef TOUCH_KEY
static int setup_led_gpio(struct vt1603_ts_drvdata *ts_drv)
{
	if (ts_drv->ledgpio >= 0)
		gpio_direction_output(ts_drv->ledgpio, 0);
    
	return 0;
}

int set_key_led_gpio(struct vt1603_ts_drvdata *ts_drv, int val)
{
	if (ts_drv->ledgpio >= 0) {
		if(val)
			gpio_direction_output(ts_drv->ledgpio, 1);
		else
			gpio_direction_output(ts_drv->ledgpio, 0);
	}

	return  0;
}

static void led_timer_func(unsigned long data)
{
	set_key_led_gpio((struct vt1603_ts_drvdata *)data,LOW);
	return;
}

#endif


/*
 * vt1603_set_reg8 - set register value of vt1603
 * @ts_drv: vt1603 driver data
 * @reg: vt1603 register address
 * @val: value register will be set
 */
inline int vt1603_set_reg8(struct vt1603_ts_drvdata *ts_drv, u8 reg, u8 val)
{
	int ret =0;
	if (ts_drv->tdev)
        ret = ts_drv->tdev->reg_write(ts_drv->tdev,reg,val);

	if(ret)
		printk("vt1609 ts write error, errno%d\n", ret);
	
	return ret;
}

/*
 * vt1603_get_reg8 - get register value of vt1603
 * @ts_drv: vt1603 driver data
 * @reg: vt1603 register address
 */
inline u8 vt1603_get_reg8(struct vt1603_ts_drvdata *ts_drv, u8 reg)
{
	u8 val = 0;
	int ret = 0;

	if (ts_drv->tdev)	
        ret = ts_drv->tdev->reg_read(ts_drv->tdev,reg,&val);

	if (ret)
		printk("vt1609 ts read error, errno%d\n", ret);
	
	return val;
}


#ifdef VT1609_DEBUG
/*
 * vt1603_reg_dump - dubug function, for dump vt1603 related registers
 * @ts_drv: vt1603 driver data
 */
static void vt1603_reg_dump(struct vt1603_ts_drvdata *ts_drv)
{
    u8 i;    
    for (i = 0; i < 15; i++)
        dbg("reg[%d]:0x%02X,  reg[%d]:0x%02X\n", 
            i, vt1603_get_reg8(ts_drv, i), i + 0xC0, vt1603_get_reg8(ts_drv, i + 0xC0));
}
#endif

/*
 * vt1603_setbits - write bit1 to related register's bit
 * @ts_drv: vt1603 driver data
 * @reg: vt1603 register address
 * @mask: bit setting mask
 */
inline void vt1603_setbits(struct vt1603_ts_drvdata *ts_drv, u8 reg, u8 mask)
{
    u8 tmp = 0;
    tmp = vt1603_get_reg8(ts_drv, reg) | mask;
    vt1603_set_reg8(ts_drv, reg, tmp);

    return;
}


/*
 * vt1603_clrbits - write bit0 to related register's bit
 * @ts_drv: vt1603 driver data
 * @reg: vt1603 register address
 * @mask:bit setting mask
 */
inline void vt1603_clrbits(struct vt1603_ts_drvdata *ts_drv, u8 reg, u8 mask)
{
    u8 tmp = vt1603_get_reg8(ts_drv, reg) & (~mask);
    vt1603_set_reg8(ts_drv, reg, tmp);

    return;
}

/*
 * vt1603_clr_ts_irq -  clear touch panel pen down/up and 
 *    conversion end/timeout interrupts
 * @ts_drv: vt1603 driver data
 * @mask: which interrupt will be cleared
 */
inline int vt1603_clr_ts_irq(struct vt1603_ts_drvdata *ts_drv, u8 mask)
{
    vt1603_setbits(ts_drv, VT1603_INTS_REG, mask);
    return 0;
}


/*
 * Enable I2S CLK, wmt-i2s.c have done this.
 */
static void vt1603_ts_clk_enable(void)
{
#if 0
		/* set to 11.288MHz */
		auto_pll_divisor(DEV_I2S, CLK_ENABLE , 0, 0);
		auto_pll_divisor(DEV_I2S, SET_PLLDIV, 1, 11288);
		/*clock = auto_pll_divisor(DEV_I2S, GET_FREQ , 0, 0);
		info("%s : clock=%d \n" , __func__, clock);*/

		/* Enable BIT4:ARFP clock, BIT3:ARF clock */
		PMCEU_VAL |= (BIT4 | BIT3);

		/* Enable BIT2:AUD clock */
		PMCE3_VAL |= BIT2;

		/* disable GPIO and Pull Down mode */
		GPIO_CTRL_GP10_I2S_BYTE_VAL &= ~0xFF;
		GPIO_CTRL_GP27_BYTE_VAL &= ~(BIT0 | BIT1 | BIT2);

		GPIO_PULL_EN_GP10_I2S_BYTE_VAL &= ~0xFF;
		GPIO_PULL_EN_GP27_BYTE_VAL &= ~(BIT0 | BIT1 | BIT2);

		/* set to 2ch input, 2ch output */
		GPIO_PIN_SHARING_SEL_4BYTE_VAL &= ~(BIT3 | BIT17 | BIT19 | BIT20 | BIT22);
		GPIO_PIN_SHARING_SEL_4BYTE_VAL |= (BIT0 | BIT2 | BIT16 | BIT18 | BIT21);
#endif
    return;
}

/*
 * vt1603_setup_ts_mode - switch to VT1603 TS mode 
 * @ts_drv: vt1603 driver data
 */
static int vt1603_setup_ts_mode(struct vt1603_ts_drvdata *ts_drv)
{
    int ret = 0;
    struct vt1603_ts_platform_data *ts_pdata;

    ts_pdata = ts_drv->pdata;	
    ret |= vt1603_set_reg8(ts_drv, VT1603_CDPR_REG, ts_pdata->sclk_div);
    if (ts_pdata->panel_type == PANEL_TYPE_4WIRED)
        ret |= vt1603_set_reg8(ts_drv, VT1603_CR_REG, BIT1);
    else
        ret |= vt1603_set_reg8(ts_drv, VT1603_CR_REG, BIT1 | BIT0);

    vt1603_clr_ts_irq(ts_drv, 0x0f);

    return (ret < 0)? -1 : 0;
	
}


static int vt1603_fifo_push(struct vt1603_fifo *Fifo, int Data)
{
	Fifo->buf[Fifo->head] = Data;
	Fifo->head++;
	if(Fifo->head >= VT1603_FIFO_LEN){
		Fifo->head = 0;
		Fifo->full = 1;
	}
	
	return 0;
}

static int vt1603_fifo_avg(struct vt1603_fifo Fifo, int *Data)
{
	int i=0;
	int Sum=0,Max=0,Min=0;

	if(!Fifo.full && !Fifo.head)//FIFO is empty
		return 0;
	
	if(!Fifo.full ){
		for(i=0; i<Fifo.head; i++)
			Sum += Fifo.buf[i];

		*Data = Sum/Fifo.head;
		return 0;
	}
	
	Max = Fifo.buf[0];
	Min = Fifo.buf[0];
	for(i=0; i<VT1603_FIFO_LEN; i++){
		Sum += Fifo.buf[i];

		if(Max < Fifo.buf[i])
			Max = Fifo.buf[i];

		if(Min > Fifo.buf[i])
			Min = Fifo.buf[i];
	}
	Sum -= Max;
	Sum -= Min;
	*Data = Sum/(VT1603_FIFO_LEN-2);

	return 0;
	
}


inline int vt1603_ts_pos_calibration(struct vt1603_ts_drvdata *ts_drv,struct vt1603_ts_pos *to_cal)
{
	int x, y;

    x = (g_CalcParam.a1 * to_cal->x + g_CalcParam.b1 * to_cal->y +
         g_CalcParam.c1) / g_CalcParam.delta;
    y = (g_CalcParam.a2 * to_cal->x + g_CalcParam.b2 * to_cal->y +
         g_CalcParam.c2) / g_CalcParam.delta;

    /* pos check */
    if (x < 0)
        x = 0;
    if (y < 0)
        y = 0;
    if (x > ts_drv->resl_x)
        x = ts_drv->resl_x - 1;
    if (y > ts_drv->resl_y)
		y = ts_drv->resl_y - 1;

	if (ts_drv->lcd_exchg) {
		int tmp;
		tmp = x;
		x = y;
		y = ts_drv->resl_x - tmp;
	}

	to_cal->x = x;
    to_cal->y = y;
    
    return 0;
}

static inline void vt1603_ts_set_rawdata(struct vt1603_ts_drvdata *ts_drv,struct vt1603_ts_pos *pos)
{
    ts_drv->raw_x = pos->x;
    ts_drv->raw_y = pos->y;

    return;
}

inline void vt1603_ts_report_pos(struct vt1603_ts_drvdata *ts_drv, struct vt1603_ts_pos *pos)
{
    vt1603_ts_set_rawdata(ts_drv,pos);
    vt1603_ts_pos_calibration(ts_drv,pos);

    input_report_abs(ts_drv->input, ABS_MT_POSITION_X, pos->x);
    input_report_abs(ts_drv->input, ABS_MT_POSITION_Y, pos->y);
    input_mt_sync(ts_drv->input);
    input_sync(ts_drv->input);

    return;
}

#ifdef TOUCH_KEY
void vt1603_ts_report_key(struct vt1603_ts_drvdata *ts_drv)
{
    if(ts_drv->touch_key_used ){
        
        if(ts_drv->ledgpio >= 0 ) 
            mod_timer(&ts_drv->led_timer, jiffies+10*HZ);
        
        if(ts_drv->key_pressed && ts_drv->key_idx < _MAX_NUM ){
            input_report_key(ts_drv->input, key_codes[ts_drv->key_idx], 1);
            input_sync(ts_drv->input);
            input_report_key(ts_drv->input, key_codes[ts_drv->key_idx], 0); 
            input_sync(ts_drv->input);
            dbg("report as key event %d \n",ts_drv->key_idx);
        }
    }

    return;
}

inline int vt1603_ts_get_key(struct vt1603_ts_drvdata *ts_drv,struct vt1603_ts_pos pos)
{
    if(ts_drv->touch_key_used){
		
        if(pos.y > ts_drv->tsc_key.low && pos.y < ts_drv->tsc_key.upper){

            ts_drv->key_pressed = 1;
            if(pos.x>(ts_drv->tsc_key.key[_SEARCH].pos-ts_drv->tsc_key.delta) && 
                pos.x<(ts_drv->tsc_key.key[_SEARCH].pos+ts_drv->tsc_key.delta)){
                ts_drv->key_idx = ts_drv->tsc_key.key[_SEARCH].idx;
            }
            else if(pos.x>(ts_drv->tsc_key.key[_BACK].pos-ts_drv->tsc_key.delta) && 
                pos.x<(ts_drv->tsc_key.key[_BACK].pos+ts_drv->tsc_key.delta)){
                ts_drv->key_idx = ts_drv->tsc_key.key[_BACK].idx;
            }
            else if(pos.x>(ts_drv->tsc_key.key[_HOME].pos-ts_drv->tsc_key.delta) && 
                pos.x<(ts_drv->tsc_key.key[_HOME].pos+ts_drv->tsc_key.delta)){
                ts_drv->key_idx = ts_drv->tsc_key.key[_HOME].idx;
            }
            else if(pos.x>(ts_drv->tsc_key.key[_MENU].pos-ts_drv->tsc_key.delta) && 
                pos.x<(ts_drv->tsc_key.key[_MENU].pos+ts_drv->tsc_key.delta)){
                ts_drv->key_idx = ts_drv->tsc_key.key[_MENU].idx;
            }
            else{
                ts_drv->key_idx = _MAX_NUM;
            }
			
            if(ts_drv->key_idx < _MAX_NUM && ts_drv->ledgpio >= 0)
                 set_key_led_gpio(ts_drv,HIGH);
			 
            return 1;
        }

        if(ts_drv->ledgpio >= 0)
            set_key_led_gpio(ts_drv,HIGH);
    }
							
    ts_drv->key_pressed= 0;

    return 0 ;
}

#endif


/*
 * vt1603_ts_get_pos - get touch panel touched position from vt1603
 *     conversion register
 * @ts_drv: vt1603 driver data
 * @pos: vt1603 touch panel touched point conversion data
 */
static inline void vt1603_ts_get_pos(struct vt1603_ts_drvdata *ts_drv, struct vt1603_ts_pos *pos)
{
    u8 datal, datah;

    /* get x-position */
    datal = vt1603_get_reg8(ts_drv, VT1603_XPL_REG);
    datah = vt1603_get_reg8(ts_drv, VT1603_XPH_REG);
    pos->x = ADC_DATA(datal, datah);
	
    /* get y-positin */
    datal = vt1603_get_reg8(ts_drv, VT1603_YPL_REG);
    datah = vt1603_get_reg8(ts_drv, VT1603_YPH_REG);
    pos->y = ADC_DATA(datal, datah);
    vt1603_clr_ts_irq(ts_drv, BIT0);

    return;
}

/*
 * vt1603_ts_isPendown - get touch panel pen state from vt1603 
 *   interrup status register
 * @ts_drv: vt1603 driver data
 */
static inline int vt1603_ts_isPendown(struct vt1603_ts_drvdata *ts_drv)
{
    u8 state = vt1603_get_reg8(ts_drv, VT1603_INTS_REG);

    if (state & BIT4)
	    return TS_PENUP_STATE;
    else
    	return TS_PENDOWN_STATE;
}


static inline int vt1603_pos_avg(struct vt1603_ts_pos *pos)
{
	vt1603_fifo_push(&px, pos->x);	
	vt1603_fifo_push(&py, pos->y);
	vt1603_fifo_avg(px, &pos->x);	
	vt1603_fifo_avg(py, &pos->y);	

	return 0;
}

static int vt1603_pos_cleanup(void)
{
	px.full = 0;
	px.head = 0;
	
	py.full = 0;
	py.head = 0;

	return 0;
}

static void vt1603_read_loop(struct work_struct* dwork)
{
	struct vt1603_ts_drvdata *ts_drv=NULL;
	struct vt1603_ts_pos pos;
	
	ts_drv = container_of(dwork, struct vt1603_ts_drvdata, read_work.work);
	ts_drv->pen_state= vt1603_ts_isPendown(ts_drv);
	if((ts_drv->pen_state == TS_PENUP_STATE) ||ts_drv->earlysus){
		vt1603_clr_ts_irq(ts_drv, 0x0F);
		if(jiffies_to_msecs(jiffies - ts_drv->ts_stamp) < 80 && !ts_drv->earlysus){
			//dbg("Debounceing@@@@@@@@\n");
			goto next_loop;
		}
		
		dbg("============== penup ==============\n");
		vt1603_set_reg8(ts_drv, VT1603_ISEL_REG36, 0x04);/* vt1603 gpio1 as IRQ output */
		input_mt_sync(ts_drv->input);
		input_sync(ts_drv->input);
	#ifdef TOUCH_KEY
        vt1603_ts_report_key(ts_drv);
	#endif
		pre_pos.x = 0;
		pre_pos.y = 0;
		ts_drv->hcnt = 0;
		vt1603_pos_cleanup();

        if(!ts_drv->earlysus)
			wmt_gpio_unmask_irq(ts_drv->intgpio);
		
		return;
	}
	
	ts_drv->ts_stamp = jiffies;
	vt1603_ts_get_pos(ts_drv, &pos);

	//Filter the first N point
	if(ts_drv->hcnt < VT1603_FILTER_HEAD_COUNT){
		ts_drv->hcnt++;	
		goto next_loop;		
	}

	/* Æ½»¬ÂË²¨*/
	if((pre_pos.x != 0 && pre_pos.y != 0)&&(abs(pre_pos.x-pos.x) > VT1603_JITTER_THRESHOLD||abs(pre_pos.y-pos.y) > VT1603_JITTER_THRESHOLD)){
		pre_pos.x = pos.x;
		pre_pos.y = pos.y;
		goto next_loop;		
	}
#ifdef TOUCH_KEY				
	if(vt1603_ts_get_key(ts_drv,pos))
		goto next_loop;
#endif

	vt1603_pos_avg(&pos);
	pre_pos.x = pos.x;
	pre_pos.y = pos.y;
	
	dbg("x=%d, y=%d\n",pos.x,pos.y);
	vt1603_ts_report_pos(ts_drv, &pos);
	
next_loop:	
	queue_delayed_work(ts_drv->workqueue, &ts_drv->read_work, 20*HZ/1000);	
	
	return;
}


static void vt1603_ts_work(struct work_struct *work)
{
    int int_sts;
    //unsigned long flags;
    struct vt1603_ts_drvdata *ts_drv=pContext;

    //spin_lock_irqsave(&ts_drv->spinlock, flags);
    mutex_lock(&ts_drv->ts_mutex);
    int_sts = vt1603_get_reg8(ts_drv, VT1603_INTS_REG);
    dbg("+++++++ ts int status 0x%02x +++++++\n",int_sts);

    if ((int_sts & BIT4) == 0 || (int_sts & BIT1) == BIT1){
	  if(ts_drv->pen_state == TS_PENUP_STATE){
		vt1603_set_reg8(ts_drv, VT1603_ISEL_REG36, 0x03);/* vt1603 gpio1 as logic high output */
		ts_drv->pen_state = TS_PENDOWN_STATE;
		ts_drv->ts_stamp = jiffies;
		vt1603_setup_ts_mode(ts_drv);	
	 	dbg("============= pendown =============\n");
		vt1603_pos_cleanup();
		if(ts_drv->dual_enable)
            vt1603_ts_dual_support(&ts_drv->dual_work.work);
		else	
            vt1603_read_loop(&ts_drv->read_work.work);

		vt1603_clr_ts_irq(ts_drv, int_sts&0x0f);
        //spin_unlock_irqrestore(&ts_drv->spinlock, flags);
	mutex_unlock(&ts_drv->ts_mutex);

        return;
	  }
    }
	
    vt1603_clr_ts_irq(ts_drv, int_sts&0x0f);
    //spin_unlock_irqrestore(&ts_drv->spinlock, flags);
    mutex_unlock(&ts_drv->ts_mutex);
	wmt_gpio_unmask_irq(ts_drv->intgpio);

    return ;
}


static irqreturn_t vt1603_ts_isr(int irq, void *dev_id)
{
    struct vt1603_ts_drvdata *ts_drv = dev_id;

    if(!gpio_irqstatus(ts_drv->intgpio) ||
        !is_gpio_irqenable(ts_drv->intgpio))
		return IRQ_NONE;

	wmt_gpio_ack_irq(ts_drv->intgpio);
	wmt_gpio_mask_irq(ts_drv->intgpio);
    schedule_work(&ts_drv->work);
    dbg("@@@@@@@ touch irq @@@@@@@\n");

    return IRQ_HANDLED;
}

static int vt1603_register_input(struct vt1603_ts_drvdata * ts_drv)
{
	if(strcmp(ts_drv->dev_id,"VT1609"))
		ts_drv->input->name = "vt1603-touch";
	else
		ts_drv->input->name = "vt1609-touch";
	
	ts_drv->input->evbit[0] = BIT_MASK(EV_SYN) | BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
    ts_drv->input->propbit[0] = BIT_MASK(INPUT_PROP_DIRECT);
    
#ifdef TOUCH_KEY 	
	if(ts_drv->touch_key_used){
		int i;
		for (i = 0; i < TOUCH_KEY_NUM; i++)
			set_bit(key_codes[i], ts_drv->input->keybit);

		ts_drv->input->keycode = key_codes;
		ts_drv->input->keycodesize = sizeof(unsigned int);
		ts_drv->input->keycodemax = TOUCH_KEY_NUM;
	}
#endif	
	if (ts_drv->lcd_exchg) {
		input_set_abs_params(ts_drv->input, ABS_MT_POSITION_X, 0, ts_drv->resl_y, 0, 0);
		input_set_abs_params(ts_drv->input, ABS_MT_POSITION_Y, 0, ts_drv->resl_x, 0, 0);
	} else {
		input_set_abs_params(ts_drv->input, ABS_MT_POSITION_X, 0, ts_drv->resl_x, 0, 0);
		input_set_abs_params(ts_drv->input, ABS_MT_POSITION_Y, 0, ts_drv->resl_y, 0, 0);
	}
	
    input_register_device(ts_drv->input);
    return 0;
}


/*
 * vt1603_ts_calibration - vt1603 self calibration routine
 * @ts_drv: vt1603 driver data
 */
static void vt1603_ts_calibration(struct vt1603_ts_drvdata *ts_drv)
{
    unsigned char i, j, tmp;
    unsigned char cal[5][8] = {{0}};
    unsigned int cal_sum[8] = {0};
    struct vt1603_ts_platform_data *ts_pdata;

    dbg("Enter\n");
    ts_pdata = ts_drv->pdata;    
    for (j = 0; j < 5; j++) {
        tmp = BIT6 | BIT0 | (ts_pdata->cal_sel << 4);
        vt1603_set_reg8(ts_drv, VT1603_CCCR_REG, tmp);
        msleep(100);
        for (i = 0; i < 8; i++)
            cal[j][i] = vt1603_get_reg8(ts_drv, VT1603_ERR8_REG + i);
    }
    for (i = 0; i < 8; i++) {
        for (j = 0; j < 5; j++)
            cal_sum[i] += cal[j][i];
        tmp = (u8)cal_sum[i]/5;
        vt1603_set_reg8(ts_drv, VT1603_DBG8_REG + i, tmp);
    }

    dbg("Exit\n");
    return ;
}

/*
 * vt1603_ts_reset - reset vt1603, auto postition conversion mode,
 *     do self calibration if enable
 * @ts_drv: vt1603 driver data
 */
static void vt1603_ts_reset(struct vt1603_ts_drvdata * ts_drv)
{   
    struct vt1603_ts_platform_data *ts_pdata;
    ts_pdata = ts_drv->pdata;

    /* power control enable */
    vt1603_set_reg8(ts_drv, VT1603_PWC_REG, 0x18);
    /* begin calibrate if calibration enable */
    if ((ts_pdata != NULL) && (ts_pdata->cal_en == CALIBRATION_ENABLE)) {
        vt1603_ts_calibration(ts_drv);
    }
	
    /* clock divider */
    vt1603_set_reg8(ts_drv, VT1603_CDPR_REG, ts_pdata->sclk_div);  
    
    /* clean debug register,for some 2 layer PCB machine enter debug mode unexpected */
	vt1603_set_reg8(ts_drv, VT1603_DCR_REG, 0x00);
    
    vt1603_set_reg8(ts_drv, VT1603_INTEN_REG, BIT1);//Just Enable pendown IRQ

    /* auto position conversion mode and panel type config */
    if (ts_pdata->panel_type== PANEL_TYPE_4WIRED)
      	vt1603_set_reg8(ts_drv, VT1603_CR_REG, BIT1);
    else
        vt1603_set_reg8(ts_drv, VT1603_CR_REG, BIT1 | BIT0);
	
     /* interrupt control, pen up/down detection enable */
    vt1603_set_reg8(ts_drv, VT1603_INTCR_REG, 0xff);
	
    /* mask other module interrupts     */
    vt1603_set_reg8(ts_drv, VT1603_IMASK_REG27, 0xff);
    vt1603_set_reg8(ts_drv, VT1603_IMASK_REG28, 0xFF);
    vt1603_set_reg8(ts_drv, VT1603_IMASK_REG29, 0xFF);
    /* reset headphone detect irq */
    vt1603_set_reg8(ts_drv, VT1603_IMASK_REG27, 0xfd);

    if (ts_pdata->irq_type == HIGH_ACTIVE|| ts_pdata->irq_type == RISING_EDGE_ACTIVE)
        vt1603_clrbits(ts_drv, VT1603_IPOL_REG33, BIT5);
    else
        vt1603_setbits(ts_drv, VT1603_IPOL_REG33, BIT5);
	
    vt1603_set_reg8(ts_drv, VT1603_ISEL_REG36, 0x04);/* vt1603 gpio1 as IRQ output */
    /* clear irq */
    vt1603_clr_ts_irq(ts_drv, 0x0F);

    return;
}


static struct vt1603_ts_platform_data vt1603_ts_pdata = {
    .panel_type   	= PANEL_TYPE_4WIRED,
    .cal_en       	= CALIBRATION_DISABLE,
    .cal_sel      	= 0x00,
    .shift          = 0x00,
    .sclk_div     	= 0x08,
    .irq_type     	= LOW_ACTIVE,
};

struct delayed_work resume_work;
static void vt1603_resume_work(struct work_struct* dwork)
{
    struct vt1603_ts_drvdata *ts_drv = pContext;
    ts_drv->pen_state = TS_PENUP_STATE;

    /* must ensure mclk is available */
    vt1603_ts_clk_enable();
    /* vt1603 ts hardware resume     */
    vt1603_ts_reset(ts_drv);
    /* clear irq before enale gpio irq */
    vt1603_clr_ts_irq(ts_drv, 0x0f );

	gpio_direction_input(ts_drv->intgpio);
	wmt_gpio_set_irq_type(ts_drv->intgpio, IRQ_TYPE_EDGE_FALLING);
	wmt_gpio_unmask_irq(ts_drv->intgpio);
 #ifdef TOUCH_KEY
    if(ts_drv->touch_key_used && ts_drv->ledgpio >= 0)
        setup_led_gpio(ts_drv);
#endif
#ifdef VT1609_DEBUG
    /* 4. dump vt1603 to ensure setting ok  */
    vt1603_reg_dump(ts_drv);
#endif
}
static __devinit int 
vt1603_ts_probe(struct platform_device *pdev)
{
    int ret = 0;
    struct vt1603_ts_drvdata *ts_drv = pContext;
    struct vt1603_ts_platform_data *ts_pdata = NULL;

    ts_pdata = &vt1603_ts_pdata;	
	
    ts_drv->pdata       = ts_pdata;
    ts_drv->pen_state   = TS_PENUP_STATE;
    ts_drv->tdev        = dev_get_platdata(&pdev->dev);
	
    //spin_lock_init(&ts_drv->spinlock);
    mutex_init(&ts_drv->ts_mutex);

    dev_set_drvdata(&pdev->dev, ts_drv);
#ifdef CONFIG_HAS_EARLYSUSPEND
    ts_drv->earlysuspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
    ts_drv->earlysuspend.suspend = vt1603ts_early_suspend;
    ts_drv->earlysuspend.resume = vt1603ts_late_resume;
    register_early_suspend(&ts_drv->earlysuspend);
#endif
    /* 1. mclk enable */
    vt1603_ts_clk_enable();
    /* 2.vt1603 touch-panel and sar-adc module reset */
    vt1603_ts_reset(ts_drv);

#ifdef VT1609_DEBUG
    /* 4. dump vt1603 to ensure setting ok  */
    vt1603_reg_dump(ts_drv);
#endif
    /* initial battery if battery detection enable  */
    /* initial temperature if temperature detection enable */

    /* request iuput device  */
    ts_drv->input = input_allocate_device();
    if (!ts_drv->input) {
        printk("vt1603_ts: alloc input device failed");
        ret = -ENOMEM;
        goto release_driver_data;
    }
    vt1603_register_input(ts_drv);
	
    INIT_DELAYED_WORK(&ts_drv->read_work, vt1603_read_loop);
    INIT_DELAYED_WORK(&ts_drv->dual_work, vt1603_ts_dual_support);
    INIT_DELAYED_WORK(&resume_work, vt1603_resume_work);
    ts_drv->workqueue = create_singlethread_workqueue("vt160x-touch");
    if(!ts_drv->workqueue){
        printk("vt160x create singlethread work queue failed!\n");
        goto release_driver_data;
    }

    INIT_WORK(&ts_drv->work, vt1603_ts_work);

    if (request_irq(ts_drv->gpio_irq, vt1603_ts_isr, IRQF_SHARED, "vt160x-touch", ts_drv)) {
        printk("vt160x_ts: request IRQ %d failed\n", ts_drv->gpio_irq);
        ret = -ENODEV;
    }
    
	wmt_gpio_set_irq_type(ts_drv->intgpio, IRQ_TYPE_EDGE_FALLING);
	wmt_gpio_unmask_irq(ts_drv->intgpio);

#ifdef TOUCH_KEY
    if(ts_drv->touch_key_used && ts_drv->ledgpio >= 0)
        setup_led_gpio(ts_drv);
#endif

    dbg("%s Touch Screen Driver Installed!\n",ts_drv->dev_id);
    return ret;

release_driver_data:
    kfree(ts_drv);
    ts_drv = NULL;
    
    return ret;
}

static __devexit int 
vt1603_ts_remove(struct platform_device *pdev)
{
    struct vt1603_ts_drvdata *ts_drv;
    ts_drv = dev_get_drvdata(&pdev->dev);

    dbg("Enter\n");
	
	wmt_gpio_mask_irq(ts_drv->intgpio);
    if(ts_drv->dual_enable)
        vt1603_dual_exit(ts_drv);
	
    /* input unregister                 */
    input_unregister_device(ts_drv->input);
    cancel_work_sync(&ts_drv->work);
    cancel_delayed_work_sync(&ts_drv->dual_work);
    cancel_delayed_work_sync(&ts_drv->read_work);
    destroy_workqueue(ts_drv->workqueue);
    vt1603_ts_dev_cleanup(ts_drv);
#ifdef TOUCH_KEY
    if(ts_drv->touch_key_used && ts_drv->ledgpio >= 0)
        del_timer_sync(&ts_drv->led_timer);
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
    unregister_early_suspend(&ts_drv->earlysuspend);
#endif
    /* free vt1603 driver data          */
    dev_set_drvdata(&pdev->dev, NULL);
    mutex_destroy(&ts_drv->ts_mutex);
    free_irq(ts_drv->gpio_irq,ts_drv);
    kfree(ts_drv);   
    ts_drv = NULL;

    dbg("Exit\n");
    return 0;
}

#ifdef CONFIG_PM

#ifdef CONFIG_HAS_EARLYSUSPEND

static void 
vt1603ts_early_suspend(struct early_suspend *h)
{
    struct vt1603_ts_drvdata *ts_drv = pContext;

    dbg("Enter\n");

	wmt_gpio_mask_irq(ts_drv->intgpio);
#ifdef TOUCH_KEY
    if(ts_drv->touch_key_used && ts_drv->ledgpio >= 0){
        del_timer_sync(&ts_drv->led_timer);
        set_key_led_gpio(ts_drv,LOW);
    }
#endif

    ts_drv->earlysus = 1;
    dbg("Exit\n");
    return;
}

static void 
vt1603ts_late_resume(struct early_suspend *h)
{
    struct vt1603_ts_drvdata *ts_drv = pContext;

    dbg("Enter\n");
    ts_drv->pen_state = TS_PENUP_STATE;

    /* must ensure mclk is available */
    vt1603_ts_clk_enable();
    /* vt1603 ts hardware resume     */
    vt1603_ts_reset(ts_drv);
    /* clear irq before enale gpio irq */
    vt1603_clr_ts_irq(ts_drv, 0x0f );

    ts_drv->earlysus = 0;
	//wmt_gpio_set_irq_type(ts_drv->intgpio, IRQ_TYPE_EDGE_FALLING);
	wmt_gpio_unmask_irq(ts_drv->intgpio);
 #ifdef TOUCH_KEY
    if(ts_drv->touch_key_used && ts_drv->ledgpio >= 0)
        setup_led_gpio(ts_drv);
    dbg("Exit\n");
#endif

    return;
}

#endif

static int 
vt1603_ts_suspend(struct platform_device *pdev, pm_message_t message)
{
    struct vt1603_ts_drvdata *ts_drv = dev_get_drvdata(&pdev->dev);

    dbg("Enter\n");
#ifdef VT1609_DEBUG
    /* 4. dump vt1603 to ensure setting ok  */
    vt1603_reg_dump(ts_drv);
#endif

    ts_drv = dev_get_drvdata(&pdev->dev);

	wmt_gpio_mask_irq(ts_drv->intgpio);
#ifdef TOUCH_KEY
    if(ts_drv->touch_key_used && ts_drv->ledgpio >= 0){
        set_key_led_gpio(ts_drv,LOW);
        del_timer_sync(&ts_drv->led_timer);
    }
#endif

    dbg("Exit\n");
    return 0;
}

static int
vt1603_ts_resume(struct platform_device *pdev)
{
    //struct vt1603_ts_drvdata *ts_drv = dev_get_drvdata(&pdev->dev);

    dbg("Enter\n");
	//delay resume work because some resources were closed by audio driver.
    schedule_delayed_work(&resume_work, HZ); 
#if 0
    ts_drv->pen_state = TS_PENUP_STATE;

    /* must ensure mclk is available */
    vt1603_ts_clk_enable();
    /* vt1603 ts hardware resume     */
    vt1603_ts_reset(ts_drv);
    /* clear irq before enale gpio irq */
    vt1603_clr_ts_irq(ts_drv, 0x0f );

	gpio_direction_input(ts_drv->intgpio);
	wmt_gpio_set_irq_type(ts_drv->intgpio, IRQ_TYPE_EDGE_FALLING);
	wmt_gpio_unmask_irq(ts_drv->intgpio);
 #ifdef TOUCH_KEY
    if(ts_drv->touch_key_used && ts_drv->ledgpio >= 0)
        setup_led_gpio(ts_drv);
#endif
#ifdef VT1609_DEBUG
    /* 4. dump vt1603 to ensure setting ok  */
    vt1603_reg_dump(ts_drv);
#endif
#endif
    dbg("Exit\n");

    return 0;
}

#else
#define vt1603_ts_suspend NULL
#define vt1603_ts_resume  NULL
#endif


static struct platform_driver vt1603_driver = {
    .driver    = {
        		.name = VT1603_DRIVER_NAME,
        		.owner = THIS_MODULE,
    },
    .probe     	= vt1603_ts_probe,
    .remove    	= vt1603_ts_remove,
    .suspend   	= vt1603_ts_suspend,
    .resume   	= vt1603_ts_resume,
};

static int vt1603_ts_dev_open(struct inode *inode, struct file *filp)
{
    struct vt1603_ts_drvdata *ts_drv;

    dbg("Enter\n");

    ts_drv = container_of(inode->i_cdev, struct vt1603_ts_drvdata , cdev);
    if (ts_drv == NULL) {
        printk("can not get vt1603_ts driver data\n");
        return -ENODATA;
    }
    filp->private_data = ts_drv;

    dbg("Exit\n");
    return 0;
}

static int vt1603_ts_dev_close(struct inode *inode, struct file *filp)
{
    struct vt1603_ts_drvdata  *ts_drv;

    dbg("Enter\n");

    ts_drv = container_of(inode->i_cdev, struct vt1603_ts_drvdata , cdev);
    if (ts_drv == NULL) {
        printk("can not get vt1603_ts driver data\n");
        return -ENODATA;
    }

    dbg("Exit\n");
    return 0;
}

static long vt1603_ts_dev_ioctl(struct file *filp,unsigned int cmd, unsigned long arg)
{
    int ret = 0;
    int nBuff[7] = { 0 };
    char env_val[96] = { 0 };
    struct vt1603_ts_drvdata *ts_drv;

    dbg("Enter\n");
    /* check type and command number */
    if (_IOC_TYPE(cmd) != VT1603_TS_IOC_MAGIC)
        return -ENOTTY;

    ts_drv = filp->private_data;
    switch (cmd) {
	    case VT1603_TS_IOC_CAL_DONE:
	           copy_from_user(nBuff, (unsigned int *)arg, 7 * sizeof(int));
	           g_CalcParam.a1 = nBuff[0];
	           g_CalcParam.b1 = nBuff[1];
	           g_CalcParam.c1 = nBuff[2];
	           g_CalcParam.a2 = nBuff[3];
	           g_CalcParam.b2 = nBuff[4];
	           g_CalcParam.c2 = nBuff[5];
	           g_CalcParam.delta = nBuff[6];
	           
	           if (g_CalcParam.delta == 0)
	           		g_CalcParam.delta = 1;
				
		     sprintf(env_val, "%d %d %d %d %d %d %d",
	                		nBuff[0], nBuff[1], nBuff[2], nBuff[3], nBuff[4], nBuff[5], nBuff[6]);
			
		     wmt_setsyspara("wmt.io.ts.2dcal", env_val);
		     printk("TOUCH CAL DONE: [%s]\n", env_val);
		     break;

	    case VT1603_TS_IOC_CAL_RAWDATA:
			nBuff[0] = ts_drv->raw_x;
			nBuff[1] = ts_drv->raw_y;
			copy_to_user((unsigned int *)arg, nBuff, 2 * sizeof(int));
			printk("TOUCH CAL RAWDATA: x=%-4d, y=%-4d \n", nBuff[0], nBuff[1]);
	        break;
	    default:
			ret = -EINVAL;
	        break;
    }

    dbg("Exit\n");
    return ret;
}

static struct file_operations vt1603_ts_fops = {
    .owner   	= THIS_MODULE,
    .open   	= vt1603_ts_dev_open,
    .unlocked_ioctl = vt1603_ts_dev_ioctl,
    .release 	= vt1603_ts_dev_close,
};


static int vt1603_ts_dev_setup(struct vt1603_ts_drvdata *ts_drv)
{
    dev_t dev_no = 0;
    int ret = 0;
    struct device *dev = NULL;

    dbg("Enter\n");
    if (VT1603_TS_DEV_MAJOR) {
        ts_drv->major = VT1603_TS_DEV_MAJOR;
        ts_drv->minor = 0;
        dev_no = MKDEV(ts_drv->major, ts_drv->minor);
        ret = register_chrdev_region(dev_no, VT1603_TS_NR_DEVS, DEV_NAME);
    } else {
        ret = alloc_chrdev_region(&dev_no, 0, VT1603_TS_NR_DEVS, DEV_NAME);
        ts_drv->major = MAJOR(dev_no);
        ts_drv->minor = MINOR(dev_no);
        dbg("vt1603_ts device major = %d, minor = %d \n", ts_drv->major,ts_drv->minor);
    }

    if (ret < 0) {
        printk("can not get major %d\n", ts_drv->major);
        goto out;
    }

    cdev_init(&ts_drv->cdev, &vt1603_ts_fops);
   
    ts_drv->cdev.owner = THIS_MODULE;
    ts_drv->cdev.ops   = &vt1603_ts_fops;
    ret = cdev_add(&ts_drv->cdev, dev_no, VT1603_TS_NR_DEVS);
    if (ret) {
        printk("add char dev for vt1603 ts failed\n");
        goto release_region;
    }

    vt1603_ts_class = class_create(THIS_MODULE, ts_drv->dev_id);
    if (IS_ERR(vt1603_ts_class)) {
        printk("create vt1603_ts class failed\n");
        ret = PTR_ERR(vt1603_ts_class);
        goto release_cdev;
    }

    dev = device_create(vt1603_ts_class, NULL, dev_no, NULL, DEV_NAME);
    if (IS_ERR(dev)) {
        printk("create device for vt160x ts failed\n");
        ret = PTR_ERR(dev);
        goto release_class;
    }

    dbg("Exit\n");
    return ret;

release_class:
    class_destroy(vt1603_ts_class);
    vt1603_ts_class = NULL;
release_cdev:
    cdev_del(&ts_drv->cdev);
release_region:
    unregister_chrdev_region(dev_no, VT1603_TS_NR_DEVS);
out:
    return ret;
}

static void vt1603_ts_dev_cleanup(struct vt1603_ts_drvdata *ts_drv)
{
    dev_t dev_no = MKDEV(ts_drv->major, ts_drv->minor);

    dbg("Enter\n");
    cdev_del(&ts_drv->cdev);
    unregister_chrdev_region(dev_no, VT1603_TS_NR_DEVS);
    device_destroy(vt1603_ts_class, dev_no);
    class_destroy(vt1603_ts_class);
    dbg("Exit\n");
}

#ifdef TOUCH_KEY
static int parse_touch_key_env(struct vt1603_ts_drvdata *ts_drv)
{
	int i = 0;
	int ret = 0;
	int len = 96;
	char retval[96] = {0};
	char *p = NULL;
	
	ret = wmt_getsyspara("wmt.ts.vkey", retval, &len);
	if(ret){
		printk("Read wmt.ts.vkey Failed.\n");
		return -EIO;
	}
	
	sscanf(retval,"%d:%d:%d:%d", &ts_drv->tsc_key.key_num,
        &ts_drv->tsc_key.low, &ts_drv->tsc_key.upper, &ts_drv->tsc_key.delta);

	if(!ts_drv->tsc_key.key_num){
		printk("tsc key number is zero!\n");
		return -EIO;
	}
	
	p = retval;
	i = 4;
	while(i--){
		p = strchr(p,':');
		p++;
	}

	for(i = 0; i < ts_drv->tsc_key.key_num; i++){
		sscanf(p,"%d_%d",&ts_drv->tsc_key.key[i].pos,&ts_drv->tsc_key.key[i].idx );
		p = strchr(p,':');
		p++;
	}

	dbg("%d:%d:%d:%d:%d_%d:%d_%d:%d_%d:%d_%d\n",
			ts_drv->tsc_key.key_num,
			ts_drv->tsc_key.low,
			ts_drv->tsc_key.upper,
			ts_drv->tsc_key.delta,
			ts_drv->tsc_key.key[0].pos,
			ts_drv->tsc_key.key[0].idx,
			ts_drv->tsc_key.key[1].pos,
			ts_drv->tsc_key.key[1].idx,
			ts_drv->tsc_key.key[2].pos,
			ts_drv->tsc_key.key[2].idx,
			ts_drv->tsc_key.key[3].pos,
			ts_drv->tsc_key.key[3].idx);

	return 0;
	
}
#endif

static int parse_dual_env(struct vt1603_ts_drvdata *ts_drv)
{
	int ret = 0;
	int len = 96;
	char retval[96] = {0};

	len = sizeof(retval);
	ret = wmt_getsyspara("wmt.io.vt1609", retval, &len);	
	if(ret){
		//printk("wmt.io.vt1609 not set, use default parameter.\n");
		ts_drv->dual_dev.vxy = 17;
		ts_drv->dual_dev.scale_x = 4;
		ts_drv->dual_dev.scale_y = 2;
		ts_drv->dual_dev.F1_CNT = 2;
		ts_drv->dual_dev.F2_CNT = 7;
		ts_drv->dual_dev.F2T1_CNT = 15;
		ts_drv->dual_dev.SAMPLE_CNT = 1;
		ts_drv->dual_dev.THR_MIN_DX = 13;
		ts_drv->dual_dev.THR_MAX_DX = 256;
        ts_drv->dual_dev.exch = 0;
		
		return 0;
	}
	
	sscanf(retval,"%d:%d:%d:%d:%d:%d:%d:%d:%d:%d", &ts_drv->dual_dev.vxy, 
		&ts_drv->dual_dev.scale_x,
		&ts_drv->dual_dev.scale_y,
		&ts_drv->dual_dev.F1_CNT,
		&ts_drv->dual_dev.F2_CNT,
		&ts_drv->dual_dev.F2T1_CNT,
		&ts_drv->dual_dev.SAMPLE_CNT,
		&ts_drv->dual_dev.THR_MIN_DX,
		&ts_drv->dual_dev.THR_MAX_DX,
		&ts_drv->dual_dev.exch);
	/*
	printk("%d:%d:%d:%d:%d:%d:%d:%d:%d\n",
		ts_drv->dual_dev.vxy,
		ts_drv->dual_dev.scale_x,
		ts_drv->dual_dev.scale_y,
		ts_drv->dual_dev.F1_CNT,
		ts_drv->dual_dev.F2_CNT ,
		ts_drv->dual_dev.F2T1_CNT ,
		ts_drv->dual_dev.SAMPLE_CNT ,
		ts_drv->dual_dev.THR_MIN_DX ,
		ts_drv->dual_dev.THR_MAX_DX),
		ts_drv->dual_dev.exch;
	*/
	return 0;	
}

static int vt1603_uboot_env_check(struct vt1603_ts_drvdata *ts_drv)
{
    int nBuff[7] = {0};
	int i = 0, Enable = 0;
	int intgpio=0;
    int ledgpio=-1;
    int reslx=480,resly=800;
    int ret=0,len = 96;
    char retval[96] = {0};
    char *p=NULL;
	
    // Get u-boot parameter
	ret = wmt_getsyspara("wmt.io.touch", retval, &len);
	if(ret){
		printk("Read wmt.io.touch Failed.\n");
		return -EIO;
	}
	
	sscanf(retval,"%d:",&Enable);
	//check touch enable
	if(Enable == 0){
		printk("System touchscreen is disbaled.\n");
		return -ENODEV;
	}
	
	p = strchr(retval,':');
	p++;
	if(strncmp(p,"vt1603",6) == 0){//check touch ID
		strcpy(ts_drv->dev_id, "VT1603A");
	}
	else if(strncmp(p,"vt1609",6) == 0){//check touch ID
		ts_drv->dual_enable = 1;
		strcpy(ts_drv->dev_id, "VT1609");
		parse_dual_env(ts_drv);
	}
	else{
		printk("Vt1609 touchscreen driver disabled.\n");
		return -ENODEV;
	}
	
	p = strchr(p,':');
	p++;
	sscanf(p,"%d:%d:%d:%d",&reslx, &resly, &intgpio, &ledgpio);

	ts_drv->resl_x = reslx;
	ts_drv->resl_y = resly;
    
	ts_drv->intgpio = intgpio;

    ts_drv->ledgpio = ledgpio;
    
	ts_drv->gpio_irq = IRQ_GPIO;

    printk("%s-Touch: reslx=%d resly=%d, Interrupt GPIO%d , Virtual Touch Key Led GPIO%d\n",ts_drv->dev_id,
        ts_drv->resl_x, ts_drv->resl_y, ts_drv->intgpio, ts_drv->ledgpio);
	  
	len = sizeof(retval);
	memset(retval, 0, sizeof(retval));

	ret = wmt_getsyspara("wmt.io.ts.2dcal", retval, &len);
	if(ret){
		printk("Read env wmt.io.ts.2dcal Failed.\n");
		//return -EIO;
	}
	
	for (i = 0; i < sizeof(retval); i++) {
		if (retval[i] == ' ' || retval[i] == ',' || retval[i] == ':')
			retval[i] = '\0';
	}

	p = retval;
	for (i = 0; (i < 7) && (p < (retval + sizeof(retval))); ) {
		if (*p == '\0')
			p++;
		else {
			sscanf(p, "%d", &nBuff[i]);
			p = p + strlen(p);
			i++;
		}
	}
	dbg("Touchscreen Calibrate Data: [%d %d %d %d %d %d %d]\n",
          nBuff[0], nBuff[1], nBuff[2], nBuff[3], nBuff[4], nBuff[5], nBuff[6]);

	g_CalcParam.a1 = nBuff[0];
	g_CalcParam.b1 = nBuff[1];
	g_CalcParam.c1 = nBuff[2];
	g_CalcParam.a2 = nBuff[3];
	g_CalcParam.b2 = nBuff[4];
	g_CalcParam.c2 = nBuff[5];
	g_CalcParam.delta = nBuff[6];

	if(g_CalcParam.delta == 0)
		g_CalcParam.delta = 1;

	memset(retval,0,sizeof(retval));
	ret = wmt_getsyspara("wmt.display.fb0", retval, &len);
	if (!ret) {
		int tmp[6];
		p = retval;
		sscanf(p, "%d:[%d:%d:%d:%d:%d", &tmp[0], &tmp[1], &tmp[2], &tmp[3], &tmp[4], &tmp[5]);
		if (tmp[4] > tmp[5])
			ts_drv->lcd_exchg = 1;
	}

    return 0;
}

static int gpio_resource_request(void)
{
	if (gpio_request(pContext->intgpio, "ts_irq") < 0) {
		printk("gpio(%d) touchscreen interrupt request fail\n", pContext->intgpio);
		return -EIO;
	}
	gpio_direction_input(pContext->intgpio);

	if (pContext->ledgpio >= 0) {
		if (gpio_request(pContext->ledgpio, "ts_led") < 0) {
			printk("gpio(%d) touchscreen led gpio request fail\n", pContext->ledgpio);
			gpio_free(pContext->intgpio);
			return -EIO;
		}
		gpio_direction_output(pContext->ledgpio, 0);
	}

	return 0;
}
static void gpio_resource_free(void)
{
	gpio_free(pContext->intgpio);
	if (pContext->ledgpio >= 0)
		gpio_free(pContext->ledgpio);
}


static int __init vt1603_ts_init(void)
{
    int ret = 0;
    struct vt1603_ts_drvdata *ts_drv = NULL;

    dbg("Enter\n");	
    ts_drv = kzalloc(sizeof(struct vt1603_ts_drvdata), GFP_KERNEL);
    if (!ts_drv) {
        printk("vt160x ts: alloc driver data failed\n");
        return -ENOMEM;
    }
    pContext = ts_drv;
    ret = vt1603_uboot_env_check(ts_drv);
    if (ret) {//vt1603 touch disabled
        goto out;
    }else{//vt1603 touch enabled
		if (gpio_resource_request())
			goto out;
        ret = vt1603_ts_dev_setup(ts_drv);//only touch calibrate need dev node
        if (ret) {
            printk("##ERR## vt160x ts create device node failed.\n");
            goto freegpio;
        }
         
#ifdef TOUCH_KEY		  
    	if(!parse_touch_key_env(ts_drv)){
          ts_drv->touch_key_used = 1;
          if(ts_drv->ledgpio >= 0){//touch virtual key back light led enabled
            init_timer(&ts_drv->led_timer);
            ts_drv->led_timer.function = led_timer_func;
            ts_drv->led_timer.data = (unsigned long) ts_drv;
          }
    	}
#endif	 
    }

    ret = platform_driver_register(&vt1603_driver);
    if(ret){
    	printk("vt160x platform driver register failed!.\n");
    	goto release_dev;
    }

    if(ts_drv->dual_enable)
        vt1603_dual_init(ts_drv);

    dbg("Exit\n");
    return ret;

release_dev:
    vt1603_ts_dev_cleanup(ts_drv);
freegpio:
	gpio_resource_free();
out:
    kfree(ts_drv);
	
    return ret;
}
//module_init(vt1603_ts_init);
late_initcall(vt1603_ts_init);
static void __exit vt1603_ts_exit(void)
{
    dbg("Enter\n");

    platform_driver_unregister(&vt1603_driver);
	gpio_resource_free();

    dbg("Exit\n");
}
module_exit(vt1603_ts_exit);

MODULE_DESCRIPTION("VT1603A/VT1609 TouchScreen  Driver");
MODULE_LICENSE("GPL");

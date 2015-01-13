#include <linux/init.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
//#include <linux/spinlock.h>
#include <linux/input.h>
#include <linux/cdev.h>
#include <linux/time.h>
#include <linux/spi/spi.h>
#include <linux/platform_device.h>
#include <mach/hardware.h>
#include <linux/gpio.h>
#include <mach/wmt_iomux.h>
#include "vt1609_ts.h"


//#define _DEBUG_

#undef dbg
#ifdef  _DEBUG_
#define dbg(fmt, args...) printk(KERN_ERR "[%s][%d]: " fmt, __func__ , __LINE__, ## args)
#else
#define dbg(fmt, args...)
#endif

#undef dbg_err
#define dbg_err(fmt, args...) printk(KERN_ERR "[%s][%d]##ERROR##: " fmt, __func__ , __LINE__, ## args)

#define VT1609_DUAL_VERSION    "1.1"

#define DUAL_BUF_LENGTH       	17
#define SINGLE_BUF_LEN         	5	
#define MAX_SAMPLE_NUM        	5
#define POLL_TOUT               100
#define DX_DETLA      			4
#define DX_NUM  	         	5

#define VXY						17
#define SCALE_X					4
#define SCALE_Y					2
#define FILTER_COUNT_T1 		2
#define FILTER_COUNT_T2 		5
#define FILTER_COUNT_2T1 		5
#define SAMPLE_COUNT			4
#define THRESHOLD_DX 			256
#define THRESHOLD_XY 			1300
#define THRESHOLD_DUAL_CNT      1

#define CHL_X1 					0x01
#define CHL_X2 					0x02
#define CHL_X3 					0x03
#define CHL_Y1 					0x04
#define CHL_Y2 					0x05
#define CHL_Y3 					0x06

static int vxy_max_updated = 0;
static int vx_max = 0;
static int vy_max = 0;

static int dual_cnt = 0;
static int prv_dx = 0;
static int dx1 = 0, dx2 = 0;

static int TwoCT = 0;
static int FirstCT = 0;
static int TouchCT = 0 ;
static int OneTCAfter2 = 0;
static int TwoTouchFlag = 0;
static struct vt1603_ts_pos pre,fixpos; 

//extern struct vt1603_ts_drvdata *pContext;

struct dual_avg_buf {
    int num;
    u32 data[DUAL_BUF_LENGTH];
};
	
static struct dual_avg_buf x_buf;
static struct dual_avg_buf y_buf;
static struct dual_avg_buf avg_buf;

static int vt1603_ts_get_ux(int *para);
static int vt1603_ts_get_uy(int *para);
static int vt1603_ts_get_vx(int *para);
static int vt1603_ts_get_vy(int *para);

static inline void dual_buf_fill(struct dual_avg_buf *dual_buf, u32 data, int len)
{
    dual_buf->data[dual_buf->num % len] = data;
    dual_buf->num++;

    return ;
}

static inline u32 dual_buf_avg(struct dual_avg_buf *dual_buf, int len)
{
    int i, num;
    u32 avg = 0;
    int max, min;

    num = (dual_buf->num < len)? dual_buf->num : len;
	
    if(num == 1)
		return dual_buf->data[0];
    if(num == 2)
		return (dual_buf->data[0]+dual_buf->data[1])/2;

    max = dual_buf->data[0];
    min = dual_buf->data[0];
    for (i = 0; i < num; i++){
        avg += dual_buf->data[i];
		
	  if(dual_buf->data[i] > max)
	  	max = dual_buf->data[i];

	  if(dual_buf->data[i] < min)
	  	min = dual_buf->data[i];
    }

    return (avg-max-min )/ (num-2);
}

static void dual_buf_init(struct dual_avg_buf *dual_buf)
{
    memset(dual_buf, 0x00, sizeof(struct dual_avg_buf));
    return ;
}

static inline void vt1603_ts_report_dual_pos(struct vt1603_ts_drvdata *ts_drv, 
                                          struct vt1603_ts_pos *fst,struct vt1603_ts_pos *lst)
{
    struct vt1603_ts_pos p1 = *fst;
    struct vt1603_ts_pos p2 = *lst;

    vt1603_ts_pos_calibration(ts_drv,&p1);
    vt1603_ts_pos_calibration(ts_drv,&p2);
    //dbg("Caled pos1 (%d, %d), pos2 (%d, %d)\n", p1.x, p1.y, p2.x, p2.y);

    input_report_abs(ts_drv->input, ABS_MT_POSITION_X, p1.x);
    input_report_abs(ts_drv->input, ABS_MT_POSITION_Y, p1.y);
    input_mt_sync(ts_drv->input);
    
    input_report_abs(ts_drv->input, ABS_MT_POSITION_X, p2.x);
    input_report_abs(ts_drv->input, ABS_MT_POSITION_Y, p2.y);
    input_mt_sync(ts_drv->input);
    
    input_sync(ts_drv->input);

    return;
}

static inline void vt1603_ts_auto_mode(struct vt1603_ts_drvdata *ts_drv)
{
    vt1603_set_reg8(ts_drv, VT1603_PWC_REG, 0x08);
    /* auto position conversion mode and panel type config */
    vt1603_set_reg8(ts_drv, VT1603_CR_REG, BIT1);
    /* disable pen up/down detection, it is a MUST opearetion */
    vt1603_set_reg8(ts_drv, 0xC8, 0x7F);

    return;
}

static inline int select_channel(struct vt1603_ts_drvdata *ts_drv,int chl)
{
	switch(chl){
		case CHL_X1://select x1
			vt1603_set_reg8(ts_drv, 0xc6, 0x12);
			vt1603_set_reg8(ts_drv, 0xc7, 0x05);
			break;
		case CHL_X2://select x2
			vt1603_set_reg8(ts_drv, 0xc6, 0x12);
			vt1603_set_reg8(ts_drv, 0xc7, 0x04);
			break;
		case CHL_X3://select x3
			vt1603_set_reg8(ts_drv, 0xc6, 0x12);
			vt1603_set_reg8(ts_drv, 0xc7, 0x00);
			break;
		case CHL_Y1://select y1
			vt1603_set_reg8(ts_drv, 0xc6, 0x21);
			vt1603_set_reg8(ts_drv, 0xc7, 0x07);
			break;
		case CHL_Y2://select y2
			vt1603_set_reg8(ts_drv, 0xc6, 0x21);
			vt1603_set_reg8(ts_drv, 0xc7, 0x06);
			break;
		case CHL_Y3://select y3
			vt1603_set_reg8(ts_drv, 0xc6, 0x21);
			vt1603_set_reg8(ts_drv, 0xc7, 0x03);
			break;
		default://default select x1
			vt1603_set_reg8(ts_drv, 0xc6, 0x12);
			vt1603_set_reg8(ts_drv, 0xc7, 0x05);
			break;
	}

	return 0;
}

static inline int select_x_channel(struct vt1603_ts_drvdata *ts_drv,int chl)
{
	switch(chl){
		case CHL_X1://select x1
			vt1603_set_reg8(ts_drv, 0xc6, 0x21);
			vt1603_set_reg8(ts_drv, 0xc7, 0x07);
			break;
		case CHL_X2://select x2
			vt1603_set_reg8(ts_drv, 0xc6, 0x21);
			vt1603_set_reg8(ts_drv, 0xc7, 0x06);
			break;
		case CHL_X3://select x3
		    vt1603_set_reg8(ts_drv, 0xc6, 0x21);
			vt1603_set_reg8(ts_drv, 0xc7, 0x03);
			break;
		case CHL_Y1://select y1
		    vt1603_set_reg8(ts_drv, 0xc6, 0x12);
			vt1603_set_reg8(ts_drv, 0xc7, 0x05);
			
			break;
		case CHL_Y2://select y2
		    vt1603_set_reg8(ts_drv, 0xc6, 0x12);
			vt1603_set_reg8(ts_drv, 0xc7, 0x04);			
			break;
		case CHL_Y3://select y3
			vt1603_set_reg8(ts_drv, 0xc6, 0x12);
			vt1603_set_reg8(ts_drv, 0xc7, 0x00);
			break;
		default://default select x1
			vt1603_set_reg8(ts_drv, 0xc6, 0x12);
			vt1603_set_reg8(ts_drv, 0xc7, 0x05);
			break;
	}

	return 0;
}

static inline int get_channel_data(struct vt1603_ts_drvdata *ts_drv,int chl)
{
    int i = 0;
    int sum = 0;
    int buf[MAX_SAMPLE_NUM] = {0};
    u32 now = 0;
    u8 tmp = 0;

    if(ts_drv->dual_dev.exch)
        select_x_channel(ts_drv,chl);
    else
        select_channel(ts_drv,chl);
    
    for (i = 0; i < ts_drv->dual_dev.SAMPLE_CNT; i++) {
        vt1603_clrbits(ts_drv, VT1603_INTS_REG, 0x0f);
        vt1603_set_reg8(ts_drv, VT1603_CR_REG, 0x10);
        udelay(100);
        now = jiffies;
        while (time_before(jiffies, now + msecs_to_jiffies(POLL_TOUT))) {
            tmp = vt1603_get_reg8(ts_drv, VT1603_INTS_REG);
            if (tmp & BIT0) {
                buf[i] = vt1603_get_reg8(ts_drv, 0xce);
                tmp = (vt1603_get_reg8(ts_drv, 0xcf) & 0x0f);
                buf[i] |= (tmp << 8);
                sum += buf[i];
                goto next;
            }
        }
        printk("VT1609 %s timeout!\n", __func__);
        return 0;

    next:
        ;//printk("CHL %d buf[%d] is %d\n", chl, i, buf[i]);
    }
	
    return sum/ts_drv->dual_dev.SAMPLE_CNT;

}


static inline int vt1603_get_paramters(struct vt1603_ts_drvdata *ts_drv, int *para)
{
    /* change to manual mode now */
    vt1603_set_reg8(ts_drv, VT1603_CR_REG, 0x00);
    /* set prechare to 0x10 */
    vt1603_set_reg8(ts_drv, VT1603_TSPC_REG, 0x10);
    /* get parameters now */
    para[0] = get_channel_data(ts_drv, CHL_X1);
    para[1] = get_channel_data(ts_drv, CHL_X2);
    para[2] = get_channel_data(ts_drv, CHL_Y1);
    para[3] = get_channel_data(ts_drv, CHL_Y2);

    para[4] = get_channel_data(ts_drv, CHL_X3);
    para[5] = get_channel_data(ts_drv, CHL_Y3);

    /* reset adc, this is a MUST operation */
    vt1603_set_reg8(ts_drv, 0xc8, 0x8f);
    vt1603_set_reg8(ts_drv, 0xc0, BIT6 | BIT5 | BIT0);
	
    return 0;
}

static int vt1603_get_vxy(struct vt1603_ts_drvdata *ts_drv, int *vx, int *vy)
{
    int i;
    int xbuf[5] ={0}, ybuf[5] ={0};
    int sum_vx = 0,sum_vy = 0;
    int max_vx = 0,min_vx = 0;
    int max_vy = 0,min_vy = 0;
	
    /* change to manual mode now */
    vt1603_set_reg8(ts_drv, VT1603_CR_REG, 0x00);
    /* set prechare to 0x10 */
    vt1603_set_reg8(ts_drv, VT1603_TSPC_REG, 0x10);
    for(i=0; i<5; i++){
        xbuf[i] = get_channel_data(ts_drv,CHL_X3);
        ybuf[i] = get_channel_data(ts_drv,CHL_Y3);
        sum_vx += xbuf[i];
        sum_vy += ybuf[i];   
    }
	
    max_vx = min_vx = xbuf[0];
    max_vy = min_vy = ybuf[0];

    for(i=0; i<5; i++){
        if(xbuf[i] > max_vx)
            max_vx = xbuf[i];
			
        if(xbuf[i] < min_vx)
            min_vx = xbuf[i];
			
        if(ybuf[i] > max_vy)
            max_vy = ybuf[i];
			
        if(ybuf[i] < min_vy)
            min_vy = ybuf[i];
    }
    *vx = (sum_vx - max_vx - min_vx)/3;
    *vy = (sum_vy - max_vy - min_vy)/3;
    dbg("updated vx_max=%d; vy_max=%d\n",*vx, *vy);
    /* reset adc, this is a MUST operation */
    vt1603_set_reg8(ts_drv, 0xc8, 0x8f);
    vt1603_set_reg8(ts_drv, 0xc0, BIT6 | BIT5 | BIT0);
	
    return 0;
}

static inline int vt1603_ts_get_ux(int *para)
{
    return abs(para[1] - para[0]);
}

static inline int vt1603_ts_get_uy(int *para)
{
    return abs(para[3] - para[2]);
}

static inline int vt1603_ts_get_vx(int *para)
{
    return abs(vx_max - para[4]);
}

static inline int vt1603_ts_get_vy(int *para)
{
    return abs(vy_max - para[5]);
}

static inline int vt1603_ts_nTouch(struct vt1603_ts_drvdata *ts_drv, int *para)
{
     int  ux, uy, vx, vy;

    ux = vt1603_ts_get_ux(para);
    uy = vt1603_ts_get_uy(para);
    vx = vt1603_ts_get_vx(para);
    vy = vt1603_ts_get_vy(para);
    //printk("ux:%-3d, uy:%-3d, vx:%-3d, vy:%-3d\n",  ux, uy, vx, vy);

    if ((vx <= 5) && (vy <= 5)){
        dual_cnt = 0;	
        return Single_TOUCH;
    }else if ((vx >= ts_drv->dual_dev.vxy) || (vy >= ts_drv->dual_dev.vxy)){
        dual_cnt++;
        //printk("ux:%-3d, uy:%-3d, vx:%-3d, vy:%-3d\n",  ux, uy, vx, vy);
        return (dual_cnt > THRESHOLD_DUAL_CNT)? Multi_TOUCH : Single_TOUCH;
    }else if (((vx > 5) || (vy > 5)) && ((ux >= 2 * ts_drv->dual_dev.vxy) || (uy >= 2 * ts_drv->dual_dev.vxy))){
        dual_cnt++;
        //printk("ux:%-3d, uy:%-3d, vx:%-3d, vy:%-3d\n",  ux, uy, vx, vy);
        return (dual_cnt > THRESHOLD_DUAL_CNT)? Multi_TOUCH : Single_TOUCH;
    }else{
        dual_cnt = 0;
        return Single_TOUCH;
    }
    
}

static int pos_fix(const int limit, int p)
{
    if (p > limit) p = limit;
    if (p < 0) p = 0;

    return (u16)(p & 0xffff);
}

static int vt1603_ts_update_vxy(struct vt1603_ts_drvdata *ts_drv, int *vx, int *vy)
{
    u8 val;
    int timeout = 100;

    val = vt1603_get_reg8(ts_drv, VT1603_CR_REG);
    while (timeout-- &&  val != 0x02) {
        msleep(20);
        val = vt1603_get_reg8(ts_drv, VT1603_CR_REG);
    }

    if(!timeout){
        dbg_err("get vx_max/vy_max failed!\n");
        goto out;
    }

    vt1603_get_vxy(ts_drv, vx,vy);
    dbg("update vx_max:%d, vy_max:%d\n", vx_max, vy_max);

out:
    vt1603_set_reg8(ts_drv, VT1603_INTS_REG, 0x0F);
	
    return 0;
}

void vt1603_ts_dual_support(struct work_struct* dwork)
{
    int nTouch = 0;
    int para[6] = { 0 };
    //unsigned long flags = 0;
    int vx = 0, vy = 0, dx = 0;
    struct vt1603_ts_pos p,pos1, pos2;
    struct vt1603_ts_drvdata *ts_drv = NULL;
    u8 int_sts = 0;

    ts_drv = container_of(dwork, struct vt1603_ts_drvdata, dual_work.work);

    //spin_lock_irqsave(&ts_drv->spinlock, flags);
    mutex_lock(&ts_drv->ts_mutex);

    int_sts = vt1603_get_reg8(ts_drv, VT1603_INTS_REG);
    if (int_sts & BIT4 || ts_drv->earlysus) {
        if (jiffies_to_msecs(jiffies - ts_drv->ts_stamp) < TS_DEBOUNCE && !ts_drv->earlysus) {
            dbg("vt1603 ts debouncing?...\n");
            //vt1603_clr_ts_irq(ts_drv, int_sts & 0x0F);
            goto next_loop;
        }
        dbg("======= penup ======\n");

        /* update vx_max/vy_max only when first penup */
        if(!vxy_max_updated){
            vxy_max_updated ++;
        	vt1603_ts_update_vxy(ts_drv, &vx_max, &vy_max);
        }
        vt1603_ts_auto_mode(ts_drv);
        /* vt1603 gpio1 as IRQ output */
	    vt1603_set_reg8(ts_drv, VT1603_ISEL_REG36, 0x04);	
        input_mt_sync(ts_drv->input);
        input_sync(ts_drv->input);
        ts_drv->pen_state = TS_PENUP_STATE;		
    #ifdef TOUCH_KEY
        vt1603_ts_report_key(ts_drv);
    #endif
        dual_buf_init(&avg_buf);
        dual_buf_init(&x_buf);
        dual_buf_init(&y_buf);

        dx2 = 0;
        dx1 = 0;		
        pre.x = 0;
        pre.y = 0;
        FirstCT = 0;
        TwoCT = 0;	  
        TouchCT = None_TOUCH;
        OneTCAfter2 = 0;
        TwoTouchFlag = 0;
        vt1603_clr_ts_irq(ts_drv, int_sts & 0x0F);

        if(!ts_drv->earlysus) 
			wmt_gpio_unmask_irq(ts_drv->intgpio);

        //spin_unlock_irqrestore(&ts_drv->spinlock, flags);
	mutex_unlock(&ts_drv->ts_mutex);

        return;
    }
	
    ts_drv->ts_stamp = jiffies; 	
    ts_drv->pen_state = TS_PENDOWN_STATE;
    vt1603_get_paramters(ts_drv, para);
    vt1603_ts_auto_mode(ts_drv);
    //vt1603_clr_ts_irq(ts_drv, 0x0F & int_sts);

    nTouch = vt1603_ts_nTouch(ts_drv, para);	
    if(nTouch == Single_TOUCH){
		p.x = (para[0] + para[1]) / 2;
      	p.y = (para[2] + para[3]) / 2;

		if(TwoTouchFlag ==0 && FirstCT < ts_drv->dual_dev.F1_CNT){
			FirstCT ++;
			dbg("Filter First %d Single Touch\n",FirstCT);
			goto next_loop;
			
		}else if(TwoTouchFlag == 1 && OneTCAfter2 < ts_drv->dual_dev.F2T1_CNT){
			dbg("Filter First %d pointer when back to single touch from dual touch\n",OneTCAfter2);
			dx1 = 0;
			dx2 = 0;
			TwoCT = 0;
			OneTCAfter2 ++;
			dual_buf_init(&x_buf);
			dual_buf_init(&y_buf);
			dual_buf_init(&avg_buf);
			goto next_loop;

		}else if(p.x > vx_max || p.y > vy_max){
			dbg("Pos (%d,%d) beyond vx_max or vy_max\n",p.x,p.y);
			goto next_loop;
			
		}else if((pre.x!=0 && pre.y!=0) && (abs(pre.x-p.x) > THRESHOLD_XY||abs(pre.y-p.y) > THRESHOLD_XY )){
			dbg("Threhold Filter Pos (%-4d,%-4d) ,dx=%-4d,dy=%-4d\n",p.x,p.y,abs(pre.x-p.x),abs(pre.y-p.y));
			pre.x = p.x;
			pre.y = p.y;
			goto next_loop;
			
		}else{
			dual_buf_fill(&x_buf, p.x, SINGLE_BUF_LEN);
			dual_buf_fill(&y_buf, p.y, SINGLE_BUF_LEN);
			p.x = dual_buf_avg(&x_buf, SINGLE_BUF_LEN);
			p.y = dual_buf_avg(&y_buf, SINGLE_BUF_LEN);
			dbg("Report PHY Pos (%-4d,%-4d)\n",p.x,p.y);
			pre.x = p.x;
			pre.y = p.y;
		#ifdef TOUCH_KEY				
			if(vt1603_ts_get_key(ts_drv, p))
				goto next_loop;
		#endif
		
			vt1603_ts_report_pos(ts_drv, &p);
			
			TwoCT = 0;
			TouchCT = Single_TOUCH;
			OneTCAfter2 = 0;
			TwoTouchFlag = 0;
			goto next_loop;
		}
    }
    else if(nTouch == Multi_TOUCH){		
		vx = vt1603_ts_get_vx(para);
		vy = vt1603_ts_get_vy(para);
		dx = ts_drv->dual_dev.scale_y * vy + ts_drv->dual_dev.scale_x * vx;
		
		if(dx1 && dx2)
			dx = (dx+dx1+dx2)/3;	
		dx2 = dx1;
		dx1 = dx;
		dbg("vx=%-3d, vy=%-3d, dx=%-d, Ddx=%-3d\n",vx,vy,dx,abs(prv_dx-dx));
        
    	if(TwoCT < ts_drv->dual_dev.F2_CNT){
			TwoCT ++;
			dual_buf_init(&avg_buf);
			dbg("Filter The First %d Dual Touch\n",TwoCT);
			goto next_loop;
			
		}else if (prv_dx!=0 && (abs(prv_dx - dx) > ts_drv->dual_dev.THR_MAX_DX)){
			dbg("Threhold Filter Dual Touch dx=%d\n",abs(prv_dx - dx) );
			prv_dx = dx;
			goto next_loop;

		}else{
			//process and report dual touch data
			dual_buf_fill(&avg_buf, dx, DUAL_BUF_LENGTH);
			dx = dual_buf_avg(&avg_buf, DUAL_BUF_LENGTH);
			
			if(abs(prv_dx - dx) < ts_drv->dual_dev.THR_MIN_DX){
				//printk("Replace with last dx Ddx=%d\n",abs(prv_dx - dx));
				dx = prv_dx;
			}

			if(TwoTouchFlag==0 && TouchCT==Single_TOUCH){//Single Touch ->Multi Touch 
				fixpos = pre;
				//printk("Touch(%-4d,%-4d) 1--->2\n",pre.x,pre.y);
			}else if(TwoTouchFlag==0 && TouchCT==None_TOUCH){//Multi Touch from the beginning
				fixpos.x = vx_max/2;
				fixpos.y = vy_max/2;
				//printk("Touch(%-4d,%-4d) 2--->2\n",pos1.x, pos1.y);
			}

			pos1 = fixpos;
			pos2.x = fixpos.x;
			if(fixpos.y > vy_max/2)
				pos2.y = pos_fix(vy_max, (fixpos.y - 150 - dx*DX_DETLA/DX_NUM));
			else
				pos2.y = pos_fix(vy_max, (fixpos.y + 150 + dx*DX_DETLA/DX_NUM));

			dbg("PHY dx=%d, pos1.y=%d, pos2.y=%d\n", dx, pos1.y, pos2.y);
			vt1603_ts_report_dual_pos(ts_drv, &pos1, &pos2);

			prv_dx = dx;
			TouchCT = Multi_TOUCH;
			TwoTouchFlag = 1;
			OneTCAfter2 = 0;
		#ifdef TOUCH_KEY
			if(ts_drv->touch_key_used && ts_drv->ledgpio >= 0)
				set_key_led_gpio(ts_drv,HIGH);
		#endif
			goto next_loop;	
		}
    }
    else{
        dbg_err("Main Loop Error!\n");
    }

next_loop:
    queue_delayed_work(ts_drv->workqueue, &ts_drv->dual_work, msecs_to_jiffies(20));
    vt1603_clr_ts_irq(ts_drv, 0x0f);
    //spin_unlock_irqrestore(&ts_drv->spinlock, flags);
    mutex_unlock(&ts_drv->ts_mutex);
    
    return ;
}

int vt1603_dual_init(struct vt1603_ts_drvdata *ts_drv)
{
    int ret = 0;
    //unsigned long flags = 0;
    int retries = 20;
    u8 val = 0;

    if (ts_drv == NULL) {
        printk(KERN_ERR "VT1609 TouchScreen Driver Does Not Exsit!\n");
        ret = -1;
        goto out;
    }

    //spin_lock_irqsave(&ts_drv->spinlock, flags);
    mutex_lock(&ts_drv->ts_mutex);

    while (retries--) {
        val = vt1603_get_reg8(ts_drv, VT1603_INTS_REG);
        if ((val & BIT4) == 0) {
            printk(KERN_ERR "Do not keep in touching, when vt1609 driver to be installed!\n");
            msleep(20);
            continue ;
        }

        val = vt1603_get_reg8(ts_drv, VT1603_CR_REG);
        if ( val != 0x02) {
            printk(KERN_ERR "VT1609 is not working in TS mode now!reg: C1=0x%02x\n",val);
            msleep(10);
            continue;
        }

        break ;
    }

    if (retries == 0) {
        printk(KERN_ERR "Enable VT1609 Dual Touch Support Failed!\n");
        ret = -1;
        goto out;
    }

    
    vt1603_set_reg8(ts_drv, VT1603_CDPR_REG, 0x04);
    vt1603_set_reg8(ts_drv, VT1603_TSPC_REG, 0x10);
    vt1603_get_vxy(ts_drv, &vx_max, &vy_max);
    vt1603_ts_auto_mode(ts_drv);
    vt1603_clr_ts_irq(ts_drv, BIT0 | BIT2 | BIT3);

    dual_buf_init(&avg_buf);
    dual_buf_init(&x_buf);
    dual_buf_init(&y_buf);
	
    printk("VT1609 Dual Touch vx_max=%d,vy_max=%d, vxy=%d ver=%s\n",vx_max, vy_max,ts_drv->dual_dev.vxy,VT1609_DUAL_VERSION);
out:
    vt1603_clr_ts_irq(ts_drv, 0x0f);
    //spin_unlock_irqrestore(&ts_drv->spinlock, flags);    
    mutex_unlock(&ts_drv->ts_mutex);
	
    return ret;
}

void vt1603_dual_exit(struct vt1603_ts_drvdata *ts_drv)
{
    vt1603_set_reg8(ts_drv, VT1603_CDPR_REG, ts_drv->pdata->sclk_div);
    vt1603_set_reg8(ts_drv, VT1603_TSPC_REG, 0x20);
    printk("VT1609 Dual Touch Support Disabled.\n");
	
    return ;
}



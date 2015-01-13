#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
//#include <linux/earlysuspend.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/cdev.h>
#include <asm/uaccess.h>
#include <linux/pm_runtime.h>

#if defined(CONFIG_HAS_EARLYSUSPEND)
#include <linux/earlysuspend.h>
#endif
#include <linux/input/mt.h>

#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <mach/hardware.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/suspend.h>
#include <linux/async.h>
#include <linux/wait.h>
#include <linux/hrtimer.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/irq.h>
#include <linux/kthread.h>
#include <linux/firmware.h>
#include <linux/power/wmt_battery.h>
#include "../../../video/backlight/wmt_bl.h"
#include "lw86x0_ts.h"
#include "wmt_ts.h"
//#include "wmt_custom_lw86x0.h"

#define TIME_CHECK_CHARGE 3000

#define MAX_MULTI_DATA_SIZE 256

#define HDMI_BASE_ADDR	(HDMI_TRANSMITTE_BASE_ADDR + 0xC000)
#define REG_HDMI_HOTPLUG_DETECT		(HDMI_BASE_ADDR + 0x3ec)

struct i2c_client *lw_i2c_client = NULL;
struct i2c_client *client;//add by jackie
extern char g_dbgmode;
extern int COL_NUM;
extern int ROW_NUM;
extern int SKIP_ZERO_POINT;

struct wmtts_device lw86x0_tsdev;
static int tsirq_gpio;

static int skip_zero_num = 0;

u16 mcu_status_old = 0xffff;
u16 mcu_status = 0xffff;

typedef struct Fw_Version{   //add by jackie
    u8 magic_num1;
    u8 magic_num2;
    u8 mj_ver;
    u8 mn_ver;
}Fw_Ver;//add by jackie

//struct for report touch info
struct ts_event {
    u16	x[SUPPORT_POINT_NUM_MAX];//point x
    u16	y[SUPPORT_POINT_NUM_MAX];//point y
    u16	pressure[SUPPORT_POINT_NUM_MAX];//point pressure
    u8  touch_point;//touch point number
};

struct lw86x0_ts_data {
	struct input_dev	*input_dev;
	struct ts_event		event;
	struct work_struct 	touch_event_work;
	struct workqueue_struct *ts_workqueue;
#if defined(CONFIG_HAS_EARLYSUSPEND)
	struct early_suspend early_suspend;
#endif
}l_tsdata;

static struct mutex ts_data_mutex;
static int l_powermode = -1;
static int l_hdmimode = -1;

static int l_keylen = 4;
static int l_baseaxis = 1; //0:x-axis,1:y-axis
int l_keypos[TS_KEY_NUM+1][2];

unsigned int l_tskey[TS_KEY_NUM][2] = {
    {0,KEY_MENU},
    {0,KEY_HOME},
    {0,KEY_BACK},
    {0,KEY_SEARCH},
};
static int l_early_suspend = 0; // 1:the early suspend function has been excuted

static int stop_timer = 0;
struct work_struct   phone_status_work;
struct timer_list    polling_phone_status_timer;
static int check_chip_status(void);
static int first_init_reg = 1;
static u16 auto_coff_value[20] = {0};
//static finger_up_status = 1;

u8 get_fw_file_check_sum(void);
u16 get_fw_check_sum(void);

extern int register_bl_notifier(struct notifier_block *nb);

extern int unregister_bl_notifier(struct notifier_block *nb);
//static struct ts_event old_event;

void swap_byte_in_buffer(u16* buf, int count )
{
    int i;
    for(i = 0; i < count; i++ )
    {
        buf[i] = swap16(buf[i]);
    }
}

/**
** for read register
** rxbuf:read value
** txdata:read register address
** rxlength:read value length
**/

static int lw86x0_i2c_rxdata(char *rxbuf, char*txdata, int rxlength)
{
    int ret;
    //int reg;//add jackie

    struct i2c_msg msgs[] = {
        {
	    .addr	= lw_i2c_client->addr,
	    .flags	= 0,
	    .len	= 2,
	    .buf	= txdata,
	},
	{
	    .addr	= lw_i2c_client->addr,
	    .flags	= I2C_M_RD,
	    .len	= rxlength,
	    .buf	= rxbuf,
	},
    };

    //ret = wmt_i2c_xfer_continue_if_4(msgs, 2, 1);

    ret = i2c_transfer(lw_i2c_client->adapter, &msgs[0], 2);//add by jackie
    if (ret != 2)
    {   
	    dbg("msg i2c rxdata error: %d\n", ret);
        return -1;
    }
    else
    {
        return 0;
    }

#if 0
	struct i2c_msg xfer_msg[2];
	if (reg < 0x80) {
		i2c_transfer(client->adapter, xfer_msg, ARRAY_SIZE(xfer_msg));
		msleep(5);
	}
	return i2c_transfer(client->adapter, xfer_msg, ARRAY_SIZE(xfer_msg)) == ARRAY_SIZE(xfer_msg) ? 0 : -EFAULT;
#endif 

}

/**
** for write register
** txdata:register address and value u8
** length:txdata length
**/

static int lw86x0_i2c_txdata(char *txdata, int length)
{
	int ret;

	struct i2c_msg msg[] = {
		{
			.addr	= lw_i2c_client->addr,
			.flags	= 0,
			.len	= length,
			.buf	= txdata,
		},
	};


    ret = i2c_transfer(lw_i2c_client->adapter, &msg[0], 1);//1

	//ret = wmt_i2c_xfer_continue_if_4(msg, 1, 1);
	if (ret != 1)
    {
		dbg("i2c txdata error: %d\n", ret);
        return -1;
    }
    else
    {
        return 0;
    }
  
}

/**
** Interface write register for other functions
** addr:write register address
** value:write register value
**/

int lw86x0_write_reg(u16 addr, u16 value)
{
    u8 buf[4];
    int ret = -1;
    unsigned char * pVal    = (unsigned char *) &value;
    unsigned char * pOffset = (unsigned char *) &addr;
    buf[0] = pOffset[1];
    buf[1] = pOffset[0];
    buf[2] = pVal[1];
    buf[3] = pVal[0];
    ret = lw86x0_i2c_txdata(buf, 4);
    if (ret < 0) 
    {
        dbg("lw86x0_write_reg error: %d\n", ret);
        return -1;
    }
    return 0;
}

/*int lw86x0_write_reg_multi(u16 start_addr, u16 value[], u16 num)
{
    u8 buf[MAX_MULTI_DATA_SIZE];
    int ret = -1;
    int i = 0;
    unsigned char * pVal    = (unsigned char *) &value[0];
    unsigned char * pOffset = (unsigned char *) &addr;
    buf[0] = pOffset[1];
    buf[1] = pOffset[0];
    //buf[2] = pVal[1];
    //buf[3] = pVal[0];
    for(i = 0; i < num; i++)
    {
        pVal    = (unsigned char *) &value[i];
        buf[2*i + 2] = pVal[1];
        buf[2*i + 3] = pVal[0];
    }

    ret = lw86x0_i2c_txdata(buf, num*2+2);

    if (ret < 0) 
    {
        dbg("lw86x0_write_reg error: %d\n", ret);
        return -1;
    }
    return 0;
}*/
/**
** Interface read register for other functions
** addr:read register address
** pdata:read register value
** regcnt:read register count
**/

int lw86x0_read_reg(u16 addr, u16 *pdata, int regcnt)
{
    int ret;

    u16 offset_reverse = swap16(addr);
    ret = lw86x0_i2c_rxdata((char*)pdata, (char*)&offset_reverse, 2*regcnt);

    if (ret < 0)
    {   
	    dbg("lw86x0_read_reg error: %d\n", ret);
        return -1;
    }
    else
    {
        swap_byte_in_buffer(pdata, regcnt);
        return 0;
    }
}

int wmt_ts_load_firmware(char* firmwarename, unsigned char** firmdata, int* fwlen)
{
	int i;
	const struct firmware *fw_entry;
	for (i = 0; i < 3; i++) {
		if(request_firmware(&fw_entry, firmwarename, &lw_i2c_client->dev)!=0)
			printk(KERN_ERR "cat't request firmware #%d\n", i);
		else
			break;
	}
	if (i == 3)
		return -EINVAL;

	if (fw_entry->size <= 0) {
		printk(KERN_ERR "load firmware error\n");
		release_firmware(fw_entry);
		return -1;
	}

	*firmdata = kzalloc(fw_entry->size + 1, GFP_KERNEL);
	memcpy(*firmdata, fw_entry->data, fw_entry->size);
	*fwlen = fw_entry->size;
	release_firmware(fw_entry);

	return 0;
}

static u16 *default_setting_table;
static int cfg_len;

static int load_cfgfile(void)
{
	u32 val[2];
	u16 temp[200];
	int i = 0;
	char cfgname[32] = {0};
	u8 *pData;
	int fileLen;
	char *p;
	char *s; 

	wmt_ts_get_configfilename(cfgname);
	if (wmt_ts_load_firmware(cfgname, &pData, &fileLen)) {
		errlog("Load config file failed~ \n");
		return -1;
	}
	s = pData;
	p = strstr(s, "COL_NUM");
	sscanf(p, "COL_NUM=%d;", &COL_NUM);
	p = strstr(s, "ROW_NUM");
	sscanf(p, "ROW_NUM=%d;", &ROW_NUM);
	p = strstr(s, "SKIP_ZERO_POINT");
	sscanf(p, "SKIP_ZERO_POINT=%d;", &SKIP_ZERO_POINT);
    dbg("COL_NUM=%d;ROW_NUM=%d;SKIP_ZERO_POINT=%d;",COL_NUM,ROW_NUM,SKIP_ZERO_POINT);
	
	p = pData;
	while (*p != '{') {
		p++;
		if(*p == '\0') {
			errlog("Bad config file\n");
			i = -1;
			goto end;
		}
	}
	while (*p != '}') {
		if (!strncmp(p, "0x", 2)) {
			i++;
			if ((i & 0x0001) != 0) {
				sscanf(p, "0x%x,0x%x,", val, val+1);
				temp[i-1] = val[0] & 0x0000FFFF;
				temp[i] = val[1] & 0x0000FFFF;
			}
		}
		p++;
		if(*p == '\0') {
			i = -1;
			errlog("Bad config file\n");
			goto end;
		}
	};

	dbg("the number of data:0x%x\n", i);
	default_setting_table = kzalloc(i*2, GFP_KERNEL);
	memcpy(default_setting_table, temp, i*2);
	cfg_len = i;

	dbg("paring config file end.\n");
end:
	kfree(pData);
	return i;
}

void lw86x0_stop_timer(int flags)
{    
	stop_timer = flags;
}


static u16 get_trim_info(void)
{
	u16 trim_info = 0;
	u8 buf[2] = {0};
	lw86x0_write_reg(0x00e4, 0x0000);
    lw86x0_write_reg(0x00e2, 0x0302);
    lw86x0_write_reg(0x00e3, 0x0000);
    lw86x0_write_reg(0x00e2, 0x034e);
    lw86x0_write_reg(0x00e2, 0x0302);
    lw86x0_read_reg(0x00e4, buf, 1);
    lw86x0_write_reg(0x00e2, 0x0000);
    trim_info = buf[1];
    dbg("trim info is %04x",trim_info);
    return trim_info;
}

/**
** load default register setting
**/

void lw86x0_load_def_setting(void)
{
    int i = 0;
    u16 trim_value = 0;
    u16 trim_info = 0;

    lw86x0_write_reg(0x00e6, 0x3311);
    trim_info = get_trim_info();
    for(i = 0; i < cfg_len / 2; i++)
    {
        if(default_setting_table[2*i] == 0xffff)
        {
            msleep(default_setting_table[2*i+1]);
        }
        else
        {
            if(default_setting_table[2*i] == 0x00ee)
            {
                lw86x0_read_reg(0x00ee, &trim_value, 1);
                if(trim_value == 0x00a0)
                {
                    trim_value = 0x00c0 + trim_info;
                }
                else
                {
                    trim_value = 0x100 + trim_value;
                }
                lw86x0_write_reg(0x00ee, trim_value);
            }
            else
            {
                lw86x0_write_reg(default_setting_table[2*i], default_setting_table[2*i+1]);
            }
            //lw86x0_write_reg(default_setting_table[2*i], default_setting_table[2*i+1]);
        }
        /*if(i == 0)
        {
            msleep(100);
        }*/
    }
    if(first_init_reg == 1)
    {
        for(i = 0; i < 19; i++)
        {
            lw86x0_read_reg(0x0092+i, &auto_coff_value[i], 1);
        }
        first_init_reg = 0;
    }
    else
    {
        lw86x0_write_reg(0x0035, 0x0070);
        lw86x0_write_reg(0x0060, 0x0307);
        lw86x0_write_reg(0x0091, 0x0200);
        for(i = 0; i < 19; i++)
        {
            lw86x0_write_reg(0x0092+i, auto_coff_value[i]);
        }
        lw86x0_write_reg(0x0035, 0x2070);
        msleep(100);
        lw86x0_write_reg(0x0060, 0x0306);
    }
}

/**
** set reset pin for lw86x0 
**/

static void lw86x0_hw_reset(void)
{
    wmt_rst_output(0);
    //msleep(500);
    msleep(30);
    wmt_rst_output(1);
}

static void lw86x0_ts_release(void)
{
    int i = 0;
    struct lw86x0_ts_data *data = &l_tsdata;
    int down = 0;

   // dbg("lw86x0_ts_release");

    for (i = 0; i < l_keylen; i++)
    {
	down |= l_tskey[i][0];
    }
    if (down != 0)
    {
	// if down clear the  flag
	for ( i = 0; i < l_keylen; i++)
	{
            l_tskey[i][0] = 0;
	};
	//dbg("key up!\n");
	if (wmt_ts_enable_keyled())
	    wmt_ts_turnoff_light();
    } 
    else 
    {
	    if (!lw86x0_tsdev.penup)
	    {
	        input_mt_sync(data->input_dev);
	        input_sync(data->input_dev);
	        //dbg("rpt pen\n");
	    }
        lw86x0_tsdev.penup = 1;
	//dbg("pen up\n");
	//wake_up(&ts_penup_wait_queue);
    }
}

/**
**set wmt touch key count
**/

void wmt_ts_set_keylen(int keylen)
{
    l_keylen = keylen;
}

/**
**set wmt touch baseaxis
**axis:0--x axis,1--y axis.
**/

void wmt_ts_set_baseaxis(int axis)
{
    l_baseaxis = axis;
}

/**
** set wmt touch key info struct keypos
** index:set key number
** min:key min point value
** max:key max point value
**/

void wmt_ts_set_keypos(int index, int min,int max)
{
    l_keypos[index][0] = min;
    l_keypos[index][1] = max;
}

/**
** report key info to wmt android
**/
#if 0

static int lw86x0_report_key_info(void)
{
	struct lw86x0_ts_data *data = &l_tsdata;
    u16 x, y;
    u16 key_stpos,key_vrpos; // the stable and variable position for touch key
    int i =0;
    lw86x0_read_reg(0x0161, &x, 1);
    lw86x0_read_reg(0x016B, &y, 1);    
    if (wmt_ts_enable_tskey() != 0)		
    {           
        switch (l_baseaxis)         
        {
            case 0:                 
                key_stpos = y;                    
                key_vrpos = x;                    
                break;              
            case 1:             
            default:                    
                key_stpos = x;                    
                key_vrpos = y;                    
                break;          
        }      
    }
    for (i=0;i < l_keylen;i++)			
    {               
        if ((key_vrpos>=l_keypos[i][0]) && (key_vrpos<=l_keypos[i][1]))             
        {                   
            // report the key                   
            if (0 == l_tskey[i][0])                 
            {                       
                input_report_key(data->input_dev, l_tskey[i][1], 1);                        
                input_report_key(data->input_dev, l_tskey[i][1], 0);                        
                input_sync(data->input_dev);                        
                l_tskey[i][0] = 1;                      
                dbg("report tskey:%d\n",i);                     
                if (wmt_ts_enable_keyled())                         
                    wmt_ts_turnon_light();                  
            }                  
            return 1;//key            
        }    
    }
    return 0;//no key
   
}
#endif

static void check_mode(void)
{
	int dcin = wmt_charger_is_dc_plugin();
	int hdmiin = (REG32_VAL(REG_HDMI_HOTPLUG_DETECT) & BIT31) >> 31;

	if (dcin == l_powermode && hdmiin == l_hdmimode)
		return;
	if (!dcin && !hdmiin) {
		klog("DC and HDMI removed\n");
		lw86x0_write_reg(0x01e9, 0x0000);
	} else {
		klog("DC or HDMI in\n");
		lw86x0_write_reg(0x01e9, 0x0001);
	}
	l_powermode = dcin;
	l_hdmimode = hdmiin;
}

/**
** report touch info to wmt android
** touch_number: touch count
**/

static void lw86x0_report_touch_info(u16 touch_number)
{
    struct lw86x0_ts_data *data = &l_tsdata;
    struct ts_event *event = &data->event;
    u16 i;

    //old_event = *event;
    //dbg("Enter into lw86x0_report_touch_info");
    check_mode();
    if(touch_number == 0)
    {
        input_mt_sync(data->input_dev);
        input_sync(data->input_dev);
        return;
    }
    if(touch_number> wmt_ts_get_fingernum()){
        //dbg("Invalid Touch point count is found %d",touch_number);
        return;
    }
    event->touch_point = touch_number;
    
    //memset(event->x, 0, SUPPORT_POINT_NUM*sizeof(u16) );
    //memset(event->y, 0, SUPPORT_POINT_NUM*sizeof(u16) );
    //memset(event->pressure, 0, SUPPORT_POINT_NUM*sizeof(u16) );
    for( i = 0; i <touch_number; i++ )
    {
        lw86x0_read_reg(0x0161+i, &event->x[i], 1);
        lw86x0_read_reg(0x016B+i, &event->y[i], 1);
        lw86x0_read_reg(0x0175+i, &event->pressure[i], 1);
    }

    for (i = 0; i < touch_number; i++) 
    {
        int x = (event->x[i]) & 0x03ff;
        int y = (event->y[i]) & 0x03ff;
        int id = ((event->x[i])>>12)&0x000f;
	int tmp;

        if(x>wmt_ts_get_resolvX())
        {
            x = wmt_ts_get_resolvX();
        }

        if(y>wmt_ts_get_resolvY())
        {
            y= wmt_ts_get_resolvY();
        }

	if (wmt_ts_get_xaxis()) {
		tmp = x;
		x = y;
		y = tmp;
	}
	if (wmt_ts_get_xdir())
		x = wmt_ts_get_resolvX() - x;
	if (wmt_ts_get_ydir())
		y = wmt_ts_get_resolvY() - y;

	if (wmt_ts_get_lcdexchg()) {
		int tmp;
		tmp = x;
		x = y;
		y = wmt_ts_get_resolvX() - tmp;
	}

	dbg("id %d [%d, %d] p %d",id, x, y, event->pressure[i]);
        //input_report_abs(input_dev, ABS_MT_TOUCH_MAJOR,event->pressure[i]);                
        input_report_abs(data->input_dev, ABS_MT_POSITION_X, x);
        input_report_abs(data->input_dev, ABS_MT_POSITION_Y, y);
        input_report_abs(data->input_dev, ABS_MT_TRACKING_ID, id);        
        input_mt_sync(data->input_dev);
    }
    /* SYN_REPORT */
    input_sync(data->input_dev);
}

/**
**lw86x0 touch irq work function
**/

static void lw86x0_ts_touch_irq_work(struct work_struct *work)
{

    u16 int_touch_status=0;
    mutex_lock(&ts_data_mutex);

    //dbg("Enter into lw86x0_ts_touch_irq_work");

    //finger_up_status = 0;
    lw86x0_read_reg(0x01f5, &int_touch_status, 1);
    
    //dbg("Read 0x1f5 = %d",int_touch_status);
    if( int_touch_status & 0x0001)
    {
        u16 touch_number=0;
        lw86x0_read_reg(0x0160, &touch_number, 1);
        //dbg("tn=%d\n",touch_number);
        if(touch_number==0)
        {
            skip_zero_num++;
            if(SKIP_ZERO_POINT==skip_zero_num)
            {
                dbg("tn=%d\n",touch_number);
                lw86x0_write_reg(0x01f2, 0x0010);
                lw86x0_report_touch_info(touch_number);
                lw86x0_ts_release();
                //finger_up_status = 1;
            }
            else if(SKIP_ZERO_POINT<skip_zero_num)
            {
                skip_zero_num = SKIP_ZERO_POINT+1;
            }
        }
        else if(touch_number==15)
        {
            //dbg("touch_number=%d\n",touch_number);
        }
        else
        {
            dbg("tn=%d\n",touch_number);
            lw86x0_write_reg(0x01f2, 0x0011);
            skip_zero_num = 0;
            lw86x0_report_touch_info(touch_number);
        }
    }
    else
    {
        //finger_up_status = 1;
    }
    lw86x0_write_reg(0x01f5, 0xffff);//clear interrupt
    //mdelay(500);
    //dbg("clear interrupt 1");
    lw86x0_write_reg(0x01f5, 0xffff);//clear interrupt
    //mdelay(500);
    //dbg("clear interrupt 2");
    //lw86x0_write_reg(0x01f5, 0xffff);//clear interrupt
    //mdelay(500);
    //dbg("clear interrupt 3");
    //lw86x0_write_reg(0x01f5, 0xffff);//clear interrupt
    //mdelay(500);
    //dbg("clear interrupt 4");
    //dbg("Write 0x1f5 = 0xffff");
    //lw86x0_read_reg(0x01f5, &int_touch_status, 1);
    //dbg("Re-Read 0x1f5 = %d",int_touch_status);

    if(g_dbgmode==0)
    {
        //dbg("Enable Irq");
        wmt_enable_gpirq(tsirq_gpio); 
    }
    mutex_unlock(&ts_data_mutex);
}

static irqreturn_t lw86x0_ts_interrupt(int irq, void *dev_id)
{
    //dbg("enter lw86x0_ts_interrupt");
    //if (!wmt_is_tsirq_enable(tsirq_gpio))
    //{
    //    dbg("tsirq not enabled");
	//    return IRQ_NONE;
    //}
    if (wmt_is_tsint(tsirq_gpio))
    {
	    wmt_clr_int(tsirq_gpio);		
	    wmt_disable_gpirq(tsirq_gpio);
	    if(!l_early_suspend)
        {
            //dbg("tsirq enabled");
            queue_work(l_tsdata.ts_workqueue, &l_tsdata.touch_event_work);
        }   
	    return IRQ_HANDLED;
    }
    return IRQ_NONE;
}

static void reset_chip(void)
{
	printk("\nReset LW IC\n\n");
    lw86x0_write_reg(0x00e6, 0x3311);
    lw86x0_write_reg(0x00e0, 0x0005);
    lw86x0_write_reg(0x0214, 0x0020);
    lw86x0_write_reg(0x033d, 0x8100);
    mdelay(500);
    wmt_rst_output(0);
    wmt_set_irq_mode(tsirq_gpio, 0);
    mdelay(100);
    wmt_rst_output(1);
    wmt_set_gpirq(tsirq_gpio, GIRQ_FALLING);
    lw86x0_load_def_setting();
    if(g_dbgmode==0)    
    {
        wmt_enable_gpirq(tsirq_gpio);
    }
}


static int check_chip_status(void)
{ 
    u16 read_value = 0;
    u16 read_sram = 0;
    int ret = lw86x0_read_reg(0x00e6, &read_value, 1);
    if(ret != 0)
    {
        reset_chip();
        return 0;
    }
    if(read_value != 0x3311)
    {
        reset_chip();
        return 0;
    }
    else
    {
        lw86x0_read_reg(0x00e0, &read_value, 1);
        if(read_value != 0x0005 && read_value != 0x000d)
        {
        		dbg("0x00e0!=0x0005,0x000d\n");
            reset_chip();
            return 0;
        }
        lw86x0_read_reg(0x0180, &read_sram, 1);
        if(read_sram != 0)
        {
        		dbg("0x0180!=0\n");
            reset_chip();
            return 0;
        }
        lw86x0_read_reg(0x0181, &mcu_status, 1);
        if(mcu_status_old == mcu_status)
        {
        			dbg("0x0180 old!=new\n");	
							reset_chip();
							mcu_status_old = mcu_status;
	            return 0;
     	  }
     	  else
     	  {
     	  			mcu_status_old = mcu_status;
     	  			return 1;
     	  }        
    }
    return 1;
}

static void phone_status_listener(struct work_struct *work)
{
    if(stop_timer == 0)
    {
        check_chip_status();
    }
}

static void lw86x0_ts_polling_phone_status(long unsigned int dev_addr)
{
    schedule_work(&phone_status_work);
    mod_timer(&polling_phone_status_timer, jiffies + msecs_to_jiffies(2000));
}

/**
** lw86x0 ts early suspend function
**/
#ifdef CONFIG_HAS_EARLYSUSPEND
static void ts_early_suspend(void)
{
    wmt_disable_gpirq(tsirq_gpio);
}

//#ifdef CONFIG_HAS_EARLYSUSPEND

static void lw86x0_ts_early_suspend(struct early_suspend *handler)
{
    printk("lw86x0_ts_early_suspend\n");
    ts_early_suspend();	
    l_early_suspend = 1;
    lw86x0_write_reg(0x000c, 0xffff);
    lw86x0_write_reg(0x033d, 0x0d60);
    lw86x0_write_reg(0x00e2, 0x0300);
    lw86x0_write_reg(0x000d, 0x4000);
    lw86x0_write_reg(0x00e5, 0x4c01);
    stop_timer = 1;
}

/**
** lw86x0 late resume function
**/
static void ts_late_resume(void)
{
    printk("ts_late_resume\n");
    //wmt_disable_gpirq(tsirq_gpio);
    //lw86x0_hw_reset();
    l_early_suspend = 0;
        
    wmt_set_gpirq(tsirq_gpio, GIRQ_FALLING);
    if(g_dbgmode==0)    
    {
        printk("g_dbgmode==0\n");
        wmt_enable_gpirq(tsirq_gpio);
    }
}


static void lw86x0_ts_late_resume(struct early_suspend *handler)
{
    printk("==lw86x0_ts_resume=\n");
    int ret = check_chip_status();
    if(ret == 1)
    {
        lw86x0_write_reg(0x000d, 0xc000);
        lw86x0_write_reg(0x00e2, 0x0100);
        lw86x0_write_reg(0x00e5, 0x4c00);
        lw86x0_write_reg(0x000c, 0xffff);
    }
    ts_late_resume();
    l_early_suspend = 0;
    stop_timer = 0;
}
#endif 

/**
** lw86x0 ts suspend function
**/

static int lw86x0_suspend(struct platform_device *pdev, pm_message_t state)
{
	return 0;
}

/**
** lw86x0 resume function
**/

static int lw86x0_resume(struct platform_device *pdev)
{
    lw86x0_hw_reset();
    first_init_reg = 1;
    lw86x0_load_def_setting();
    return 0;
}

#ifdef SUPPORT_FW_UPGRADE
//add by jackie
#define BYTES_PER_PACKET  64
//#define FILEPATH "/etc/firmware/TOUCH.BIN"
int fileLen;
u8  *pData;

void read_data_from_fw_file(void)
{
    char fwname[32] = {0};
	wmt_ts_get_firmwfilename(fwname);
	if (wmt_ts_load_firmware(fwname, &pData, &fileLen)) {
		dbg("Load firmware file failed~ \n");
		return;
	}    
}

u8 check_fw_version(void)
{   
    //Fw_Ver* pfwVerInFile = NULL;
    //Fw_Ver  fwVer;
    
    u16  fw_check_sum;
    u16  fw_file_check_sum = 0;

    read_data_from_fw_file();
    /*pfwVerInFile = (Fw_Ver* )&(pData[0x2000]);    

    printk("struct data:%c%c%d%d\n",pfwVerInFile->magic_num1,pfwVerInFile->magic_num2,pfwVerInFile->mj_ver,pfwVerInFile->mn_ver);//add by jackie
    lw86x0_write_reg(0x00e6,0x3311);
    lw86x0_flash_read((u8*)&fwVer,0x2000,4);
    printk("lw86x0_flash:%c%c%d%d\n",fwVer.magic_num1,fwVer.magic_num2,fwVer.mj_ver,fwVer.mn_ver);//add by jackie
    //printk("lw86x0_flash:%d%d\n",fwVer.magic_num1,fwVer.magic_num2);//add by jackie
    if((fwVer.magic_num1!='L'||fwVer.magic_num2!='W')
        ||((fwVer.magic_num1=='L'&&fwVer.magic_num2=='W')
            &&(pfwVerInFile->magic_num1=='L'&&pfwVerInFile->magic_num2=='W')
            &&(fwVer.mj_ver!=pfwVerInFile->mj_ver || fwVer.mn_ver!=pfwVerInFile->mn_ver))
        )*/
    lw86x0_write_reg(0x00e6, 0x3311);
    lw86x0_write_reg(0x0020, 0x9000);	
    lw86x0_write_reg(0x0002, 0x8900);
    lw86x0_write_reg(0x0115, 0x0100);	
    lw86x0_write_reg(0x0020, 0x1000);	
    msleep(200);
    fw_check_sum = get_fw_check_sum();
    fw_file_check_sum = get_fw_file_check_sum();
    printk("**********fw_check_sum = %04x, fw_file_check_sum = %04x\n",fw_check_sum,fw_file_check_sum);
    if(((fw_check_sum&0xff00)!=0x8000)||((fw_check_sum&0x00ff)!=fw_file_check_sum))
    {
	lw86x0_write_reg(0x0002, 0x8800);
        printk("firmware crc check is not equal, update firmware......\n");
        return 1;//return 1 means needing upgrade
    }
    else
    {   
        printk("firmware is not updated......\n");
	lw86x0_write_reg(0x0002, 0x8800);
        return 0;
    }    
}

void fw_download(void)
{
    int pkt_num = (fileLen+BYTES_PER_PACKET-1)/BYTES_PER_PACKET;
    int i;
    int last_pkt_size = ((int)fileLen) % BYTES_PER_PACKET;
    printk("pkt_num is:%d\n",pkt_num);//add
    if(last_pkt_size==0)
    {
        last_pkt_size = BYTES_PER_PACKET;
    }
    lw86x0_flash_write_prepare();
    for(i=0;i<pkt_num;i++)
    {
        lw86x0_flash_write(&pData[i*BYTES_PER_PACKET],i*BYTES_PER_PACKET,(i==pkt_num-1)?last_pkt_size:BYTES_PER_PACKET);     
    }
    lw86x0_flash_write_finish(fileLen);       
    printk("firmware is updated......\n");//add
}


u8 get_fw_file_check_sum(void)
{
	//u16 dataLen;
	//u8* pData = NULL;
	u16 i;
	u8 checksum = 0;
  	printk("**********dataLen = %04x\n",fileLen);
	for(i=0;i<fileLen;i++)
	{
		checksum+=pData[i];
	}
	return checksum;
}

u16 get_fw_check_sum(void)
{
    u8 cnt = 10;
    u16 check_sum = 0;
    //u16 fw_length = 0;
    while(cnt>0)
    {
        lw86x0_read_reg(0x0182, &check_sum, 1);
        printk("**********check_sum = %04x\n",check_sum);
        if((check_sum&0xff00)==0x8000)
        {
            break;
        }
        cnt--;
        msleep(100);
    }
    return check_sum;
}

static void fw_upgrader(void)
{
	u16  fw_check_sum;
	u16  fw_file_check_sum = 0;

    if(check_fw_version()==0)
    {
        return;
    }

	lw86x0_write_reg(0x00e6, 0x3311);
	fw_download();
	lw86x0_write_reg(0x0020, 0x9000);	
	lw86x0_write_reg(0x0002, 0x8900);
	lw86x0_write_reg(0x0115, 0x0100);	
	lw86x0_write_reg(0x0020, 0x1000);	
	msleep(200);
	fw_check_sum = get_fw_check_sum();
	fw_file_check_sum = get_fw_file_check_sum();
	printk("**********fw_check_sum = %04x, fw_file_check_sum = %04x\n",fw_check_sum,fw_file_check_sum);
	if(((fw_check_sum&0xff00)!=0x8000)||((fw_check_sum&0x00ff)!=fw_file_check_sum))
	{
		printk("*********redownload fw\n");
		fw_download();
		lw86x0_write_reg(0x00e6, 0x3311);
		lw86x0_write_reg(0x0020, 0x9000);	
		lw86x0_write_reg(0x0002, 0x8900);
		lw86x0_write_reg(0x0115, 0x0100);	
		lw86x0_write_reg(0x0020, 0x1000);	
		msleep(200);
		fw_check_sum = get_fw_check_sum();
		fw_file_check_sum = get_fw_file_check_sum();
		printk("**********re-check fw_check_sum = %04x, fw_file_check_sum = %04x\n",fw_check_sum,fw_file_check_sum);
		if(((fw_check_sum&0xff00)!=0x8000)||((fw_check_sum&0x00ff)!=fw_file_check_sum))
	  {
	  		lw86x0_flash_write_prepare();
	  }
	}
	else
	{
	}
	lw86x0_write_reg(0x0002, 0x8800);
    kfree(pData);
	lw86x0_hw_reset();
    
}

#endif


static int wmt_wakeup_bl_notify(struct notifier_block *nb, unsigned long event,
	void *dummy)
{
	//printk("get notify\n");
	switch (event) {
		case BL_CLOSE:
    			l_early_suspend = 1;
    			wmt_disable_gpirq(tsirq_gpio);
    			stop_timer = 1;
				cancel_work_sync(&l_tsdata.touch_event_work);
				cancel_work_sync(&phone_status_work);
			printk("\nclose backlight\n\n");
			break;
		case BL_OPEN:
    			l_early_suspend = 0;
    			wmt_enable_gpirq(tsirq_gpio);
				lw86x0_write_reg(0x01f5,0xffff);//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    			stop_timer = 0;
			printk("\nopen backlight\n\n");
			break;
	}

	return NOTIFY_OK;
}

static struct notifier_block wmt_bl_notify = {
	.notifier_call = wmt_wakeup_bl_notify,
};

static int lw86x0_ts_probe(struct platform_device *pdev)
{
    int err = 0;
    int i = 0;
    u16 read_from_e6 = 0;   
    lw_i2c_client  = ts_get_i2c_client();//get i2c_client

    memset(&l_tsdata, 0 ,sizeof(l_tsdata));
    INIT_WORK(&l_tsdata.touch_event_work, lw86x0_ts_touch_irq_work);
    mutex_init(&ts_data_mutex);

    l_tsdata.ts_workqueue = create_singlethread_workqueue("lw86x0-ts-queue");
    if (!l_tsdata.ts_workqueue) {
	err = -ESRCH;
	goto exit_create_singlethread;
    }

    l_tsdata.input_dev = input_allocate_device();
    if (!l_tsdata.input_dev) {
	err = -ENOMEM;
	dbg("failed to allocate input device\n");
	goto exit_input_dev_alloc_failed;
    }
	
    l_tsdata.input_dev->evbit[0] = BIT_MASK(EV_SYN) | BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
    l_tsdata.input_dev->propbit[0] = BIT_MASK(INPUT_PROP_DIRECT);

    if (wmt_ts_get_lcdexchg()) {
	    input_set_abs_params(l_tsdata.input_dev,
			    ABS_MT_POSITION_X, 0, wmt_ts_get_resolvY(), 0, 0);
	    input_set_abs_params(l_tsdata.input_dev,
			    ABS_MT_POSITION_Y, 0, wmt_ts_get_resolvX(), 0, 0);
    } else {
	    input_set_abs_params(l_tsdata.input_dev,
			    ABS_MT_POSITION_X, 0, wmt_ts_get_resolvX(), 0, 0);
	    input_set_abs_params(l_tsdata.input_dev,
			    ABS_MT_POSITION_Y, 0, wmt_ts_get_resolvY(), 0, 0);
    }
    input_set_abs_params(l_tsdata.input_dev,
                 ABS_MT_TRACKING_ID, 0, 15, 0, 0);
    
    l_tsdata.input_dev->name		= LW86X0_NAME;
    for (i = 0; i < TS_KEY_NUM; i++)
    {
	set_bit(l_tskey[i][1], l_tsdata.input_dev->keybit);
    };
    err = input_register_device(l_tsdata.input_dev);
    if (err) {
	errlog("lw86x0_ts_probe: failed to register input device.");
	goto exit_input_register_device_failed;
    }
    
#ifdef SUPPORT_FW_UPGRADE
    fw_upgrader();
    mdelay(500);   
#endif

    err = load_cfgfile();
    if (err < 0)
        goto exit_load_cfgfile_failed;
    lw86x0_load_def_setting();
    
#ifdef CONFIG_HAS_EARLYSUSPEND
    l_tsdata.early_suspend.level = EARLY_SUSPEND_LEVEL_DISABLE_FB+ 1;
    l_tsdata.early_suspend.suspend = lw86x0_ts_early_suspend;
    l_tsdata.early_suspend.resume	= lw86x0_ts_late_resume;
    register_early_suspend(&l_tsdata.early_suspend);
#endif

    // init interrupt gpio
    tsirq_gpio = wmt_ts_get_gpionum();
    wmt_set_gpirq(tsirq_gpio, GIRQ_FALLING);//GIRQ_FALLING);
    wmt_disable_gpirq(tsirq_gpio);
	
    if(request_irq(wmt_get_tsirqnum(), lw86x0_ts_interrupt, IRQF_SHARED, "ts_lw86x0", l_tsdata.input_dev) < 0){
        errlog("Could not allocate intrrupt for ts_lw86x0 !\n");
        err = -1;
        goto exit_register_irq;
    }
    lw86x0_ts_touch_irq_work(&l_tsdata.touch_event_work);
    if(g_dbgmode==0)
    {
        wmt_enable_gpirq(tsirq_gpio);
    }
    msleep(5);
    dbg("irqgpio=%d,irq=%d,resetgpio=%d\n", tsirq_gpio, wmt_get_tsirqnum(),wmt_ts_get_resetgpnum());

    lw86x0_read_reg(0x00e6, &read_from_e6, 1);
    if(read_from_e6 == 0x3311 || read_from_e6 == 0xa311)
    {
        INIT_WORK(&phone_status_work, phone_status_listener);
        init_timer(&polling_phone_status_timer);
        setup_timer(&polling_phone_status_timer, lw86x0_ts_polling_phone_status, (long unsigned int) pdev);
        lw86x0_ts_polling_phone_status((long unsigned int) pdev);
    }

    register_bl_notifier(&wmt_bl_notify);

    return 0;
exit_register_irq:
#ifdef CONFIG_HAS_EARLYSUSPEND//add by jackie
    unregister_early_suspend(&l_tsdata.early_suspend);
#endif
	kfree(default_setting_table);
exit_load_cfgfile_failed:
    
exit_input_register_device_failed:
    input_free_device(l_tsdata.input_dev);
exit_input_dev_alloc_failed:
    cancel_work_sync(&l_tsdata.touch_event_work);
    destroy_workqueue(l_tsdata.ts_workqueue);
exit_create_singlethread:
    return err;
}

static int lw86x0_ts_remove(struct platform_device *pdev)
{
	kfree(default_setting_table);
#ifdef CONFIG_HAS_EARLYSUSPEND
    unregister_early_suspend(&l_tsdata.early_suspend);
#endif 
    free_irq(wmt_get_tsirqnum(), l_tsdata.input_dev);
    input_unregister_device(l_tsdata.input_dev);
    flush_workqueue(l_tsdata.ts_workqueue);
    cancel_work_sync(&l_tsdata.touch_event_work);
    destroy_workqueue(l_tsdata.ts_workqueue);
    mutex_destroy(&ts_data_mutex);
    del_timer(&polling_phone_status_timer);
    unregister_bl_notifier(&wmt_bl_notify);
    dbg("remove...\n");
    return 0;
}


static int lw86x0_ts_init(void)
{
    dbg("lw86x0_ts_init\n");
    lw86x0_hw_reset();
    return 0;
}

static void  lw86x0_ts_exit(void)
{
    dbg("lw86x0_ts_exit\n");    
}

struct wmtts_device lw86x0_tsdev = {
    .driver_name = "s_lw86x0_ts",
    .ts_id       = "lw86x0",
    .init        = lw86x0_ts_init,
    .exit        = lw86x0_ts_exit,
    .probe       = lw86x0_ts_probe,
    .remove      = lw86x0_ts_remove,
    .suspend     = lw86x0_suspend,
    .resume      = lw86x0_resume,
    .penup       = 1,
};


#include <linux/unistd.h>
#include <linux/time.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/suspend.h>
#include <linux/proc_fs.h>
#include <linux/input.h>
#include <linux/types.h>
#include <linux/platform_device.h>
#include <mach/hardware.h>
#include <asm/uaccess.h>
#include <linux/i2c.h>//add
#include "wmt_ts.h"
#include "lw86x0_ts.h"
//#include "wmt_custom_lw86x0.h"


struct i2c_client *l_client = NULL;



/////////////////////////////////////////////////////////////////

// commands for ui
#define TS_IOC_MAGIC  't'
#define LW86X0_READ_REG                 _IOWR(TS_IOC_MAGIC, 1,  int*)
#define LW86X0_WRITE_REG                _IOW(TS_IOC_MAGIC,  2,  int*)
#define LW86X0_FLASH_DOWNLOAD           _IOW(TS_IOC_MAGIC,  3, int *)
#define LW86X0_FLASH_UPLOAD             _IOWR(TS_IOC_MAGIC, 4, int *)
#define LW86X0_CTRL_DEBUG_MODE          _IOW(TS_IOC_MAGIC,  5, int *)
#define LW86X0_CTRL_RD_DIFF	        _IOR(TS_IOC_MAGIC,  6, int *)		
#define LW86X0_CTRL_RD_CDC		_IOR(TS_IOC_MAGIC,  7, int *)
#define LW86X0_CTRL_RD_AMB              _IOR(TS_IOC_MAGIC,  8, int *)
#define LW86X0_CTRL_STOP_TIMER          _IOR(TS_IOC_MAGIC,  15, int *)
#define TS_IOC_MAXNR                    15

//
#define TS_MAJOR            11
#define TS_DRIVER_NAME	 	"wmtts_touch"
#define TS_NAME          	"wmtts"
#define WMTTS_PROC_NAME     "wmtts_config"

#define LIGHT_ON_WAIT_TIME  5000 // 5s

#define EXT_GPIO0   	0
#define EXT_GPIO1   	1
#define EXT_GPIO2   	2
#define EXT_GPIO3   	3
#define EXT_GPIO4	4
#define EXT_GPIO5	5
#define EXT_GPIO6  	6
#define EXT_GPIO7   	7

struct touch_tp_info {
	char name[64];
	unsigned int i2caddr;
	int xaxis; // 0:x,1:x swap with y
	int xdir; // 1:positive,-1:revert
	int ydir; // 1:positive,-1:revert
	int finger_num;
};


static struct touch_tp_info l_tp[] = {
	{"LW86X0",(0x30>>1), 0, 1, 1},
};

static int irq_gpio;
static int rst_gpio;
static int keyled_gpio = -1;
static int light_level;
static int light_time = 5000; // unit: ms
static int panelres_x;
static int panelres_y;
static int lcd_exchg = 0;
static DECLARE_WAIT_QUEUE_HEAD(queue);
static TS_EVENT g_evLast;
static struct mutex cal_mutex;

static struct class* l_dev_class = NULL;
static struct device *l_clsdevice = NULL;
extern struct wmtts_device lw86x0_tsdev;
static struct wmtts_device* l_tsdev = &lw86x0_tsdev;
static unsigned char ts_i2c_addr = 0;

static struct proc_dir_entry* l_tsproc = NULL;
static struct timer_list l_lighttimer; // for shaking
static int l_tskey_btn = 0; // zero to disable touch key, positive to support touch key

#if 0
//add
struct tp_infor{

char name[64];
int i2caddr;
int xaxis;
int xdir;
int ydir;
int finger_num;
};

//add by jackie
//static int l_tpindex = -1;
static struct tp_infor l_tpinfor[1];
#endif 
/////////////////////////////////////////////////////
//   extrenal function
/////////////////////////////////////////////////////
extern int wmt_getsyspara(char *varname, unsigned char *varval, int *varlen);
extern int wmt_setsyspara(char *varname, unsigned char *varval);
/////////////////////////////////////////////////////
static int ts_writeproc( struct file   *file,
                           const char    *buffer,
                           unsigned long count,
                           void          *data );
static int ts_readproc(char *page, char **start, off_t off,
			  int count, int *eof, void *data);


char g_dbgmode = 0;
int COL_NUM;
int ROW_NUM;
int SKIP_ZERO_POINT;

int  wmt_ts_get_configfilename(char* fname)
{
	sprintf(fname,"%s.cfg",l_tp[0].name);
	return 0;
}

int  wmt_ts_get_firmwfilename(char* fname)
{
	sprintf(fname,"%s_fw.bin",l_tp[0].name);
	return 0;
}

int wmt_ts_get_xaxis(void)
{
    return l_tp[0].xaxis;
}

int wmt_ts_get_xdir(void)
{
    return l_tp[0].xdir;
}

int wmt_ts_get_ydir(void)
{
    return l_tp[0].ydir;
}

int wmt_ts_get_fingernum(void)
{
    return l_tp[0].finger_num;
}

 int wmt_ts_get_gpionum(void)
{
    return irq_gpio;
}

int wmt_ts_get_resetgpnum(void)
{
    return rst_gpio;
}

int wmt_ts_get_lcdexchg(void)
{
	return lcd_exchg;
}

int wmt_ts_get_resolvX(void)
{
    return panelres_x;
}

int wmt_ts_get_resolvY(void)
{
    return panelres_y;
}

int wmt_ts_enable_tskey(void)
{
    return l_tskey_btn;
}

int wmt_ts_enable_keyled(void)
{
    return (keyled_gpio>=0 ? 1:0);
}


static void ts_lighttimer_timeout(unsigned long timeout)
{
    // turn off the light
    if (20 == keyled_gpio)
    {
	if (light_level>=0)
	{
	    REG32_VAL(__GPIO_BASE+0x00F0) &= ~BIT4; // output low
	    dbg("turn off the light!\n");
	} else {
	    REG32_VAL(__GPIO_BASE+0x00F0) |= BIT4; // output high
	    dbg("turn off the light!\n");
	}
    }
}


static void wmt_ts_init_light(void)
{
    if (20 == keyled_gpio)
    {
	setup_timer(&l_lighttimer, ts_lighttimer_timeout, 0);
	// init gpio20 and turn off light
	REG32_VAL(__GPIO_BASE+0x00F0) &= ~BIT4; // output low
	REG32_VAL(__GPIO_BASE+0x00B0) |= BIT4; // output enable
	REG32_VAL(__GPIO_BASE+0x0070) |= BIT4; // enable gpio
	REG32_VAL(__GPIO_BASE+0x04F0) |= BIT4; // pull up
	REG32_VAL(__GPIO_BASE+0x04B0) |= BIT4; // enable pull up/down
    }
}

void wmt_ts_turnoff_light(void)
{
    if (20 == keyled_gpio)
    {
	mod_timer(&l_lighttimer, jiffies + msecs_to_jiffies(light_time));
    }
}

void wmt_ts_turnon_light(void)
{
    if (20 == keyled_gpio)
    {
	del_timer(&l_lighttimer);
	if (light_level >= 0)
	{
	    REG32_VAL(__GPIO_BASE+0x00F0) |= BIT4; // output high
	    dbg("turn on the light!\n");
	} else {
	    REG32_VAL(__GPIO_BASE+0x00F0) &= ~BIT4; // output low
	    dbg("turn on the light!\n");
        }
    }
}

static void wmt_ts_remove_light(void)
{
    if (20 == keyled_gpio)
    {
 	if (light_level >= 0)
	{
	    REG32_VAL(__GPIO_BASE+0x00F0) &= ~BIT4; // output low
	} else {
	    REG32_VAL(__GPIO_BASE+0x00F0) |= BIT4; // output high
	}
	del_timer(&l_lighttimer);
    }
}

int wmt_is_tsirq_enable(int num)
{
    int val = 0;

    if(num > 11)
	return 0;

    if(num<4)
	val = REG32_VAL(__GPIO_BASE+0x0300) & (1<<(num*8+7)); 
    else if(num >= 4 && num < 8)
	val = REG32_VAL(__GPIO_BASE+0x0304) & (1<<((num-4)*8+7)); 
    else
	val = REG32_VAL(__GPIO_BASE+0x0308) & (1<<((num-8)*8+7));  

    return val?1:0;
}

int wmt_is_tsint(int num)
{
    if (num > 11)
    {
	return 0;
    }
    return (REG32_VAL(__GPIO_BASE+0x0360) & (1<<num)) ? 1: 0; 
}

void wmt_clr_int(int num)
{
    if (num > 11)
    {
	return;
    }
    REG32_VAL(__GPIO_BASE+0x0360) = 1<<num;
}

void wmt_tsreset_init(int num)
{
    REG32_VAL(__GPIO_BASE+0x0040) |= (1<<num);//&= ~(1<<num); //enable gpio
    REG32_VAL(__GPIO_BASE+0x00C0) |= (1<<num); // out high
    REG32_VAL(__GPIO_BASE+0x0080) |= (1<<num); //output enable
    msleep(5);
    //REG32_VAL(__GPIO_BASE+0x00C0) |= (1<<num);
}

// enable:0-disable,1-enable
void wmt_enable_rst_pull(int enable)
{
    if (enable)
    {
	REG32_VAL(__GPIO_BASE+0x0480) |= (1<<rst_gpio); //enable pull up/down
    } else {
	REG32_VAL(__GPIO_BASE+0x0480) &= ~(1<<rst_gpio); //disable pull up/down
    }
}

// up:0-pull down,1-pull up
void wmt_set_rst_pull(int up)
{
    if (up)
    {
	REG32_VAL(__GPIO_BASE+0x04c0) |= (1<<rst_gpio); //pull up
    } else {
	REG32_VAL(__GPIO_BASE+0x04c0) &= ~(1<<rst_gpio); //pull down
    }
}

// high:0-low level,1-high level
void wmt_rst_output(int high)
{
    REG32_VAL(__GPIO_BASE+0x0040) |= (1<<rst_gpio); //enable gpio
    if (high)
    {
	REG32_VAL(__GPIO_BASE+0x00C0) |= (1<<rst_gpio); // high
    } else {
	REG32_VAL(__GPIO_BASE+0x00C0) &= ~(1<<rst_gpio); // low
    }
    REG32_VAL(__GPIO_BASE+0x0080) |= (1<<rst_gpio); //set output
}

void wmt_rst_input(void)
{
    REG32_VAL(__GPIO_BASE+0x0040) |= (1<<rst_gpio); //enable gpio
    REG32_VAL(__GPIO_BASE+0x0080) &= ~(1<<rst_gpio); //set input
}

int wmt_set_irq_mode(unsigned int num, int mode) 
{
    if(num >11)
 	return -1;
    REG32_VAL(__GPIO_BASE+0x0040) &= ~(1<<num); //enable gpio
    if(mode == 0)
    {
        REG32_VAL(__GPIO_BASE+0x0080) |= (1<<num); //set output
	REG32_VAL(__GPIO_BASE+0x04c0) |= (1<<num); //pull down
	REG32_VAL(__GPIO_BASE+0x0480) &= ~(1<<num); //enable pull up/down
    }
    else if(mode == 1)
        REG32_VAL(__GPIO_BASE+0x0080) &= ~(1<<num); //set input
    //msleep(5);
    return 0;
}


int wmt_set_gpirq(unsigned int num, int type) 
{
    int shift;
    int offset;
    unsigned long reg;
	
    if(num >11)
	return -1;
    //if (num > 9)
	//GPIO_PIN_SHARING_SEL_4BYTE_VAL &= ~BIT4; // gpio10,11 as gpio
    REG32_VAL(__GPIO_BASE+0x0040) &= ~(1<<num); //enable gpio
    REG32_VAL(__GPIO_BASE+0x0080) &= ~(1<<num); //set input
    REG32_VAL(__GPIO_BASE+0x04c0) |= (1<<num); //pull down
    REG32_VAL(__GPIO_BASE+0x0480) &= ~(1<<num); //enable pull up/down

    //set gpio irq triger type
    if(num < 4){//[0,3]
	shift = num;
	offset = 0x0300;
    }else if(num >= 4 && num < 8){//[4,7]
	shift = num-4;
	offset = 0x0304;
    }else{// [8,11]
	shift = num-8;
	offset = 0x0308;
    }
	
    reg = REG32_VAL(__GPIO_BASE + offset);

    switch(type){
	case GIRQ_LOW:
	    reg &= ~(1<<(shift*8+2)); 
	    reg &= ~(1<<(shift*8+1));
	    reg &= ~(1<<(shift*8));
	    break;
	case GIRQ_HIGH:
	    reg &= ~(1<<(shift*8+2)); 
	    reg &= ~(1<<(shift*8+1));
	    reg |= (1<<(shift*8));
	    break;
	case GIRQ_FALLING:
	    reg &= ~(1<<(shift*8+2)); 
	    reg |= (1<<(shift*8+1));
	    reg &= ~(1<<(shift*8));
	    break;
	case GIRQ_RISING:
	    reg &= ~(1<<(shift*8+2)); 
	    reg |= (1<<(shift*8+1));
	    reg |= (1<<(shift*8));
	    break;
	default://both edge
	    reg |= (1<<(shift*8+2)); 
	    reg &= ~(1<<(shift*8+1));
	    reg &= ~(1<<(shift*8));
	    break;
			
    }
    //reg |= 1<<(shift*8+7);//enable interrupt
    reg &= ~(1<<(shift*8+7)); //disable int
    REG32_VAL(__GPIO_BASE + offset) = reg; 
    REG32_VAL(__GPIO_BASE+0x0360) = 1<<num; //clear interrupt status
    msleep(5);
    return 0;
}

int wmt_enable_gpirq(unsigned int num)
{
    if(num > 11)
        return -1;
    if(num<4)
        REG32_VAL(__GPIO_BASE+0x0300) |= 1<<(num*8+7); //enable interrupt 
    else if(num >= 4 && num < 8)
        REG32_VAL(__GPIO_BASE+0x0304) |= 1<<((num-4)*8+7); //enable interrupt 
    else
        REG32_VAL(__GPIO_BASE+0x0308) |= 1<<((num-8)*8+7); //enable interrupt 

    return 0;
}

int wmt_disable_gpirq(unsigned int num)
{	
    if(num > 11)
        return -1;
	
    if(num<4)
        REG32_VAL(__GPIO_BASE+0x0300) &= ~(1<<(num*8+7)); //enable interrupt 
    else if(num >= 4 && num < 8)
        REG32_VAL(__GPIO_BASE+0x0304) &= ~(1<<((num-4)*8+7)); //enable interrupt 
    else
        REG32_VAL(__GPIO_BASE+0x0308) &= ~(1<<((num-8)*8+7)); //enable interrupt 
	
    return 0;
}


int wmt_get_tsirqnum(void)
{
    return IRQ_GPIO;
}

int wmt_ts_set_rawcoord(unsigned short x, unsigned short y)
{
    g_evLast.x = x;
    g_evLast.y = y;
    //dbg("raw(%d,%d)*\n", x, y);
    return 0;
}

static void wmt_ts_platform_release(struct device *device)
{
    return;
}

static struct platform_device wmt_ts_plt_device = {
    .name           = TS_DRIVER_NAME,
    .id             = 0,
    .dev            = {
        .release = wmt_ts_platform_release,
    },
//    .num_resources  = ARRAY_SIZE(wm9715_ts_resources),
//    .resource       = wm9715_ts_resources,
};

static int wmt_ts_suspend(struct platform_device *pdev, pm_message_t state)
{
    dbg("ts suspend....\n");
    if (wmt_ts_enable_keyled())
        wmt_ts_remove_light();
    if (l_tsdev->suspend !=NULL)
    {
        return l_tsdev->suspend(pdev, state);
    }
    return 0;
}
static int wmt_ts_resume(struct platform_device *pdev)
{
    klog("ts resume....\n");
    if (wmt_ts_enable_keyled())
        wmt_ts_init_light();
    if (l_tsdev->resume != NULL)
    {
        return l_tsdev->resume(pdev);
    }
    return 0;
}

static int wmt_ts_probe(struct platform_device *pdev)
{
    l_tsproc= create_proc_entry(WMTTS_PROC_NAME, 0666, NULL/*&proc_root*/);
    if (l_tsproc != NULL)
    {
        l_tsproc->read_proc = ts_readproc;
        l_tsproc->write_proc = ts_writeproc;	
    }
    if (l_tsdev->probe != NULL)
    {
        return l_tsdev->probe(pdev);
    }
    else 
    {
        return 0;
    }
}

static int wmt_ts_remove(struct platform_device *pdev)
{
    if (l_tsproc != NULL)
    {
        remove_proc_entry(WMTTS_PROC_NAME, NULL);
        l_tsproc = NULL;
    }

    if (l_tsdev->remove != NULL)
        return l_tsdev->remove(pdev);
    else
        return 0;
}

static struct platform_driver wmt_ts_plt_driver = {
    .driver = {
        .name = TS_DRIVER_NAME,
        .owner	= THIS_MODULE,
    },
    .probe   = wmt_ts_probe,
    .remove  = wmt_ts_remove,
    .suspend = wmt_ts_suspend,
    .resume  = wmt_ts_resume,
};

static int wmt_ts_open(struct inode *inode, struct file *filp)
{
    int ret = 0;
    return ret;
}

static int wmt_ts_close(struct inode *inode, struct file *filp)
{
    return 0;
}

static unsigned int wmt_ts_poll(struct file *filp, struct poll_table_struct *wait)
{
    return 0;
}


void read_diff(rawdata* pdata)
{
    //u16 Buffer[COL_NUM_MAX*ROW_NUM_MAX*2];	
    u16 *Buffer;
    u16 idx = 0;
    int i,j;
    u16 addr =0;
    pdata->col = COL_NUM;
    pdata->row = ROW_NUM;
	Buffer = kzalloc(COL_NUM_MAX*ROW_NUM_MAX*2*2, GFP_KERNEL);
	if (!Buffer) {
		errlog("mem alloc fail.\n");
		return;
    }
    for(i=0;i<pdata->row;i++)
    {
        addr = 0x42f2+i*60;
        printk("read_diff: addr=0x%04x\n",addr);
        for(j=0;j<pdata->col*2;j++)
        {
            if(lw86x0_read_reg(addr+j, &Buffer[idx],1)!=0)
            {
                lw86x0_read_reg(addr+j, &Buffer[idx],1);
            }
            printk("read_diff: Buffer[%d]=0x%04x\n",idx,Buffer[idx]);
            idx++;
        }
    }
    for(i=0; i<pdata->col * pdata->row; i++)
    {
        pdata->data[i] = ((Buffer[i*2]<<8)&0xff00)|(Buffer[i*2+1]&0x00ff);
        printk("read_diff: pdata->data[%d]=0x%04x\n",i,pdata->data[i]);
    }
	kfree(Buffer);
}

void read_cdc(rawdata* pdata)
{	
    int i,j;
    u16 addr = 0x2fc8;
    u16 idx = 0;

    pdata->col = COL_NUM;
    pdata->row = ROW_NUM;

    for(i=0;i<pdata->col;i++)
    {
        for(j=0;j<pdata->row;j++)
        {
            printk("read_cdc: addr=0x%04x\n",addr+idx);
            if(lw86x0_read_reg(addr+idx, &pdata->data[j*pdata->col+i],1)!=0)
            {
                lw86x0_read_reg(addr+idx, &pdata->data[j*pdata->col+i],1);
            }
            printk("read_cdc: pdata->data[%d]=0x%04x\n",j*pdata->col+i,pdata->data[j*pdata->col+i]);
            idx++;
        }
    }
}

void read_amb(rawdata* pdata)
{	
    int i,j;
    u16 addr = 0x2E04;
    u16 idx = 0;

    pdata->col = COL_NUM;
    pdata->row = ROW_NUM;

    for(i=0;i<pdata->col;i++)
    {
        for(j=0;j<pdata->row;j++)
        {
            printk("read_amb: addr=0x%04x\n",addr+idx);
            if(lw86x0_read_reg(addr+idx, &pdata->data[j*pdata->col+i],1)!=0)
            {
                lw86x0_read_reg(addr+idx, &pdata->data[j*pdata->col+i],1);
            }
            printk("read_amb: pdata->data[%d]=0x%04x\n",j*pdata->col+i,pdata->data[j*pdata->col+i]);
            idx++;
        }
    }

    
}

void lw86x0_flash_write_prepare(void)
{
    lw86x0_stop_timer(1);
    lw86x0_write_reg(0x00e2, 0x0300);//#write_en=1 tmr=1
    udelay(1);
    //mass erase
    lw86x0_write_reg(0x00e2, 0x0305);//#xe=1 mas1=1
    lw86x0_write_reg(0x00e2, 0x0315);//#erase=1
    udelay(5);
    lw86x0_write_reg(0x00e2, 0x0395);//#nvstr=1
    mdelay(20);
    lw86x0_write_reg(0x00e2, 0x0385);//#erase=0
    udelay(100);
    lw86x0_write_reg(0x00e2, 0x0305);//#nvstr=0 
    lw86x0_write_reg(0x00e2, 0x0300);//#xe=0 mas1=0   
}

void lw86x0_flash_write(u8* pbData,u16 start_addr, u16 num)
{
    u16 yaddr = start_addr;
    u16 xaddr = (start_addr)&0xFF80;//x addr is a 8bit address
    u16 cnt = 0;
    while(cnt<num)
    {
        while(1)
        {
            lw86x0_write_reg(0x00e3, xaddr);//#xaddr
            lw86x0_write_reg(0x00e2, 0x0304);//#xe=1
            lw86x0_write_reg(0x00e2, 0x0324);//#prog=1
            udelay(5);
            lw86x0_write_reg(0x00e2, 0x03a4);//#nvstr=1
            udelay(10);
            do
            {
                u16 data = pbData[cnt];
                lw86x0_write_reg(0x00e3, yaddr);//#yaddr
                lw86x0_write_reg(0x00e4, data);//#din
                lw86x0_write_reg(0x00e2, 0xfbac);//#ye=0
                lw86x0_write_reg(0x00e2, 0x03a4);//#ye=0
                yaddr++;
                cnt++;
                if(cnt==num)
                {
                    break;
                }
            }while(yaddr&0x007F);
            xaddr+=0x0080;
            udelay(20);
            lw86x0_write_reg(0x00e2, 0x0384);//#prog=0
            udelay(100);
            lw86x0_write_reg(0x00e2, 0x0304);//#nvstr=0
            lw86x0_write_reg(0x00e2, 0x0300);//#xe=0
            if(cnt==num)
            {
                break;
            }
        }
    }

}

void lw86x0_flash_write_finish(u16 total_len)
{
    //write length of FW to last 2 byte of flash
    u8 *pLen = (u8 *)&total_len;
    lw86x0_flash_write(&(pLen[1]),0x7FFE, 1);
    lw86x0_flash_write(&(pLen[0]),0x7FFF, 1);

    lw86x0_write_reg(0x00e2, 0x0300);//#tmr=1
    lw86x0_write_reg(0x00e2, 0x0200);//#tmr=0
    lw86x0_write_reg(0x00e2, 0x02c0);//#nvstr=1 se=1
    lw86x0_write_reg(0x00e2, 0x02eb);//#prog=1 ifren=1 ye=1 mas=1
    lw86x0_write_reg(0x00e2, 0x03eb);//#tmr=1
    lw86x0_write_reg(0x00e2, 0x02eb);//#tmr=0
    lw86x0_write_reg(0x00e2, 0x02c0);//#prog=0 ifren=0 ye=0 mas=0
    lw86x0_write_reg(0x00e2, 0x0200);//#nvstr=0 se=0
    lw86x0_write_reg(0x00e2, 0x0204);//#xe=1
    lw86x0_write_reg(0x00e2, 0x0214);//#erase=1
    udelay(5);
    lw86x0_write_reg(0x00e2, 0x0294);//#nvstr=1
    mdelay(20);
    lw86x0_write_reg(0x00e2, 0x0284);//#erase=0
    udelay(5);
    lw86x0_write_reg(0x00e2, 0x0204);//#nvstr=0
    lw86x0_write_reg(0x00e2, 0x0200);//#xe=0 
    lw86x0_write_reg(0x00e2, 0x0000); 
    lw86x0_write_reg(0x000c, 0xffff); 
    lw86x0_stop_timer(0);
}

void lw86x0_flash_read(u8* pbData, u16 start_addr, u16 num)
{
    lw86x0_stop_timer(1);
    u16 cnt;
    u16 rd_data = 0;
    u8 *buf;
    for(cnt=0; cnt<num;cnt++)
    {
        lw86x0_write_reg(0x00e4, 0x0000);//#read data
        lw86x0_write_reg(0x00e2, 0x0300);//#tmr=1
        lw86x0_write_reg(0x00e3, start_addr+cnt);
        lw86x0_write_reg(0x00e2, 0x034c);//#se=1 ye=1 xe=1       
        lw86x0_write_reg(0x00e2, 0x0300);//#se=1 ye=1 xe=1
        lw86x0_read_reg(0x00e4, &rd_data,1);
        buf = (u8 *)&rd_data;
        pbData[cnt] = buf[1];        
    }
    lw86x0_stop_timer(0);
}


static long wmt_ts_ioctl(/*struct inode * node,*/ struct file *dev, unsigned int cmd, unsigned long arg)
{
    reg_word regword;
    //rawdata rdata;
    rawdata *rdata;
    flash_op f_pkt;//flash packet    

    int ret = 0;
    char ch;
    rdata = kzalloc(sizeof(rawdata), GFP_KERNEL);
    if (!rdata) {
		errlog("mem alloc fail.\n");
		return -ENOMEM;
    }
	
    if (_IOC_TYPE(cmd) != TS_IOC_MAGIC){ 
        dbg("CMD ERROR!");
        return -ENOTTY;
    }
	
    if (_IOC_NR(cmd) > TS_IOC_MAXNR){ 
        dbg("NO SUCH IO CMD!\n");
        return -ENOTTY;
    }

    switch (cmd) {	
        case LW86X0_WRITE_REG:
            copy_from_user(&regword, (reg_word*)arg, sizeof(regword));
            dbg("write reg[%d] word value 0x%x", regword.uOffset, regword.uValue );
            ret = lw86x0_write_reg(regword.uOffset, regword.uValue);
            if (ret != 0)
            {
                dbg("Faied to write reg. ret 0x%x\n",ret);        
            }
            return 0;
        case LW86X0_READ_REG:
            copy_from_user(&regword, (reg_word*)arg, sizeof(regword));
            ret = lw86x0_read_reg(regword.uOffset, &regword.uValue, 1);
            if (ret != 0)
            {
                dbg("Faied to read reg. ret 0x%x\n",ret);        
            }
            else
            {       
                
                dbg("read reg[%d]=0x%04x",regword.uOffset, regword.uValue);
            }
            copy_to_user((unsigned int*)arg, &regword, sizeof(regword));
            return 0;  
        case LW86X0_CTRL_DEBUG_MODE:
            copy_from_user(&ch, (char*)arg, sizeof(char));
            printk("LW86X0_CTRL_DEBUG_MODE,%c", ch);
            if(ch=='1')
            {
                g_dbgmode = 1;
                wmt_disable_gpirq(irq_gpio);
            }
            else
            {
                g_dbgmode = 0;
                wmt_enable_gpirq(irq_gpio);
            }
            return 0;   
		/*
        case LW86X0_CTRL_RD_DIFF:
            copy_from_user(&rdata, (rawdata*)arg, sizeof(rdata));
            printk("tpd-ioctrl: LW86X0_CTRL_RD_DIFF\n");
            read_diff(&rdata);            
            copy_to_user((unsigned int*)arg, &rdata, sizeof(rdata));
            return 0;
        case LW86X0_CTRL_RD_CDC:
            copy_from_user(&rdata, (rawdata*)arg, sizeof(rdata));
            printk("tpd-ioctrl: LW86X0_CTRL_RD_CDC\n");
            read_cdc(&rdata);            
            copy_to_user((unsigned int*)arg, &rdata, sizeof(rdata));
            return 0;
        case LW86X0_CTRL_RD_AMB:
            copy_from_user(&rdata, (rawdata*)arg, sizeof(rdata));
            printk("tpd-ioctrl: LW86X0_CTRL_RD_AMB\n");
            read_amb(&rdata);            
            copy_to_user((unsigned int*)arg, &rdata, sizeof(rdata));
            return 0;
		*/
		case LW86X0_CTRL_RD_DIFF:
            copy_from_user(rdata, (rawdata*)arg, sizeof(*rdata));
            printk("tpd-ioctrl: LW86X0_CTRL_RD_DIFF\n");
            read_diff(rdata);            
            copy_to_user((unsigned int*)arg, rdata, sizeof(*rdata));
            return 0;
        case LW86X0_CTRL_RD_CDC:
            copy_from_user(rdata, (rawdata*)arg, sizeof(*rdata));
            printk("tpd-ioctrl: LW86X0_CTRL_RD_CDC\n");
            read_cdc(rdata);            
            copy_to_user((unsigned int*)arg, rdata, sizeof(*rdata));
            return 0;
        case LW86X0_CTRL_RD_AMB:
            copy_from_user(rdata, (rawdata*)arg, sizeof(*rdata));
            printk("tpd-ioctrl: LW86X0_CTRL_RD_AMB\n");
            read_amb(rdata);            
            copy_to_user((unsigned int*)arg, rdata, sizeof(*rdata));
            return 0;
        case LW86X0_FLASH_DOWNLOAD:
            copy_from_user(&f_pkt, (flash_op*)arg, sizeof(f_pkt));
            if(f_pkt.startaddr==0)
            {
                lw86x0_flash_write_prepare();                
            }
            lw86x0_flash_write(f_pkt.data, 
                               f_pkt.startaddr, 
                               f_pkt.pktlen);
            printk("dnload: start addr = %04x\n",f_pkt.startaddr);
            if(f_pkt.lastpkt==1)
            {
                u16 write_len = f_pkt.startaddr + f_pkt.pktlen;                
                lw86x0_flash_write_finish(write_len);                
            }
            return 0;
        case LW86X0_FLASH_UPLOAD:
            copy_from_user(&f_pkt, (flash_op*)arg, sizeof(f_pkt));
            lw86x0_flash_read(f_pkt.data, f_pkt.startaddr, f_pkt.pktlen);
            printk("upload: start addr = %04x\n",f_pkt.startaddr);
            printk("\n");            
            copy_to_user((int*)arg, &f_pkt, sizeof(f_pkt));
            return 0;     
        case LW86X0_CTRL_STOP_TIMER:       
            copy_from_user(&ch, (char*)arg, sizeof(char));
            if(ch == '1')
                lw86x0_stop_timer(1);
            else
                lw86x0_stop_timer(0);
            return 0;
	}
	kfree(rdata);
	return -EINVAL;
}

static ssize_t wmt_ts_read(struct file *filp, char *buf, size_t count, loff_t *l)
{
	return 0;
}


static struct file_operations wmt_ts_fops = {
    .read           = wmt_ts_read,
    .poll           = wmt_ts_poll,
    .unlocked_ioctl = wmt_ts_ioctl,
    .open           = wmt_ts_open,
    .release        = wmt_ts_close,
};

static int ts_writeproc( struct file   *file,
                         const char    *buffer,
                         unsigned long count,
                         void          *data )
{
    int calibrate = 0;
    int val = 0;

    if (sscanf(buffer, "calibrate=%d\n", &calibrate))
    {
        if (1 == calibrate)
        {
            if((l_tsdev->capacitance_calibrate != NULL) && 
	       (0 == l_tsdev->capacitance_calibrate()))
            {
                printk(KERN_ALERT "%s calibration successfully!\n", l_tsdev->ts_id);
            } else {
                printk(KERN_ALERT "%s calibration failed!\n", l_tsdev->ts_id);
            }
       }
    } else if (sscanf(buffer, "out=%d\n", &val))
    {
        switch(val)
        {
            case 1: // reset1
                REG32_VAL(__GPIO_BASE+0x0040) |= (1<<irq_gpio); //enable gpio
                REG32_VAL(__GPIO_BASE+0x00C0) |= (1<<irq_gpio); //out high
                REG32_VAL(__GPIO_BASE+0x0080) |= (1<<irq_gpio); //set input
                break;
            case 0: // reset2
                REG32_VAL(__GPIO_BASE+0x0040) |= (1<<irq_gpio); //enable gpio
                REG32_VAL(__GPIO_BASE+0x00C0) &= ~(1<<irq_gpio); //out high
                REG32_VAL(__GPIO_BASE+0x0080) |= (1<<irq_gpio); //set input
                break;
            default:
                break;
        };
    }
    return count;
}

static int ts_readproc(char *page, char **start, off_t off,
                       int count, int *eof, void *data)
{
    int len = 0;

    len = sprintf(page, 
                  "echo calibrate=1 > /proc/wmtts_config   to calibrate ts.\n");
    return len;
}

unsigned char wmt_ts_get_i2caddr(void)
{
    return ts_i2c_addr;
}

static int wmt_check_touch_env(void)
{
    int ret = 0;
    int len = 127;
    char retval[128] = {0},*p=NULL,*s=NULL;
    int Enable=0;

    // Get u-boot parameter
    ret = wmt_getsyspara("wmt.io.touch", retval, &len);
    if(ret){
    	errlog("Read wmt.io.touch Failed.\n");
    	return -EIO;
    }
    
    //check touch enable	
    p = retval;
    sscanf(p,"%d:", &Enable);
    if(Enable == 0){
    errlog("Touch Screen Is Disabled.\n");
    return -ENODEV;
    }

    //check touch IC name
    p = strchr(p,':');p++;
    if (strncmp(p, l_tp[0].name, strlen(l_tp[0].name))) {
    	errlog("Can't find %s!\n", l_tp[0].name);
    	return -ENODEV;
    }
    
    //get firmware file name
    s = strchr(p,':');
    memset(l_tp[0].name,0x00,sizeof(l_tp[0].name));
    strncpy(l_tp[0].name, p, (s-p));
    dbg("ts_fwname=%s\n", l_tp[0].name);

    p = s + 1;
    ret = sscanf(p,"%d:%d:%d:%d:%d:%d:%d:%d:%x",
    		&irq_gpio,&panelres_x,&panelres_y,&rst_gpio,
    		&(l_tp[0].xaxis),&(l_tp[0].xdir),&(l_tp[0].ydir),
    		&(l_tp[0].finger_num),&(l_tp[0].i2caddr));

    dbg("%d;%d;%d;%d;%d;%d;%d;%d;%x;",irq_gpio,panelres_x,panelres_y,rst_gpio,
    		(l_tp[0].xaxis),(l_tp[0].xdir),(l_tp[0].ydir),
    		(l_tp[0].finger_num),(l_tp[0].i2caddr));

	ret = wmt_getsyspara("wmt.display.fb0", retval, &len);
    if (!ret) {
		int tmp[6];
	    p = retval;
		sscanf(p, "%d:[%d:%d:%d:%d:%d", &tmp[0], &tmp[1], &tmp[2], &tmp[3], &tmp[4], &tmp[5]);
		if (tmp[4] > tmp[5])
		    lcd_exchg = 1;
    }

    return 0;
}
//#if 0
//add by jackie i2c_board_info
struct i2c_board_info ts_i2c_board_info = {
	.type          = WMT_TS_I2C_NAME,
	.flags         = 0x00,
	//.addr          = 0x18,   //WMT_TS_I2C_ADDR,//why error?
	.platform_data = NULL,
	.archdata      = NULL,
	.irq           = -1,
}; 
//add jackie static 
 int ts_i2c_register_device (void)
{
	struct i2c_board_info *ts_i2c_bi;
	struct i2c_adapter *adapter = NULL;
	//struct i2c_client *client   = NULL;

	ts_i2c_board_info.addr =  l_tp[0].i2caddr;
	ts_i2c_bi = &ts_i2c_board_info;
	adapter = i2c_get_adapter(1);/*in bus 1*/

	if (NULL == adapter) {
		printk("can not get i2c adapter, client address error\n");
		return -1;
	}
	l_client = i2c_new_device(adapter, ts_i2c_bi);
	if (l_client == NULL) {
		printk("allocate i2c client failed\n");
		return -1;
	}
	i2c_put_adapter(adapter);
	return 0;
}
//add by jackie
struct i2c_client* ts_get_i2c_client(void)
{
	return l_client;
}
//add
static int __init wmt_ts_init(void)
{
    int ret = 0;
  
    if(wmt_check_touch_env())
        return -ENODEV;
		
//add by jackie
  	if (ts_i2c_register_device()<0)
	{
		dbg("Error to run ts_i2c_register_device()!\n");
		return -1;
	}

    mutex_init(&cal_mutex);
	
    if (l_tsdev->init() < 0){
        printk(KERN_ERR "Errors to init %s ts IC!!!\n", l_tsdev->ts_id);
        return -1;
    }
    if (wmt_ts_enable_keyled())
        wmt_ts_init_light();
     // Create device node
    if (register_chrdev (TS_MAJOR, TS_NAME, &wmt_ts_fops)) {
        printk (KERN_ERR "wmt touch: unable to get major %d\n", TS_MAJOR);
        return -EIO;
    }	
	
    l_dev_class = class_create(THIS_MODULE, TS_NAME);
    if (IS_ERR(l_dev_class)){
        ret = PTR_ERR(l_dev_class);
        printk(KERN_ERR "Can't class_create touch device !!\n");
        return ret;
    }
    l_clsdevice = device_create(l_dev_class, NULL, MKDEV(TS_MAJOR, 0), NULL, TS_NAME);
    if (IS_ERR(l_clsdevice)){
        ret = PTR_ERR(l_clsdevice);
        printk(KERN_ERR "Failed to create device %s !!!",TS_NAME);
        return ret;
    }
	
    // register device and driver of platform
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

    klog("wmt ts driver init ok!\n");
    return ret;
}

//add by jackie
static void ts_i2c_unregister_device(void)
{
	if (l_client != NULL)
	{
		i2c_unregister_device(l_client);
		l_client = NULL;
	}
}
//add end

static void __exit wmt_ts_exit(void)
{
    dbg("%s\n",__FUNCTION__);

    if (wmt_ts_enable_keyled())
        wmt_ts_remove_light();
    l_tsdev->exit();
    mutex_destroy(&cal_mutex);
    platform_driver_unregister(&wmt_ts_plt_driver);
    platform_device_unregister(&wmt_ts_plt_device);
    device_destroy(l_dev_class, MKDEV(TS_MAJOR, 0));
    unregister_chrdev(TS_MAJOR, TS_NAME);
    class_destroy(l_dev_class);
	ts_i2c_unregister_device();
}


module_init(wmt_ts_init);
module_exit(wmt_ts_exit);

MODULE_LICENSE("GPL");


#include <linux/unistd.h>
#include <linux/time.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
//#include <asm/semaphore.h>
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
#include <linux/i2c.h>
#include <linux/irq.h>

#include <asm/uaccess.h>
#include <linux/fs.h>
#include <linux/syscalls.h>


#include "wmt_ts.h"
#include "ssd253x-ts.h"

/////////////////////////////////////////////////////////////////

// commands for ui
#define TS_IOC_MAGIC  't'

#define TS_IOCTL_CAL_START    _IO(TS_IOC_MAGIC,   1)
#define TS_IOCTL_CAL_DONE     _IOW(TS_IOC_MAGIC,  2, int*)
#define TS_IOCTL_GET_RAWDATA  _IOR(TS_IOC_MAGIC,  3, int*)
#define TS_IOCTL_CAL_QUIT	_IOW(TS_IOC_MAGIC,  4, int*)
#define TS_IOCTL_AUTO_CALIBRATION	_IOW(TS_IOC_MAGIC,  5, int*)
#define TS_IOC_MAXNR          5

//
#define TS_MAJOR                	11
#define TS_DRIVER_NAME	 	"wmtts_touch"
#define TS_NAME          	 	"wmtts"
#define WMTTS_PROC_NAME     "wmtts_config"

#define EXT_GPIO0   	0
#define EXT_GPIO1   	1
#define EXT_GPIO2   	2
#define EXT_GPIO3   	3
#define EXT_GPIO4	4
#define EXT_GPIO5	5
#define EXT_GPIO6  	6
#define EXT_GPIO7   	7

typedef struct {
    int   a1;
    int   b1;
    int   c1;
    int   a2;
    int   b2;
    int   c2;
    int   delta;
}CALIBRATION_PARAMETER, *PCALIBRATION_PARAMETER;


static int lcd_exchg = 0;
static int irq_gpio;
static int rst_gpio;
static int panelres_x;
static int panelres_y;
static int l_xaxis=0;
static int l_xdirect=1;
static int l_yaxis=1;
static int l_ydirect=1;
static int l_cutedge=-1;
static DECLARE_WAIT_QUEUE_HEAD(queue);
static CALIBRATION_PARAMETER g_CalcParam;
static TS_EVENT g_evLast;
static struct mutex cal_mutex;
static DECLARE_WAIT_QUEUE_HEAD(ts_penup_wait_queue);

extern struct wmtts_device raysen_tsdev;
static struct wmtts_device* l_tsdev = &raysen_tsdev;
static struct i2c_client *l_client=NULL;
static int l_penup = 0; // 1-pen up,0-pen down
static char l_firmid[21];

/////////////////////////////////////////////////////
//    function declare
/////////////////////////////////////////////////////
extern int wmt_getsyspara(char *varname, unsigned char *varval, int *varlen);
extern int wmt_setsyspara(char *varname, unsigned char *varval);

///////////////////////////////////////////////////////////////////////
void TouchPanelCalibrateAPoint(
    int   UncalX,     //@PARM The uncalibrated X coordinate
    int   UncalY,     //@PARM The uncalibrated Y coordinate
    int   *pCalX,     //@PARM The calibrated X coordinate
    int   *pCalY      //@PARM The calibrated Y coordinate
    )
{
	int   x, y;
	mutex_lock(&cal_mutex);
    	x = (g_CalcParam.a1 * UncalX + g_CalcParam.b1 * UncalY +
         	g_CalcParam.c1) / g_CalcParam.delta;
    	y = (g_CalcParam.a2 * UncalX + g_CalcParam.b2 * UncalY +
         	g_CalcParam.c2) / g_CalcParam.delta;

//klog("afer(%d,%d)(%d,%d)\n", x,y,panelres_x,panelres_y);
    	if ( x < 0 )
       	 x = 0;

    	if ( y < 0 )
        	y = 0;
        if (x >= panelres_x)
        	x = panelres_x-1;
        if (y >= panelres_y)
        	y = panelres_y-1;

    	*pCalX = x;
    	*pCalY = y;
	mutex_unlock(&cal_mutex);
	return;
}

static int parse_firmwarefile(const char* filedata, struct ChipSetting** firmarr, int maxlen)
{
	char endflag[]="/* End flag */";
	const char* p = filedata;	
	int val[4];
	int i = 0;
	int j = 0;
	const char* s = NULL;

	// the first {
	while (*p!='{') p++; 
	p++;
	s = p;
	// calculate the number of array
	while (strncmp(p,endflag,strlen(endflag)))
	{
		if (*p=='{')
		{
			i++;
		}
		p++;
	};
	dbg("the number of arry:0x%x\n", i);
	// alloc the memory for array
	*firmarr = kzalloc(sizeof(struct ChipSetting)*i, GFP_KERNEL);
	// parse the value of array
	p = s;
	j = 0;
	while (strncmp(p,endflag,strlen(endflag)))
	{
		if (*p=='{')
		{
			memset(val,0,sizeof(val));
			sscanf(p,"{%x,%x,%x,%x}",val,val+1,val+2,val+3);
			(*firmarr)[j].No = val[0]&0x00FF;
			(*firmarr)[j].Reg = val[1]&0x00FF;
			(*firmarr)[j].Data1 = val[2]&0x00FF;
			(*firmarr)[j].Data2 = val[3]&0x00FF;
			dbg("arry[0x%x]:%x,%x,%x,%x\n",j,(*firmarr)[j].No,(*firmarr)[j].Reg,(*firmarr)[j].Data1,
			                   (*firmarr)[j].Data2);
			j++;
		}
		//p = strchr(p,'}');
		p++;
		if (j>=i-2)
		{
			dbg("%s",p);
		}
		
	};
	if (i != j)
	{
		errlog("Error parsing file(the number of arry not match)!\n");
		return -1;
	};
	dbg("paring firmware file end.\n");
	return i;
}


static struct device* get_tp_device(void){
	if(l_client == NULL){
		errlog("l_client is NULL\n");
	}
	return &l_client->dev;
}


//filepath:the path of firmware file;
//firmdata:store the data from firmware file;
//maxlen: the max len of firmdata;
//return:the number of firmware data,negative-parsing error.
int read_firmwarefile(char* filepath, struct ChipSetting** firmdata, int maxlen)
{
    const u8 *data = NULL;
    int i = 0;
	int ret = -1;
	const struct firmware* tpfirmware = NULL;
	
    klog("ts config file:%s\n",filepath);


	ret = request_firmware(&tpfirmware, filepath, get_tp_device());
	if (ret < 0) {
		errlog("Failed load tp firmware: %s  ret=%d\n", filepath,ret);
		goto err_end;
	}	

	data = tpfirmware->data;
	
    i = parse_firmwarefile(data,firmdata,maxlen);
    if (i <= 0)
    {
    	errlog("error to parse firmware file.\n");
		ret = -1;
    	goto error_parse_fw;
    }
	ret = i;
	
	
    dbg("success to read firmware file!\n");;

error_parse_fw:
	if(tpfirmware){
		release_firmware(tpfirmware);
		tpfirmware = NULL;
	}
err_end:
	return ret;
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

int wmt_ts_get_xaxis(void)
{
	return l_xaxis;
}

int wmt_ts_get_xdir(void)
{
	return l_xdirect;
}

int wmt_ts_get_yaxis(void)
{
	return l_yaxis;
}

int wmt_ts_get_ydir(void)
{
	return l_ydirect;
}

int wmt_ts_get_cutedge(void)
{
	return l_cutedge;
}

void wmt_ts_get_firmwname(char* firmname)
{
	sprintf(firmname,"ssd253x_%s_cfg.tpf",l_firmid);
}

int wmt_ts_get_fingernum(void)
{
	if (!strcmp(l_firmid,"10rs10f1609043psy1"))
	{
		return 10;
	}
	return 5;
}

//up:1-pen up,0-pen down
void wmt_ts_set_penup(int up)
{
	l_penup = up;
}

//
int wmt_ts_wait_penup(void)
{
	int ret = wait_event_interruptible(
			ts_penup_wait_queue,
			(1==l_penup));
	return ret;
}

// return:1-pen up,0-pen dwon
int wmt_ts_ispenup(void)
{
	return l_penup;
}


void wmt_ts_wakeup_penup(void)
{
	wake_up(&ts_penup_wait_queue);
}

int wmt_is_tsirq_enable(void)
{
	int val = 0;
	int num = irq_gpio;
	
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

int wmt_is_tsint(void)
{
	int num = irq_gpio;
	
	if (num > 11)
	{
		return 0;
	}
	return (REG32_VAL(__GPIO_BASE+0x0360) & (1<<num)) ? 1: 0; 
}

void wmt_clr_int(void)
{
	int num = irq_gpio;
	
	if (num > 11)
	{
		return;
	}
	REG32_VAL(__GPIO_BASE+0x0360) = 1<<num;
}

void wmt_tsreset_init(void)
{
	int num = rst_gpio;
	
	REG32_VAL(__GPIO_BASE+0x0040) |= (1<<num);//&= ~(1<<num); //enable gpio
	REG32_VAL(__GPIO_BASE+0x00C0) &= ~(1<<num); // out low
	REG32_VAL(__GPIO_BASE+0x0080) |= (1<<num); //output enable
	msleep(10);
	REG32_VAL(__GPIO_BASE+0x00C0) |= (1<<num); // out high
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

void wmt_set_intasgp(void)
{
	REG32_VAL(__GPIO_BASE+0x0040) |= (1<<irq_gpio); //enable gpio
}

// val:1--high,0-low
void wmt_intgp_out(int val)
{
	if (val)
	{
		REG32_VAL(__GPIO_BASE+0x00C0) |= (1<<irq_gpio); // high
	} else {
		REG32_VAL(__GPIO_BASE+0x00C0) &= ~(1<<irq_gpio); // low
	}
	REG32_VAL(__GPIO_BASE+0x0080) |= (1<<irq_gpio); //set output
}

void wmt_ts_set_irqinput(void)
{
	int num = irq_gpio;
	
	REG32_VAL(__GPIO_BASE+0x0040) |= (1<<num); //enable gpio
	REG32_VAL(__GPIO_BASE+0x0080) &= ~(1<<num); //set input
}

unsigned int wmt_ts_irqinval(void)
{
	return REG32_VAL(__GPIO_BASE+0x0000)&(1<<irq_gpio);
}

int wmt_set_gpirq(int type) 
{
	int shift;
	int offset;
	unsigned long reg;
	int num = irq_gpio;
	
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
		case IRQ_TYPE_LEVEL_LOW:
			reg &= ~(1<<(shift*8+2)); 
			reg &= ~(1<<(shift*8+1));
			reg &= ~(1<<(shift*8));
			break;
		case IRQ_TYPE_LEVEL_HIGH:
			reg &= ~(1<<(shift*8+2)); 
			reg &= ~(1<<(shift*8+1));
			reg |= (1<<(shift*8));
			break;
		case IRQ_TYPE_EDGE_FALLING:
			reg &= ~(1<<(shift*8+2)); 
			reg |= (1<<(shift*8+1));
			reg &= ~(1<<(shift*8));
			break;
		case IRQ_TYPE_EDGE_RISING:
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

int wmt_enable_gpirq(void)
{
	int num = irq_gpio;
	
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

int wmt_disable_gpirq(void)
{
	int num = irq_gpio;
	
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
	return l_tsdev->suspend(pdev, state);
}
static int wmt_ts_resume(struct platform_device *pdev)
{
	dbg("ts resume....\n");
	return l_tsdev->resume(pdev);
}

static int wmt_ts_probe(struct platform_device *pdev)
{

	if (l_tsdev->probe != NULL)
		return l_tsdev->probe(pdev);
	else 
		return 0;
}

static int wmt_ts_remove(struct platform_device *pdev)
{

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
	.probe = wmt_ts_probe,
	.remove = wmt_ts_remove,
	.suspend        = wmt_ts_suspend,
	.resume         = wmt_ts_resume,
};

static int wmt_check_touch_env(void)
{
	int ret = 0;
	int len = 150;
    char retval[150] = {0},*p=NULL;
	int Enable=0,Gpio=0,PX=0,PY=0;
	char* s=NULL;
		
    	// Get u-boot parameter
	ret = wmt_getsyspara("wmt.io.touch", retval, &len);
	if(ret){
		errlog("Read wmt.io.touch Failed.\n");
		return -EIO;
	}
	sscanf(retval,"%d:",&Enable);
	//check touch enable
	if(Enable == 0){
		errlog("Touch Screen Is Disabled.\n");
		return -ENODEV;
	}

	p = strchr(retval,':');
	p++;
	if(strncmp(p, l_tsdev->ts_id,strlen(l_tsdev->ts_id))){//check touch ID
		errlog("[WMTENV] %s is not found\n", l_tsdev->ts_id);
		return -ENODEV;
	}
	// get firmwareid
	s = p+strlen(l_tsdev->ts_id)+1; //point to firmware id	
	p = strchr(p,':');
	memset(l_firmid,0,sizeof(l_firmid));
	len = p-s;
	if (len>=20)
	{
		len = 19;
	}
	strncpy(l_firmid,s,len);
	p++;
	sscanf(p,"%d:%d:%d:%d:%d:%d:%d:%d:%d",&Gpio,&PX,&PY,&rst_gpio,
							&l_xaxis,&l_xdirect,
							&l_yaxis,&l_ydirect,
							&l_cutedge);

	irq_gpio = Gpio;
	panelres_x = PX;
	panelres_y = PY;
	dbg("p.x=%d,p.y=%d,gpio=%d,resetgpio=%d,\nx-axis=%d,x_dir=%d,y-axis=%d,y_dir=%d,cutedge=%d\nfirmwareid:%s\n", 
	          panelres_x, panelres_y, irq_gpio, rst_gpio,
	          l_xaxis,l_xdirect,l_yaxis,l_ydirect,l_cutedge,
	          l_firmid);

	memset(retval,0,sizeof(retval));
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

struct i2c_board_info ts_i2c_board_info = {
	.type          = WMT_TS_I2C_NAME,
	.flags         = 0x00,
	.addr          = WMT_TS_I2C_ADDR,
	.platform_data = NULL,
	.archdata      = NULL,
	.irq           = -1,
};

static int ts_i2c_register_device (void)
{
	struct i2c_board_info *ts_i2c_bi;
	struct i2c_adapter *adapter = NULL;
	//struct i2c_client *client   = NULL;
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

static void ts_i2c_unregister_device(void)
{
	if (l_client != NULL)
	{
		i2c_unregister_device(l_client);
		l_client = NULL;
	}
}

struct i2c_client* ts_get_i2c_client(void)
{
	return l_client;
}

static int __init wmt_ts_init(void)
{
	int ret = 0;
  
	if(wmt_check_touch_env())
		return -ENODEV;

	if (ts_i2c_register_device()<0)
	{
		dbg("Error to run ts_i2c_register_device()!\n");
		return -1;
	}
	mutex_init(&cal_mutex);
	
	if (l_tsdev->init() < 0){
		dbg("Errors to init %s ts IC!!!\n", l_tsdev->ts_id);
		ret = -1;
		goto err_init;
	}
	// Create device node
/*	if (register_chrdev (TS_MAJOR, TS_NAME, &wmt_ts_fops)) {
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
*/	
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

	klog("%s driver init ok!\n",l_tsdev->ts_id);
	return 0;
err_init:
	ts_i2c_unregister_device();
	return ret;
}

static void __exit wmt_ts_exit(void)
{
	dbg("%s\n",__FUNCTION__);
	
	l_tsdev->exit();	
	platform_driver_unregister(&wmt_ts_plt_driver);
	platform_device_unregister(&wmt_ts_plt_device);
	//device_destroy(l_dev_class, MKDEV(TS_MAJOR, 0));
	//unregister_chrdev(TS_MAJOR, TS_NAME);
	//class_destroy(l_dev_class);
	mutex_destroy(&cal_mutex);
	ts_i2c_unregister_device();
}


module_init(wmt_ts_init);
module_exit(wmt_ts_exit);

MODULE_LICENSE("GPL");


#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h> 
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/input.h>
//#include <asm/gpio.h>
#include <asm/irq.h>
#include <linux/irq.h>
#include <asm/io.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <mach/hardware.h>
#include <linux/platform_device.h>
#include <linux/suspend.h>
#include <linux/slab.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif /* CONFIG_HAS_EARLYSUSPEND */


#include "ssd253x-ts.h"
#include "wmt_ts.h"

//#define CONFIG_TOUCHSCREEN_SSL_DEBUG	
#undef  CONFIG_TOUCHSCREEN_SSL_DEBUG

#define DEVICE_ID_REG                    2
#define VERSION_ID_REG                 3
#define AUTO_INIT_RST_REG             68
#define EVENT_STATUS                   121
#define EVENT_MSK_REG                 122
#define IRQ_MSK_REG                     123
#define FINGER01_REG                    124
#define EVENT_STACK                   	 128
#define EVENT_FIFO_SCLR               135
#define TIMESTAMP_REG                 136
#define SELFCAP_STATUS_REG         185		

#define	ON_TOUCH_INT	INT_EI11    //GPIO :set the interrupt 
#define DEVICE_NAME	"touch_ssd253x"
#define SSD253X_I2C_ADDR 0x48 //0x48

// SSD2533 Setting
// Touch Panel Example
static struct ChipSetting* ssd253xcfgTable = NULL;
static int l_cfglen = 0;

static struct ChipSetting ssd253xcfgTable_default[]={
{2,0x06,0x1B,0x28},
{2,0xd7,0x00,0x00},
{2,0xd8,0x00,0x07},
{2,0xdb,0x00,0x01},
{2,0x30,0x03,0x08},
{2,0x34,0xd4,0x1e},
{2,0x57,0x00,0x06},
{2,0x40,0x00,0xc8},
{2,0x41,0x00,0x30},
{2,0x42,0x00,0xc0},
{2,0x43,0x00,0x30},
{2,0x44,0x00,0xc0},
{2,0x45,0x00,0xc0},
{2,0x46,0x00,0x0f},
{2,0x5f,0x00,0x00},
{2,0x2d,0x00,0x00},
{2,0x66,0x1F,0x38},
{2,0x67,0x1c,0x92},
{2,0x25,0x00,0x02},
};


// For SSD2533 Bug Version Only //
//#define	SSD2533FIXEDCODE
 struct ChipSetting ssd253xcfgTable1[]={
{ 1, 0xA4, 0x00, 0x00},			//MCU prescaler default=01
{ 1, 0xD4, 0x08, 0x00},			//Dummy Code
{ 1, 0xD4, 0x08, 0x00},			//Set Osc frequency default=8, range 0 to F
};

 struct ChipSetting Reset[]={
{ 0, 0x04, 0x00, 0x00},	// SSD2533
};

 struct ChipSetting Resume[]={
{ 0, 0x04, 0x00, 0x00},	// SSD2533
{ 1, 0x25, 0x12, 0x00}, // Set Operation Mode   //Set from int setting
};

 struct ChipSetting Suspend[] ={
{ 1, 0x25, 0x00, 0x00}, // Set Operation Mode
{ 0, 0x05, 0x00, 0x00},	// SSD2533
};


#ifdef CONFIG_HAS_EARLYSUSPEND
static void ssd253x_ts_early_suspend(struct early_suspend *h);
static void ssd253x_ts_late_resume(struct early_suspend *h);
#endif /* CONFIG_HAS_EARLYSUSPEND */

static irqreturn_t ssd253x_ts_isr(int irq, void *dev_id);
static enum hrtimer_restart ssd253x_ts_timer(struct hrtimer *timer);
//extern int wmt_i2c_xfer_continue_if_4(struct i2c_msg *msg, unsigned int num,int bus_id);


static int SSDS53X_SCREEN_MAX_X =  800;
static int SSDS53X_SCREEN_MAX_Y =  480;



enum{
	IC_SSD2533 = 1,
	IC_SSD2543,
	IC_SSD2531
};

static int ic_flag;

static struct workqueue_struct *ssd253x_wq;

int Ssd_Timer1,Ssd_Timer2,Ssd_Timer_flag;

struct ssl_ts_priv {
	struct input_dev *input;
	struct hrtimer timer;
	struct work_struct  ssl_work;
#ifdef	CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif 

	int irq;
	int use_irq;
	int FingerNo;
    int earlysus;
    
	int FingerX[FINGERNO];
	int FingerY[FINGERNO];
	int FingerP[FINGERNO];

	int Resolution;
	int EventStatus;
	int FingerDetect;

	int sFingerX[FINGERNO];
	int sFingerY[FINGERNO];
	int pFingerX[FINGERNO];
	int pFingerY[FINGERNO];
};

static struct ssl_ts_priv* l_ts = NULL;
struct wmtts_device ssd253x_tsdev;
static DECLARE_WAIT_QUEUE_HEAD(ts_penup_wait_queue);

#define SD_INIT
#ifdef SD_INIT
#define TP_CHR "tp_chr"

#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>

static long tp_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
static int tp_open(struct inode *inode, struct file *file);
static int tp_release(struct inode *inode, struct file *file);
static ssize_t tp_read(struct file *file, char __user *buf, size_t count,loff_t *offset);
static ssize_t tp_write(struct file *file, const char __user *buf,size_t count, loff_t *offset);

//void InitFromSD(struct i2c_client *client);

//struct ChipSetting _ssd253xcfgTable[200];	
//int sd_init_size=0;


//struct i2c_client *g_tp_client;

#endif



static int ReadRegister(/*struct i2c_client *client,*/uint8_t reg,int ByteNo)
{
	unsigned char buf[4];
	struct i2c_msg msg[2];
	int ret;
	struct i2c_client* client = ts_get_i2c_client();

	memset(buf, 0xFF, sizeof(buf));
	msg[0].addr = SSD253X_I2C_ADDR;
	msg[0].flags = 0 | I2C_M_NOSTART;
	msg[0].len = 1;
	msg[0].buf = &reg;

	msg[1].addr = SSD253X_I2C_ADDR;
	msg[1].flags = I2C_M_RD;
	msg[1].len = ByteNo;
	msg[1].buf = buf;

	ret = i2c_transfer(client->adapter, msg, 2);
	if (ret <= 0)
	{
		printk("read the address (0x%x) of the ssd253x fail, ret=%d.\n", reg, ret);
		return -1;
	}

	if(ByteNo==1) return (int)((unsigned int)buf[0]<<0);
	if(ByteNo==2) return (int)((unsigned int)buf[1]<<0)|((unsigned int)buf[0]<<8);
	if(ByteNo==3) return (int)((unsigned int)buf[2]<<0)|((unsigned int)buf[1]<<8)|((unsigned int)buf[0]<<16);
	if(ByteNo==4) return (int)((unsigned int)buf[3]<<0)|((unsigned int)buf[2]<<8)|((unsigned int)buf[1]<<16)|(buf[0]<<24);
	return 0;
}

static int WriteRegister(/*struct i2c_client *client,*/uint8_t Reg,unsigned char Data1,unsigned char Data2,int ByteNo)
{	
	struct i2c_msg msg;
	unsigned char buf[4];
	int ret;
	struct i2c_client* client = ts_get_i2c_client();

	buf[0]=Reg;
	buf[1]=Data1;
	buf[2]=Data2;
	buf[3]=0;

	msg.addr = SSD253X_I2C_ADDR;
	msg.flags = 0;
	msg.len = ByteNo+1;
	msg.buf = (char *)buf;
	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret <= 0)
	{
		printk(KERN_ERR "write the address (0x%x) of the ssd25xx fail, ret=%d.\n", buf[0], ret);
		return -1;
	}
	return 0;
	
}

int SSD253xdeviceInit1(void) 
{	
#ifdef	SSD2533FIXEDCODE
	int i;
	mdelay(600); //SSD2533 ESD2 EEPROM VERSION
	for(i=0;i<sizeof(ssd253xcfgTable1)/sizeof(ssd253xcfgTable1[0]);i++)
	{
		if (WriteRegister(ssd253xcfgTable1[i].Reg,
				ssd253xcfgTable1[i].Data1,ssd253xcfgTable1[i].Data2,
				ssd253xcfgTable1[i].No))
		{
			return -1;
		}
	}
#endif
	return 0;
}

int SSD253xdeviceInit(void)
{	
	int i;
	
	for(i=0;i<l_cfglen/*sizeof(ssd253xcfgTable)/sizeof(ssd253xcfgTable[0])*/;i++)
	{
		if (WriteRegister(ssd253xcfgTable[i].Reg,
				ssd253xcfgTable[i].Data1,ssd253xcfgTable[i].Data2,
				ssd253xcfgTable[i].No))
		{
			return -1;
		}
		if (0 == i)
		{
			msleep(300);
		}
	}
	msleep(300);
	return 0;
}

int deviceReset(void)
{	
	int i;

	i = 0;//just for remove warning message
	wmt_rst_output(1);
	mdelay(5);
	wmt_rst_output(0);
	mdelay(10);
	wmt_rst_output(1);
	msleep(200);
	
	//if(ic_flag == IC_SSD2533){
	for(i=0;i<sizeof(Reset)/sizeof(Reset[0]);i++)
	{
		if (WriteRegister(Reset[i].Reg,
				Reset[i].Data1,Reset[i].Data2,
				Reset[i].No))
		{
			return -1;
		}
	}
	//}
	
	mdelay(100);
	if (SSD253xdeviceInit1())
	{
		return -1;
	}
	return 0;
}

int deviceResume(void)
{	
	int i;
	for(i=0;i<sizeof(Resume)/sizeof(Resume[0]);i++)
	{
		if (WriteRegister(Resume[i].Reg,
				Resume[i].Data1,Resume[i].Data2,
				Resume[i].No))
		{
			return -1;
		}
		mdelay(100);
	}
	return 0;
}

int deviceSuspend(void)
{	
	int i;
	//int timeout=10;
	//int status;
	
	for(i=0;i<sizeof(Suspend)/sizeof(Suspend[0]);i++)
	{
		if (WriteRegister(Suspend[i].Reg,
				Suspend[i].Data1,Suspend[i].Data2,
				Suspend[i].No))
		{
			return -1;
		}
		mdelay(100);
	}
	return 0;
}

#define Mode RunningAverageMode
#define Dist RunningAverageDist
void RunningAverage(unsigned short *xpos,unsigned short *ypos,int No,struct ssl_ts_priv *ssl_priv)
{	
	int FilterMode[4][2]={{0,8},{5,3},{6,2},{7,1}};
	int dx,dy;
	int X,Y;

	X=*xpos;
	Y=*ypos;
	if((ssl_priv->pFingerX[No]!=0x0FFF)&&(X!=0x0FFF))
	{
		dx=abs(ssl_priv->pFingerX[No]-X);
		dy=abs(ssl_priv->pFingerY[No]-Y);
		if(dx+dy<Dist*64)
		{
			ssl_priv->pFingerX[No]=(FilterMode[Mode][0]*ssl_priv->pFingerX[No]+FilterMode[Mode][1]*X)/8;
			ssl_priv->pFingerY[No]=(FilterMode[Mode][0]*ssl_priv->pFingerY[No]+FilterMode[Mode][1]*Y)/8;
		}
		else
		{
			ssl_priv->pFingerX[No]=X;
			ssl_priv->pFingerY[No]=Y;
		}
	}
	else
	{
		ssl_priv->pFingerX[No]=X;
		ssl_priv->pFingerY[No]=Y;
	}
	*xpos=ssl_priv->pFingerX[No];
	*ypos=ssl_priv->pFingerY[No];
}

void FingerCheckSwap(int *FingerX,int *FingerY,int *FingerP,int FingerNo,int *sFingerX,int *sFingerY)
{
  	int i,j;
  	int index1,index2;
  	int Vx,Vy;
  	int Ux,Uy;
  	int R1x,R1y;
  	int R2x,R2y;
	for(i=0;i<FingerNo;i++)
  	{
 		index1=i;
	    	if( FingerX[index1]!=0xFFF)
		if(sFingerX[index1]!=0xFFF) 
		{
			for(j=i+1;j<FingerNo+3;j++)
			{
				index2=j%FingerNo;
	    			if( FingerX[index2]!=0xFFF)
				if(sFingerX[index2]!=0xFFF) 
		    		{
					Ux=sFingerX[index1]-sFingerX[index2];
					Uy=sFingerY[index1]-sFingerY[index2];      
					Vx= FingerX[index1]- FingerX[index2];
					Vy= FingerY[index1]- FingerY[index2];					

					R1x=Ux-Vx;
					R1y=Uy-Vy;
					R2x=Ux+Vx;
					R2y=Uy+Vy;
							
					R1x=R1x*R1x;
					R1y=R1y*R1y; 
					R2x=R2x*R2x;
					R2y=R2y*R2y;

					if(R1x+R1y>R2x+R2y)
				    	{
				    		Ux=FingerX[index1];
						Uy=FingerY[index1];
						Vx=FingerP[index1];
							          
						FingerX[index1]=FingerX[index2];
						FingerY[index1]=FingerY[index2];
						FingerP[index1]=FingerP[index2];
							
						FingerX[index2]=Ux;
						FingerY[index2]=Uy;
						FingerP[index2]=Vx;
					}
					break;
			    	}
			}
		}
  	}        
  	for(i=0;i<FingerNo;i++)
  	{
    		sFingerX[i]=FingerX[i];
    		sFingerY[i]=FingerY[i];
  	}
}

#ifdef USE_TOUCH_KEY
static void ssd2533_ts_send_keyevent(struct ssl_ts_priv *ssl_priv,u8 btn_status, int downup)
{
	
	switch(btn_status & 0x0f)
	{
		case 0x01:
			input_report_key(ssl_priv->input, KEY_SEARCH, downup);
			break;
		case 0x02:
			input_report_key(ssl_priv->input, KEY_BACK, downup);
			break;
		case 0x04:
			input_report_key(ssl_priv->input, KEY_HOME, downup);
			break;
		case 0x08:
			input_report_key(ssl_priv->input, KEY_MENU, downup);
			break;
		default:
			break;
	}
	dbg("send %x %x\n", btn_status, downup);
}
#endif

// for ssd2533(no test)
static int ssd253x_ts_cut_edge0(unsigned short pos,unsigned short x_y)
{
	u8 cut_value = 26; //26 cut_value < 32
	if(pos == 0xfff)
	{
		return pos;
	}
	//printk("X: rude data %d\n",pos);
	if(x_y) //xpos
	{
	
		if(pos < 16)
			pos = cut_value + pos*(48 - cut_value) / 16;
		else if(pos > (XPOS_MAX - 16) )
			pos = XPOS_MAX + 16 + (pos - (XPOS_MAX -16))*(48 - cut_value) / 16;
		else
			pos = pos + 32;

		pos = SSDS53X_SCREEN_MAX_X * pos / (DRIVENO * 64);
		//printk("X: changed data %d\n",pos);
		return pos;
	}
	else    //ypos
	{
		if(pos < 16)
			pos = cut_value + pos*(48 - cut_value) / 16;
		else if(pos > (YPOS_MAX - 16) )
			pos = YPOS_MAX + 16 + (pos - (YPOS_MAX -16))*(48 - cut_value) / 16;
		else
			pos = pos + 32;
		//printk("Y: rude data %d\n",pos);
		pos = SSDS53X_SCREEN_MAX_Y* pos / (SENSENO * 64);
		//printk("Y: changed data %d\n",pos);
		return pos;		
	}
	
	
}

// for ssd2532
static int ssd253x_ts_cut_edge1(unsigned short pos,unsigned short x_y)
{
	u8 cut_value = 15; //cut_value < 32
	
	if(pos == 0xfff){
		return pos;
	}
    
	if(x_y){ //xpos 64-->96  //MAX=896
		pos = pos + cut_value;//????????Ե
		pos = SSDS53X_SCREEN_MAX_X * pos / (790+cut_value*2);//SSDS53X_SCREEN_MAX_X?????ұ?Ե
		return pos;
	}else{    //ypos  //MAX=576
		pos = pos + cut_value;//?????ϱ?Ե
		pos = SSDS53X_SCREEN_MAX_Y* pos / (470+cut_value*2);//SSDS53X_SCREEN_MAX_Y?????±?Ե
		return pos; 
	}
}

// for ssd2532,8" ssd253x_pydctp80a1.ts
// x_y:1--x,0--y
static int ssd253x_ts_cut_edge2(unsigned short pos,unsigned short x_y)
{
	int tpos;

	if (pos == 0xfff){
		return pos;
	}
    
	tpos = pos;
	if (x_y)
	{
		if (tpos<20)
		{
			tpos= tpos+18;
		} else if (tpos>585)
		{
			tpos = tpos-18;
		} else {
			tpos = (tpos-20)*565/575+30;
		}
		pos = tpos;
		return pos;
	} else {
		if (tpos <10)
		{
			tpos = tpos+10;
		} else if (tpos >795)
		{
			tpos = 795;
		} else {
			tpos = (tpos-10)*775/785+20;
		}
		pos = tpos;
		return pos;
	}	

}
// for ssd2532
static int ssd253x_ts_cut_edge3(unsigned short pos,unsigned short x_y)
{
	u8 cut_value = 15;
	
	if(pos == 0xfff){
		return pos;
	}
    
	if(x_y){ 
		pos = pos + cut_value;
		pos = SSDS53X_SCREEN_MAX_X * pos / (896+cut_value*2);
		return pos;
	}else{ 
		pos = pos + cut_value;
		pos = SSDS53X_SCREEN_MAX_Y* pos / (576+cut_value*2);
		return pos; 
	}
}

// for jun feng TP
static int ssd253x_ts_cut_edge4(unsigned short pos,unsigned short x_y)
{
	unsigned short Cut_Edge_XLeft[64]={
	0x0008,0x0009,0x000B,0x000C,0x000D,0x000E,0x0010,0x0011,
	0x0012,0x0013,0x0015,0x0016,0x0017,0x0018,0x001A,0x001B,
	0x001C,0x001D,0x001F,0x0020,0x0021,0x0022,0x0024,0x0025,
	0x0026,0x0026,0x0027,0x0028,0x0029,0x002A,0x002B,0x002C,
	0x002C,0x002D,0x002E,0x002F,0x0030,0x0031,0x0032,0x0032,
	0x0033,0x0034,0x0035,0x0036,0x0037,0x0038,0x0038,0x0039,
	0x003A,0x003B,0x003C,0x003D,0x003E,0x003E,0x003F,0x0040,
	0x0041,0x0042,0x0043,0x0044,0x0044,0x0045,0x0046,0x0047
	};

	unsigned short Cut_Edge_XRight[64]={
	0x0318,0x0317,0x0315,0x0314,0x0313,0x0312,0x0310,0x030F,
	0x030E,0x030D,0x030B,0x030A,0x0309,0x0308,0x0306,0x0305,
	0x0304,0x0303,0x0301,0x0300,0x02FF,0x02FE,0x02FC,0x02FB,
	0x02FA,0x02FA,0x02F9,0x02F8,0x02F7,0x02F6,0x02F5,0x02F4,
	0x02F4,0x02F3,0x02F2,0x02F1,0x02F0,0x02EF,0x02EE,0x02EE,
	0x02ED,0x02EC,0x02EB,0x02EA,0x02E9,0x02E8,0x02E8,0x02E7,
	0x02E6,0x02E5,0x02E4,0x02E3,0x02E2,0x02E2,0x02E1,0x02E0,
	0x02DF,0x02DE,0x02DD,0x02DC,0x02DC,0x02DB,0x02DA,0x02D9
	};

	unsigned short Cut_Edge_YUp[64]={
	0x0006,0x0007,0x0008,0x000A,0x000B,0x000C,0x000D,0x000F,
	0x0010,0x0011,0x0012,0x0014,0x0015,0x0016,0x0017,0x0018,
	0x001A,0x001B,0x001C,0x001D,0x001F,0x0020,0x0021,0x0022,
	0x0022,0x0023,0x0024,0x0025,0x0025,0x0026,0x0027,0x0028,
	0x0029,0x0029,0x002A,0x002B,0x002C,0x002C,0x002D,0x002E,
	0x002F,0x0030,0x0030,0x0031,0x0032,0x0033,0x0033,0x0034,
	0x0035,0x0036,0x0037,0x0037,0x0038,0x0039,0x003A,0x003A,
	0x003B,0x003C,0x003D,0x003E,0x003E,0x003F,0x0040,0x0041
	};

	unsigned short Cut_Edge_YDown[64]={
	0x01DA,0x01D9,0x01D8,0x01D6,0x01D5,0x01D4,0x01D3,0x01D1,
	0x01D0,0x01CF,0x01CE,0x01CC,0x01CB,0x01CA,0x01C9,0x01C8,
	0x01C6,0x01C5,0x01C4,0x01C3,0x01C1,0x01C0,0x01BF,0x01BE,
	0x01BE,0x01BD,0x01BC,0x01BB,0x01BB,0x01BA,0x01B9,0x01B8,
	0x01B7,0x01B7,0x01B6,0x01B5,0x01B4,0x01B4,0x01B3,0x01B2,
	0x01B1,0x01B0,0x01B0,0x01AF,0x01AE,0x01AD,0x01AD,0x01AC,
	0x01AB,0x01AA,0x01A9,0x01A9,0x01A8,0x01A7,0x01A6,0x01A6,
	0x01A5,0x01A4,0x01A3,0x01A2,0x01A2,0x01A1,0x01A0,0x019F
	};
	int cut_value = 5; //cut_value < 32
	if(pos == 0xfff)
	{
		return pos;
	}
	if(x_y) //xpos
	{
		dbg("X: Raw data %d\n",pos);
		if (pos >=XPOS_MAX)
			{
				pos = XPOS_MAX;
			}
		if (pos<64)
			{
				pos = Cut_Edge_XLeft[pos];		//Left cut edge
			}
		else
		if ((XPOS_MAX - pos) <64)
			{
				pos = Cut_Edge_XRight[XPOS_MAX - pos];		//Right cut edge
			}
		else
			{
				pos = pos + cut_value;		//
				pos = SSDS53X_SCREEN_MAX_X* pos / (790 + cut_value*2);//SSD253X_SCREEN_MAX_X|?????2????????|?
			}
			dbg("X: Cut edge data %d\n",pos);
		return pos;
	}
	else    //ypos
	{

		dbg("Y: Raw data %d\n",pos);
		if (pos >=YPOS_MAX)
			{
				pos = YPOS_MAX;
			}
		if (pos<64)
			{
				pos = Cut_Edge_YUp[pos];		//Up cut edge
			}
		else
		if ((YPOS_MAX - pos) <64)
			{
				pos = Cut_Edge_YDown[YPOS_MAX - pos];		//Down cut edge
			}
		else
			{
				pos = pos + cut_value;		//
				pos = SSDS53X_SCREEN_MAX_Y* pos / (470 + cut_value*2);//SSD253X_SCREEN_MAX_X|?????2????????|?
				//tpos = pos;
				//tpos = /*SSDS53X_SCREEN_MAX_Y*/ (800* pos) / (470 + cut_value*2);
				dbg("XPOS_MAX=%d,\n", XPOS_MAX);
				dbg("YPOS_MAX=%d,\n",YPOS_MAX);
				dbg("Y: Cut edge data pos= %d,tpos=%d\n",pos,tpos);
			}
			
		return pos;
	}


}

static int ssd253x_ts_cut_edge5(unsigned short pos,unsigned short x_y)
{
	unsigned short Cut_Edge_XLeft[64]={
	0x0008,0x0009,0x000B,0x000C,0x000D,0x000E,0x0010,0x0011,
	0x0012,0x0013,0x0015,0x0016,0x0017,0x0018,0x001A,0x001B,
	0x001C,0x001D,0x001F,0x0020,0x0021,0x0022,0x0024,0x0025,
	0x0026,0x0026,0x0027,0x0028,0x0029,0x002A,0x002B,0x002C,
	0x002C,0x002D,0x002E,0x002F,0x0030,0x0031,0x0032,0x0032,
	0x0033,0x0034,0x0035,0x0036,0x0037,0x0038,0x0038,0x0039,
	0x003A,0x003B,0x003C,0x003D,0x003E,0x003E,0x003F,0x0040,
	0x0041,0x0042,0x0043,0x0044,0x0044,0x0045,0x0046,0x0047
	};

	unsigned short Cut_Edge_XRight[64]={
	0x0318,0x0317,0x0315,0x0314,0x0313,0x0312,0x0310,0x030F,
	0x030E,0x030D,0x030B,0x030A,0x0309,0x0308,0x0306,0x0305,
	0x0304,0x0303,0x0301,0x0300,0x02FF,0x02FE,0x02FC,0x02FB,
	0x02FA,0x02FA,0x02F9,0x02F8,0x02F7,0x02F6,0x02F5,0x02F4,
	0x02F4,0x02F3,0x02F2,0x02F1,0x02F0,0x02EF,0x02EE,0x02EE,
	0x02ED,0x02EC,0x02EB,0x02EA,0x02E9,0x02E8,0x02E8,0x02E7,
	0x02E6,0x02E5,0x02E4,0x02E3,0x02E2,0x02E2,0x02E1,0x02E0,
	0x02DF,0x02DE,0x02DD,0x02DC,0x02DC,0x02DB,0x02DA,0x02D9
	};

	unsigned short Cut_Edge_YUp[64]={
	0x0006,0x0007,0x0008,0x000A,0x000B,0x000C,0x000D,0x000F,
	0x0010,0x0011,0x0012,0x0014,0x0015,0x0016,0x0017,0x0018,
	0x001A,0x001B,0x001C,0x001D,0x001F,0x0020,0x0021,0x0022,
	0x0022,0x0023,0x0024,0x0025,0x0025,0x0026,0x0027,0x0028,
	0x0029,0x0029,0x002A,0x002B,0x002C,0x002C,0x002D,0x002E,
	0x002F,0x0030,0x0030,0x0031,0x0032,0x0033,0x0033,0x0034,
	0x0035,0x0036,0x0037,0x0037,0x0038,0x0039,0x003A,0x003A,
	0x003B,0x003C,0x003D,0x003E,0x003E,0x003F,0x0040,0x0041
	};

	unsigned short Cut_Edge_YDown[64]={
	0x01DA,0x01D9,0x01D8,0x01D6,0x01D5,0x01D4,0x01D3,0x01D1,
	0x01D0,0x01CF,0x01CE,0x01CC,0x01CB,0x01CA,0x01C9,0x01C8,
	0x01C6,0x01C5,0x01C4,0x01C3,0x01C1,0x01C0,0x01BF,0x01BE,
	0x01BE,0x01BD,0x01BC,0x01BB,0x01BB,0x01BA,0x01B9,0x01B8,
	0x01B7,0x01B7,0x01B6,0x01B5,0x01B4,0x01B4,0x01B3,0x01B2,
	0x01B1,0x01B0,0x01B0,0x01AF,0x01AE,0x01AD,0x01AD,0x01AC,
	0x01AB,0x01AA,0x01A9,0x01A9,0x01A8,0x01A7,0x01A6,0x01A6,
	0x01A5,0x01A4,0x01A3,0x01A2,0x01A2,0x01A1,0x01A0,0x019F
	};
	u8 cut_value = 20; //cut_value < 32
	if(pos == 0xfff)
	{
		return pos;
	}
	if(x_y) //xpos
	{
		dbg("X: Raw data %d\n",pos);
		if (pos >=XPOS_MAX)
			{
				pos = XPOS_MAX;
			}
		if (pos<64)
			{
				pos = Cut_Edge_XLeft[pos];		//Left cut edge
			}
		else
		if ((XPOS_MAX - pos) <64)
			{
				pos = Cut_Edge_XRight[XPOS_MAX - pos];		//Right cut edge
			}
		else
			{
				pos = pos + cut_value;		//
				pos = SSDS53X_SCREEN_MAX_X * pos / (XPOS_MAX + cut_value*2);//SSD253X_SCREEN_MAX_X|??????????|?			}
			dbg("X: Cut edge data %d\n",pos);
			return pos;
		}
	}
	else    //ypos
	{

			dbg("Y: Raw data %d\n",pos);
		if (pos >=YPOS_MAX)
			{
				pos = YPOS_MAX;
			}
		if (pos<64)
			{
				pos = Cut_Edge_YUp[pos];		//Up cut edge
			}
		else
		if ((YPOS_MAX - pos) <64)
			{
				pos = Cut_Edge_YDown[YPOS_MAX - pos];		//Down cut edge
			}
		else
			{
				pos = pos + cut_value;		//
				pos = SSDS53X_SCREEN_MAX_Y * pos / (YPOS_MAX + cut_value*2);//SSD253X_SCREEN_MAX_X|??????????|?			}
			dbg("Y: Cut edge data %d\n",pos);
		return pos;
		}
	}
	return -1;
}

static int ssd253x_ts_cut_edge6(unsigned short pos,unsigned short x_y)
{

	#define XPOS_MAX_D (DRIVENO -EdgeDisable) *64
	#define YPOS_MAX_D (SENSENO -EdgeDisable) *64
	#undef	SSD253X_SCREEN_MAX_X
	#define SSD253X_SCREEN_MAX_X    800
	#define SSD253X_SCREEN_MAX_Y    480

	u8 cut_value = 20; //cut_value < 32
	unsigned short Cut_Edge_XLeft[64]={
	0x0008,0x0009,0x000B,0x000C,0x000D,0x000E,0x0010,0x0011,
	0x0012,0x0013,0x0015,0x0016,0x0017,0x0018,0x001A,0x001B,
	0x001C,0x001D,0x001F,0x0020,0x0021,0x0022,0x0024,0x0025,
	0x0026,0x0026,0x0027,0x0028,0x0029,0x002A,0x002B,0x002C,
	0x002C,0x002D,0x002E,0x002F,0x0030,0x0031,0x0032,0x0032,
	0x0033,0x0034,0x0035,0x0036,0x0037,0x0038,0x0038,0x0039,
	0x003A,0x003B,0x003C,0x003D,0x003E,0x003E,0x003F,0x0040,
	0x0041,0x0042,0x0043,0x0044,0x0044,0x0045,0x0046,0x0047
	};

	unsigned short Cut_Edge_XRight[64]={
	0x0318,0x0317,0x0315,0x0314,0x0313,0x0312,0x0310,0x030F,
	0x030E,0x030D,0x030B,0x030A,0x0309,0x0308,0x0306,0x0305,
	0x0304,0x0303,0x0301,0x0300,0x02FF,0x02FE,0x02FC,0x02FB,
	0x02FA,0x02FA,0x02F9,0x02F8,0x02F7,0x02F6,0x02F5,0x02F4,
	0x02F4,0x02F3,0x02F2,0x02F1,0x02F0,0x02EF,0x02EE,0x02EE,
	0x02ED,0x02EC,0x02EB,0x02EA,0x02E9,0x02E8,0x02E8,0x02E7,
	0x02E6,0x02E5,0x02E4,0x02E3,0x02E2,0x02E2,0x02E1,0x02E0,
	0x02DF,0x02DE,0x02DD,0x02DC,0x02DC,0x02DB,0x02DA,0x02D9
	};

	unsigned short Cut_Edge_YUp[64]={
	0x0006,0x0007,0x0008,0x000A,0x000B,0x000C,0x000D,0x000F,
	0x0010,0x0011,0x0012,0x0014,0x0015,0x0016,0x0017,0x0018,
	0x001A,0x001B,0x001C,0x001D,0x001F,0x0020,0x0021,0x0022,
	0x0022,0x0023,0x0024,0x0025,0x0025,0x0026,0x0027,0x0028,
	0x0029,0x0029,0x002A,0x002B,0x002C,0x002C,0x002D,0x002E,
	0x002F,0x0030,0x0030,0x0031,0x0032,0x0033,0x0033,0x0034,
	0x0035,0x0036,0x0037,0x0037,0x0038,0x0039,0x003A,0x003A,
	0x003B,0x003C,0x003D,0x003E,0x003E,0x003F,0x0040,0x0041
	};

	unsigned short Cut_Edge_YDown[64]={
	0x01DA,0x01D9,0x01D8,0x01D6,0x01D5,0x01D4,0x01D3,0x01D1,
	0x01D0,0x01CF,0x01CE,0x01CC,0x01CB,0x01CA,0x01C9,0x01C8,
	0x01C6,0x01C5,0x01C4,0x01C3,0x01C1,0x01C0,0x01BF,0x01BE,
	0x01BE,0x01BD,0x01BC,0x01BB,0x01BB,0x01BA,0x01B9,0x01B8,
	0x01B7,0x01B7,0x01B6,0x01B5,0x01B4,0x01B4,0x01B3,0x01B2,
	0x01B1,0x01B0,0x01B0,0x01AF,0x01AE,0x01AD,0x01AD,0x01AC,
	0x01AB,0x01AA,0x01A9,0x01A9,0x01A8,0x01A7,0x01A6,0x01A6,
	0x01A5,0x01A4,0x01A3,0x01A2,0x01A2,0x01A1,0x01A0,0x019F
	};

	
	if(pos == 0xfff)
	{
		return pos;
	}
	if(x_y) //xpos
	{
		//#ifdef CONFIG_TS_CUTEDGE_DEBUG
			dbg("X: Raw data %d\n",pos);
		//#endif
		if (pos >=XPOS_MAX_D)
			{
				pos = XPOS_MAX_D;
			}
		if (pos<64)
			{
				pos = Cut_Edge_XLeft[pos];		//Left cut edge
			}
		else
		if ((XPOS_MAX_D - pos) <64)
			{
				pos = Cut_Edge_XRight[XPOS_MAX_D - pos];		//Right cut edge
			}
		else
			{
				pos = pos + cut_value;		//
				pos = SSD253X_SCREEN_MAX_X * pos / (XPOS_MAX_D + cut_value*2);//SSD253X_SCREEN_MAX_X|?????2????????|?
			}
		//#ifdef CONFIG_TS_CUTEDGE_DEBUG
			dbg("X: Cut edge data %d\n",pos);
		//#endif
		return pos;
	}
	else    //ypos
	{

		//#ifdef CONFIG_TS_CUTEDGE_DEBUG
			dbg("Y: Raw data %d\n",pos);
		//#endif
		if (pos >=YPOS_MAX_D)
			{
				pos = YPOS_MAX_D;
			}
		if (pos<64)
			{
				pos = Cut_Edge_YUp[pos];		//Up cut edge
			}
		else
		if ((YPOS_MAX_D - pos) <64)
			{
				pos = Cut_Edge_YDown[YPOS_MAX_D - pos];		//Down cut edge
			}
		else
			{
				pos = pos + cut_value;		//
				pos = SSD253X_SCREEN_MAX_Y * pos / (YPOS_MAX_D + cut_value*2);//SSD253X_SCREEN_MAX_X|?????2????????|?
			}
		//#ifdef CONFIG_TS_CUTEDGE_DEBUG
			dbg("Y: Cut edge data %d\n",pos);
		//#endif
		return pos;
	}
	return -1;
}

static int ssd253x_ts_cut_edge8(unsigned short pos,unsigned short x_y)
{

	#define XPOS_MAX_D (DRIVENO -EdgeDisable) *64
	#define YPOS_MAX_D (SENSENO -EdgeDisable) *64
	#undef	SSD253X_SCREEN_MAX_X
	#define SSD253X_SCREEN_MAX_X    780
	#undef	SSD253X_SCREEN_MAX_Y
	#define SSD253X_SCREEN_MAX_Y    470

	u8 cut_value = 10;//30; //cut_value < 32
	unsigned short Cut_Edge_XLeft[64]={
	0x0008,0x0009,0x000B,0x000C,0x000D,0x000E,0x0010,0x0011,
	0x0012,0x0013,0x0015,0x0016,0x0017,0x0018,0x001A,0x001B,
	0x001C,0x001D,0x001F,0x0020,0x0021,0x0022,0x0024,0x0025,
	0x0026,0x0026,0x0027,0x0028,0x0029,0x002A,0x002B,0x002C,
	0x002C,0x002D,0x002E,0x002F,0x0030,0x0031,0x0032,0x0032,
	0x0033,0x0034,0x0035,0x0036,0x0037,0x0038,0x0038,0x0039,
	0x003A,0x003B,0x003C,0x003D,0x003E,0x003E,0x003F,0x0040,
	0x0041,0x0042,0x0043,0x0044,0x0044,0x0045,0x0046,0x0047
	};

	unsigned short Cut_Edge_XRight[64]={
	0x0318,0x0317,0x0315,0x0314,0x0313,0x0312,0x0310,0x030F,
	0x030E,0x030D,0x030B,0x030A,0x0309,0x0308,0x0306,0x0305,
	0x0304,0x0303,0x0301,0x0300,0x02FF,0x02FE,0x02FC,0x02FB,
	0x02FA,0x02FA,0x02F9,0x02F8,0x02F7,0x02F6,0x02F5,0x02F4,
	0x02F4,0x02F3,0x02F2,0x02F1,0x02F0,0x02EF,0x02EE,0x02EE,
	0x02ED,0x02EC,0x02EB,0x02EA,0x02E9,0x02E8,0x02E8,0x02E7,
	0x02E6,0x02E5,0x02E4,0x02E3,0x02E2,0x02E2,0x02E1,0x02E0,
	0x02DF,0x02DE,0x02DD,0x02DC,0x02DC,0x02DB,0x02DA,0x02D9
	};

	unsigned short Cut_Edge_YUp[64]={
	0x0006,0x0007,0x0008,0x000A,0x000B,0x000C,0x000D,0x000F,
	0x0010,0x0011,0x0012,0x0014,0x0015,0x0016,0x0017,0x0018,
	0x001A,0x001B,0x001C,0x001D,0x001F,0x0020,0x0021,0x0022,
	0x0022,0x0023,0x0024,0x0025,0x0025,0x0026,0x0027,0x0028,
	0x0029,0x0029,0x002A,0x002B,0x002C,0x002C,0x002D,0x002E,
	0x002F,0x0030,0x0030,0x0031,0x0032,0x0033,0x0033,0x0034,
	0x0035,0x0036,0x0037,0x0037,0x0038,0x0039,0x003A,0x003A,
	0x003B,0x003C,0x003D,0x003E,0x003E,0x003F,0x0040,0x0041
	};

	unsigned short Cut_Edge_YDown[64]={
	0x01DA,0x01D9,0x01D8,0x01D6,0x01D5,0x01D4,0x01D3,0x01D1,
	0x01D0,0x01CF,0x01CE,0x01CC,0x01CB,0x01CA,0x01C9,0x01C8,
	0x01C6,0x01C5,0x01C4,0x01C3,0x01C1,0x01C0,0x01BF,0x01BE,
	0x01BE,0x01BD,0x01BC,0x01BB,0x01BB,0x01BA,0x01B9,0x01B8,
	0x01B7,0x01B7,0x01B6,0x01B5,0x01B4,0x01B4,0x01B3,0x01B2,
	0x01B1,0x01B0,0x01B0,0x01AF,0x01AE,0x01AD,0x01AD,0x01AC,
	0x01AB,0x01AA,0x01A9,0x01A9,0x01A8,0x01A7,0x01A6,0x01A6,
	0x01A5,0x01A4,0x01A3,0x01A2,0x01A2,0x01A1,0x01A0,0x019F
	};

	
	if(pos == 0xfff)
	{
		return pos;
	}
	if(x_y) //xpos
	{
		//#ifdef CONFIG_TS_CUTEDGE_DEBUG
			dbg("X: Raw data %d\n",pos);
		//#endif
		if (pos >=XPOS_MAX_D)
			{
				pos = XPOS_MAX_D;
			}
		if (pos<64)
			{
				pos = Cut_Edge_XLeft[pos];		//Left cut edge
			}
		else
		if ((XPOS_MAX_D - pos) <64)
			{
				pos = Cut_Edge_XRight[XPOS_MAX_D - pos];		//Right cut edge
			}
		else
			{
				pos = pos + cut_value;		//
				pos = SSD253X_SCREEN_MAX_X * pos / (XPOS_MAX_D + cut_value*2);//SSD253X_SCREEN_MAX_X|?????2????????|?
			}
		//#ifdef CONFIG_TS_CUTEDGE_DEBUG
			dbg("X: Cut edge data %d\n",pos);
		//#endif
		return pos;
	}
	else    //ypos
	{

		//#ifdef CONFIG_TS_CUTEDGE_DEBUG
			dbg("Y: Raw data %d\n",pos);
		//#endif
		if (pos >=YPOS_MAX_D)
			{
				pos = YPOS_MAX_D;
			}
		if (pos<64)
			{
				pos = Cut_Edge_YUp[pos];		//Up cut edge
			}
		else
		if ((YPOS_MAX_D - pos) <64)
			{
				pos = Cut_Edge_YDown[YPOS_MAX_D - pos];		//Down cut edge
			}
		else
			{
				pos = pos + cut_value;		//
				pos = SSD253X_SCREEN_MAX_Y * pos / (YPOS_MAX_D + cut_value*2);//SSD253X_SCREEN_MAX_X|?????2????????|?
			}
		//#ifdef CONFIG_TS_CUTEDGE_DEBUG
			dbg("Y: Cut edge data %d\n",pos);
		//#endif
		return pos;
	}
}

static int ssd253x_ts_cut_edge7(unsigned short pos,unsigned short x_y)
{
	unsigned short SENSENO_7 = 15;
	unsigned short DRIVENO_7 = 20;
	unsigned short EdgeDisable_7 =	1;	// if Edge Disable, set it to 1, else reset to 0, OR  SSD2533 set 0
	unsigned short XPOS_MAX_7 = (DRIVENO_7 -EdgeDisable_7) *64;
	unsigned short YPOS_MAX_7 = (SENSENO_7 -EdgeDisable_7) *64;

	u8 cut_value = 10; //cut_value < 32
	dbg("enter...\n");

	if(pos == 0xfff)
	{
		return pos;
	}
	if(x_y) //xpos
	{
		if(pos < 16)
			pos = cut_value + pos*(48 - cut_value) / 16;
		else if(pos > (XPOS_MAX_7 - 16) )
			pos = XPOS_MAX_7 + 16 + (pos - (XPOS_MAX_7 -16))*(48 - cut_value) / 16;
		else
			pos = pos + 32;
		dbg("xpos_b:%d\n", pos);
		pos = SSDS53X_SCREEN_MAX_X * pos / (DRIVENO_7 * 64);
		dbg("xpos_a:%d\n", pos);
		return pos;
	}
	else    //ypos
	{
		if(pos < 16)
			pos = cut_value + pos*(48 - cut_value) / 16;
		else if(pos > (YPOS_MAX_7 - 16) )
			pos = YPOS_MAX_7 + 16 + (pos - (YPOS_MAX_7 -16))*(48 - cut_value) / 16;
		else
			pos = pos + 32;
		dbg("ypos_b:%d\n", pos);
		pos = SSDS53X_SCREEN_MAX_Y* pos / (SENSENO_7 * 64);
		dbg("ypos_a:%d\n", pos);
		return pos;		
	}
	
	
}


static int ssd253x_ts_cut_edge(unsigned short pos,unsigned short x_y)
{
	switch (wmt_ts_get_cutedge())
	{
		case 0:
			return ssd253x_ts_cut_edge0(pos,x_y);
			break;
		case 1:
			return ssd253x_ts_cut_edge1(pos,x_y);
			break;
		case 2:
			return ssd253x_ts_cut_edge2(pos,x_y);
			break;
        case 3:
			return ssd253x_ts_cut_edge3(pos,x_y);
			break;
		case 4:
			return ssd253x_ts_cut_edge4(pos,x_y);
			break;
		case 5:
			return ssd253x_ts_cut_edge5(pos,x_y);
			break;
		case 6:
			return ssd253x_ts_cut_edge6(pos,x_y);
			break;
		case 7:
			return ssd253x_ts_cut_edge7(pos,x_y);
			break;
		case 8:
			return ssd253x_ts_cut_edge8(pos,x_y);
			break;
		default:
			return -1;
	};
}

#ifdef USE_TOUCH_KEY
static u8 btn_status_last = 0;
#endif
	
static void ssd253x_ts_work(struct work_struct *work)
{
	int i;
	unsigned short xpos=0, ypos=0;
	int tx,ty;
	//width=0;
	int FingerInfo;
	int EventStatus;
	int FingerX[FINGERNO];
	int FingerY[FINGERNO];
	int FingerP[FINGERNO];
	int clrFlag=0;
	int Ssd_Timer;
	#ifdef USE_TOUCH_KEY
	u8 btn_status;
	u8 btn_status_last = 0;
	#endif

	struct ssl_ts_priv *ssl_priv = l_ts;

	#ifdef USE_TOUCH_KEY
	btn_status = ReadRegister(ssl_priv->client,SELFCAP_STATUS_REG, 1);
	//#ifdef CONFIG_TOUCHSCREEN_SSL_DEBUG
		dbg("btn pressed:%x\n", btn_status & 0x0f);
	//#endif
	if (btn_status_last != btn_status){
		if(btn_status){
			btn_status_last = btn_status;
			ssd2533_ts_send_keyevent(ssl_priv,btn_status, 1);
			dbg("send %x btn_status_last%d \n", btn_status,btn_status_last);
		}
		else{
				ssd2533_ts_send_keyevent(ssl_priv,btn_status_last, 0);
				btn_status_last = 0;
				dbg("btn_status_last %x \n", btn_status_last);
		}
		return ;
	}
	#endif

	Ssd_Timer = 0;
	if(ic_flag == IC_SSD2533){
		if(!Ssd_Timer_flag){
			Ssd_Timer = ReadRegister(TIMESTAMP_REG,2);
			if(!Ssd_Timer1){
					Ssd_Timer1 = Ssd_Timer/1000;   			
			}
		
			Ssd_Timer2 = Ssd_Timer/1000;               

		
			if((Ssd_Timer2 - Ssd_Timer1) > 10){
			    WriteRegister(AUTO_INIT_RST_REG,0x00,0x00,1);
			    Ssd_Timer_flag = 1;
			}
		 }
	}
	
	EventStatus = ReadRegister(EVENT_STATUS,2)>>4;
	ssl_priv->FingerDetect=0;
	for(i=0;i<ssl_priv->FingerNo;i++){
		if((EventStatus>>i)&0x1){
			FingerInfo=ReadRegister(FINGER01_REG+i,4);
			xpos = ((FingerInfo>>4)&0xF00)|((FingerInfo>>24)&0xFF);
			ypos = ((FingerInfo>>0)&0xF00)|((FingerInfo>>16)&0xFF);	
            dbg("raw data before cut, F%d:(%d,%d)\n",i,xpos,ypos);
			if(xpos!=0xFFF){
				ssl_priv->FingerDetect++;
				if (wmt_ts_get_cutedge()>=0){
					xpos = ssd253x_ts_cut_edge(xpos, 1);
					ypos = ssd253x_ts_cut_edge(ypos, 0);
				}
			}else {
				EventStatus=EventStatus&~(1<<i);
				clrFlag=1;
			}
		}else{
			xpos=ypos=0xFFF;
			clrFlag=1;
		}
		FingerX[i]=xpos;
		FingerY[i]=ypos;
	}

    if(ssl_priv->use_irq==1) wmt_enable_gpirq();
	if(ssl_priv->use_irq==2)
	{
		if(ssl_priv->FingerDetect==0) 
		{
			wmt_enable_gpirq();
		} else {
			hrtimer_start(&ssl_priv->timer, ktime_set(0, MicroTimeTInterupt), HRTIMER_MODE_REL);
		}
	}
	if(ic_flag == IC_SSD2533){
		if(clrFlag) WriteRegister(EVENT_FIFO_SCLR,0x01,0x00,1);
	}

	if(ssl_priv->input->id.product==0x2533)
	if(ssl_priv->input->id.version==0x0101) 
		FingerCheckSwap(FingerX,FingerY,FingerP,ssl_priv->FingerNo,ssl_priv->sFingerX,ssl_priv->sFingerY);

	// report data
	for(i=0;i<ssl_priv->FingerNo;i++)
	{
		xpos=FingerX[i];
		ypos=FingerY[i];
		if(ssl_priv->input->id.product==0x2533){
			if(ssl_priv->input->id.version==0x0101) RunningAverage(&xpos,&ypos,i,ssl_priv);
			if(ssl_priv->input->id.version==0x0102) RunningAverage(&xpos,&ypos,i,ssl_priv);
		}

		if(xpos!=0xFFF)
		{
			dbg("raw data after cut, F%d:(%d,%d)\n",i,xpos,ypos);
			switch (wmt_ts_get_xaxis())
			{				
				case 1:
					tx = ypos;
					break;
				case 0:
				default:
					tx = xpos;
					break;
			}
            
			switch (wmt_ts_get_xdir())
			{
				case 1:
					break;
				case -1:
					tx = SSDS53X_SCREEN_MAX_Y - tx;
					break;
				default:
					break;
			};
			
			if (tx <0){
				tx = 0;
			} else if (tx >= SSDS53X_SCREEN_MAX_Y){
				tx = SSDS53X_SCREEN_MAX_Y-1;
			}
			switch (wmt_ts_get_yaxis())
			{
				
				case 0:
					ty = xpos;
					break;
				case 1:
				default:
					ty = ypos;
					break;
			}
            
			switch (wmt_ts_get_ydir())
			{
				case 1:
					break;
				case -1:
					ty = SSDS53X_SCREEN_MAX_X - ty;
				default:
					break;
			}

			if (ty < 0){
				ty = 0;
			} else if (ty >=SSDS53X_SCREEN_MAX_X){
				ty = SSDS53X_SCREEN_MAX_X-1;
			}

			if (wmt_ts_get_lcdexchg()) {
				int tmp;
				tmp = tx;
				tx = ty;
				ty = wmt_ts_get_resolvX() - tmp;
			}

			ssd253x_tsdev.penup = 0;
			input_report_abs(ssl_priv->input, ABS_MT_POSITION_X, tx);
			input_report_abs(ssl_priv->input, ABS_MT_POSITION_Y, ty);
			/*input_report_abs(ssl_priv->input, ABS_MT_POSITION_X, ty);
			input_report_abs(ssl_priv->input, ABS_MT_POSITION_Y, tx);*/
			input_mt_sync(ssl_priv->input);
			dbg("report data x=%d,y=%d\n", tx, ty);

		} 
        else if(ssl_priv->FingerX[i]!=0xFFF){
			input_mt_sync(ssl_priv->input);
			//printk("pen up...\n");
			ssd253x_tsdev.penup = 1;
		}
        
		ssl_priv->FingerX[i]=FingerX[i];
		ssl_priv->FingerY[i]=FingerY[i];
	}
    
	ssl_priv->EventStatus=EventStatus;	
	input_sync(ssl_priv->input);
	if (1 == ssd253x_tsdev.penup){
		wake_up(&ts_penup_wait_queue);
	}

}


#define TPIC_INT_PLLLING	0
#define TPIC_INT_INTERUPT	1
#define TPIC_INT_HYBRID		2


static int ssd253x_probe(struct platform_device *pdev)
{
	struct ssl_ts_priv *ssl_priv;
	struct input_dev *ssl_input;
	int error;
	int i;
	//unsigned int prescale;

	//#ifdef SD_INIT
	//	g_tp_client = l_client;
	//#endif

	SSDS53X_SCREEN_MAX_X = wmt_ts_get_resolvY();	
	SSDS53X_SCREEN_MAX_Y = wmt_ts_get_resolvX();

	ssl_priv = kzalloc(sizeof(*ssl_priv), GFP_KERNEL);
	if (!ssl_priv)
	{
		errlog(" kzalloc Error!\n");
		error=-ENODEV;
		goto	err0;
	}
	l_ts = ssl_priv;
	
	ssl_input = input_allocate_device();
	if (!ssl_input)
	{
		errlog("		ssd253x_ts_probe: input_allocate_device Error\n");
		error=-ENODEV;
		goto	freealloc;
	}
	ssl_input->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS) | BIT_MASK(EV_SYN) ;
	set_bit(INPUT_PROP_DIRECT,ssl_input->propbit);
	ssl_input->name = DEVICE_NAME;
	ssl_input->id.bustype = BUS_I2C;
	ssl_input->id.vendor  = 0x2878; // Modify for Vendor ID
	
	ssl_priv->input = ssl_input;
	
	ssl_priv->FingerNo=wmt_ts_get_fingernum();//FINGERNO;
	ssl_priv->Resolution=64;

	for(i=0;i<ssl_priv->FingerNo;i++)
	{
		ssl_priv->sFingerX[i]=0xFFF;
		ssl_priv->sFingerY[i]=0xFFF;

		// For Adaptive Running Average
		ssl_priv->pFingerX[i]=0xFFF;
		ssl_priv->pFingerY[i]=0xFFF;
	}

	deviceReset();
	ssl_input->id.product = ReadRegister(DEVICE_ID_REG,2);
	ssl_input->id.version = ReadRegister(VERSION_ID_REG,2);
	ssl_input->id.product = ReadRegister(DEVICE_ID_REG,2);

	ssl_input->id.version = ReadRegister(VERSION_ID_REG,2);
	klog("SSL Touchscreen Device ID  : 0x%04X\n",ssl_input->id.product);
	klog("SSL Touchscreen Version ID : 0x%04X\n",ssl_input->id.version);

	if(ssl_input->id.product == 0x2531){
		ic_flag = IC_SSD2531;
	}else if(ssl_input->id.product == 0x2533) {
		ic_flag = IC_SSD2533;
	}else if(ssl_input->id.product == 0x2543) {
		ic_flag = IC_SSD2543;
	}

	if(ic_flag == IC_SSD2533) {
		ssl_priv->use_irq = TPIC_INT_HYBRID;
	}else if(ic_flag == IC_SSD2543) {
		ssl_priv->use_irq = TPIC_INT_INTERUPT;
	}

	SSD253xdeviceInit();
	if(ic_flag == IC_SSD2533) {
		WriteRegister(EVENT_FIFO_SCLR,0x01,0x00,1); // clear Event FiFo
	}


	if(ssl_priv->input->id.product==0x2531)		
        ssl_priv->Resolution=32;
	else if(ssl_priv->input->id.product==0x2533)	
        ssl_priv->Resolution=64;


	if (wmt_ts_get_lcdexchg()) {
		input_set_abs_params(ssl_input, ABS_MT_POSITION_X, 0,wmt_ts_get_resolvY(), 0, 0);
		input_set_abs_params(ssl_input, ABS_MT_POSITION_Y, 0,wmt_ts_get_resolvX(), 0, 0);
	} else {
		input_set_abs_params(ssl_input, ABS_MT_POSITION_X, 0,wmt_ts_get_resolvX(), 0, 0);
		input_set_abs_params(ssl_input, ABS_MT_POSITION_Y, 0,wmt_ts_get_resolvY(), 0, 0);
	}

#ifdef USE_TOUCH_KEY
	set_bit(KEY_MENU, ssl_input->keybit);
	set_bit(KEY_HOME, ssl_input->keybit);
	set_bit(KEY_BACK, ssl_input->keybit);
	set_bit(KEY_SEARCH, ssl_input->keybit);
	#endif
	error = input_register_device(ssl_input);
	if(error)
	{
		errlog("input_register_device input Error!\n");
		error=-ENODEV;
		goto	panel_init_fail;
	}
	if((ssl_priv->use_irq==0)||(ssl_priv->use_irq==2))
	{
		hrtimer_init(&ssl_priv->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		ssl_priv->timer.function = ssd253x_ts_timer;
		//#ifdef CONFIG_TOUCHSCREEN_SSL_DEBUG
		dbg("		ssd253x_ts_probe: timer_init OK!\n");
		//#endif
	}

	ssd253x_wq = create_singlethread_workqueue("ssd253x_wq");
	INIT_WORK(&ssl_priv->ssl_work, ssd253x_ts_work);
	error = request_irq(wmt_get_tsirqnum(), ssd253x_ts_isr, IRQF_SHARED, "ssd253x_ts_q", l_ts);
	if(error){
		errlog("request_irq Error!\n");
		error=-ENODEV;
		goto freeque;
	}

    wmt_set_gpirq(IRQ_TYPE_EDGE_FALLING);
	wmt_disable_gpirq();
	
	
#ifdef	CONFIG_HAS_EARLYSUSPEND
	ssl_priv->early_suspend.suspend = ssd253x_ts_early_suspend;
	ssl_priv->early_suspend.resume  = ssd253x_ts_late_resume;
	ssl_priv->early_suspend.level   = EARLY_SUSPEND_LEVEL_BLANK_SCREEN+2;
	register_early_suspend(&ssl_priv->early_suspend);
#endif
	deviceResume();
	wmt_enable_gpirq();
	dbg("SSD253X init ok!\n");	
	return 0;

freeque:
	destroy_workqueue(ssd253x_wq);
	input_unregister_device(ssl_input);
panel_init_fail:
	input_free_device(ssl_input);
freealloc:	
	kfree(ssl_priv);
err0:	
	//dev_set_drvdata(&client->dev, NULL);
	return error;
}

static int ssd253x_remove(struct platform_device *pdev)
{
	struct ssl_ts_priv *ssl_priv = l_ts;	

	if((ssl_priv->use_irq==0)||(ssl_priv->use_irq==2)) hrtimer_cancel(&ssl_priv->timer);

	//disable int
	wmt_disable_gpirq();
	//free irq
	free_irq(wmt_get_tsirqnum(), l_ts);
#ifdef	CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&ssl_priv->early_suspend);
#endif
	// free queue
	cancel_work_sync (&ssl_priv->ssl_work);
	flush_workqueue(ssd253x_wq);	
	destroy_workqueue(ssd253x_wq);
	input_unregister_device(ssl_priv->input);
	input_free_device(ssl_priv->input);
	kfree(ssl_priv);
	l_ts = NULL;
	return 0;
}


/*
static int ssd253x_ts_open(struct input_dev *dev)
{
	struct ssl_ts_priv *ssl_priv = l_ts;

	deviceResume();
	if(ssl_priv->use_irq)
	{
		wmt_enable_gpirq(); //(ssl_priv->irq);
	} else {
		hrtimer_start(&ssl_priv->timer, ktime_set(1, 0), HRTIMER_MODE_REL);
	}
	return 0;
}


static void ssd253x_ts_close(struct input_dev *dev)
{
	struct ssl_ts_priv *ssl_priv = l_ts;

	// disable interrupt
	deviceSuspend();
	if((ssl_priv->use_irq==0)||(ssl_priv->use_irq==2)) 
        hrtimer_cancel(&ssl_priv->timer);
	if((ssl_priv->use_irq==1)||(ssl_priv->use_irq==2)) 
        wmt_disable_gpirq();//(ssl_priv->irq);
}
*/
static int ssd253x_resume(struct platform_device *pdev)
{
	struct ssl_ts_priv *ssl_priv = l_ts;

	wmt_disable_gpirq();
    Ssd_Timer_flag = 0;
	deviceReset();
	SSD253xdeviceInit();
	if(ic_flag == IC_SSD2533){
		WriteRegister(EVENT_FIFO_SCLR,0x01,0x00,1); // clear Event FiFo		
	}
	deviceResume();
	wmt_set_gpirq(IRQ_TYPE_EDGE_FALLING);
	wmt_enable_gpirq();	
	
	if(! ssl_priv->use_irq) 
	{
		hrtimer_start(&ssl_priv->timer, ktime_set(1, 0), HRTIMER_MODE_REL);
	}
	ssl_priv->earlysus = 0;
	return 0;
}

static int ssd253x_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct ssl_ts_priv *ssl_priv = l_ts;

	if((ssl_priv->use_irq==0)||(ssl_priv->use_irq==2)) hrtimer_cancel(&ssl_priv->timer);
	// disable irq
	wmt_disable_gpirq();
	Ssd_Timer_flag = 0;
	if(ic_flag == IC_SSD2533){
		deviceSuspend();
	}else if(ic_flag == IC_SSD2543){
		deviceReset();
	}
	return 0;
}

#ifdef	CONFIG_HAS_EARLYSUSPEND
static void ssd253x_ts_late_resume(struct early_suspend *h)
{
	struct ssl_ts_priv *ssl_priv = l_ts;

	dbg("...\n");	
	if (ssl_priv->earlysus != 0)
	{
	    wmt_disable_gpirq();
	    Ssd_Timer_flag = 0;
		deviceReset();
		SSD253xdeviceInit();
		WriteRegister(EVENT_FIFO_SCLR,0x01,0x00,1); // clear Event FiFo		
		deviceResume();
		wmt_set_gpirq(IRQ_TYPE_EDGE_FALLING);
		wmt_enable_gpirq();	
		
		if(! ssl_priv->use_irq) 
		{
			hrtimer_start(&ssl_priv->timer, ktime_set(1, 0), HRTIMER_MODE_REL);
		}
		ssl_priv->earlysus = 0;
	}
}
static void ssd253x_ts_early_suspend(struct early_suspend *h)
{
	struct ssl_ts_priv *ssl_priv = l_ts;

	ssl_priv->earlysus = 1;
	if((ssl_priv->use_irq==0)||(ssl_priv->use_irq==2)) hrtimer_cancel(&ssl_priv->timer);
	// disable irq
	wmt_disable_gpirq();
	Ssd_Timer_flag = 0;
	deviceSuspend();

    return;
}
#endif


static irqreturn_t ssd253x_ts_isr(int irq, void *dev_id)
{
	struct ssl_ts_priv *ssl_priv = l_ts;

	if (wmt_is_tsint())
	{
		wmt_clr_int();
		if (wmt_is_tsirq_enable())
		{
			wmt_disable_gpirq();
			dbg("begin..\n");
			if(!ssl_priv->earlysus)
			{
				queue_work(ssd253x_wq, &ssl_priv->ssl_work);
			}
		}
		return IRQ_HANDLED;
	}

	return IRQ_NONE;
}

static enum hrtimer_restart ssd253x_ts_timer(struct hrtimer *timer)
{
	struct ssl_ts_priv *ssl_priv = container_of(timer, struct ssl_ts_priv, timer);
	#ifdef CONFIG_TOUCHSCREEN_SSL_DEBUG
	printk("+-----------------------------------------+\n");
	printk("|	ssd253x_ts_timer!                 |\n");
	printk("+-----------------------------------------+\n");
	#endif
	queue_work(ssd253x_wq, &ssl_priv->ssl_work);
	if(ssl_priv->use_irq==0) hrtimer_start(&ssl_priv->timer, ktime_set(0, MicroTimeTInterupt), HRTIMER_MODE_REL);
	return HRTIMER_NORESTART;
}

#ifdef SD_INIT
static const struct file_operations tp_fops = {
	.owner		= THIS_MODULE,
	.read		= tp_read,
	.write		= tp_write,
	.unlocked_ioctl	= tp_ioctl,
	.open		= tp_open,
	.release	= tp_release,
};

static struct miscdevice misc = {  
    .minor = MISC_DYNAMIC_MINOR,  
    .name  = TP_CHR, 
    .fops  = &tp_fops,  
};  
#endif


static int ssd253x_init(void)
{
	char firmwname[60];
	int i;

	if (deviceReset() != 0)
		return -1;
	memset(firmwname,0,sizeof(firmwname));
	wmt_ts_get_firmwname(firmwname);
	i = read_firmwarefile(firmwname,&ssd253xcfgTable,0x100);
	if (i <= 0)
	{
		l_cfglen = sizeof(ssd253xcfgTable_default)/sizeof(ssd253xcfgTable_default[0]);
		ssd253xcfgTable = ssd253xcfgTable_default;
		dbg("Using the default configure!\n");
	} else {
		l_cfglen = i;
	}
	Resume[1].No = ssd253xcfgTable[l_cfglen-1].No;
	Resume[1].Reg = ssd253xcfgTable[l_cfglen-1].Reg;
	Resume[1].Data1 = ssd253xcfgTable[l_cfglen-1].Data1;
	Resume[1].Data2 = ssd253xcfgTable[l_cfglen-1].Data2;
	if (SSD253xdeviceInit()!= 0)
	{
		if (i > 0)
		{
			kfree(ssd253xcfgTable);
		}
		return -1;
	}
	// init hardware

#ifdef SD_INIT
	misc_register(&misc);  
#endif


	return 0;
}

static void ssd253x_exit(void)
{
	klog("remove the module\n");

#ifdef SD_INIT
	misc_deregister(&misc);  
#endif

	
	if (ssd253xcfgTable != ssd253xcfgTable_default)
	{
		kfree(ssd253xcfgTable);
	}
	
}

static int ssd253x_wait_penup(struct wmtts_device*tsdev)
{
	int ret = wait_event_interruptible(
			ts_penup_wait_queue,
			(1==tsdev->penup));
	return ret;
}


struct wmtts_device raysen_tsdev = {
		.driver_name = "ssd253x_ts",
		.ts_id = "SSD253X",
		.init = ssd253x_init,
		.exit = ssd253x_exit,
		.probe = ssd253x_probe,
		.remove = ssd253x_remove,
		.suspend = ssd253x_suspend,
		.resume = ssd253x_resume,
		.wait_penup = ssd253x_wait_penup,
		.penup = 1,
};

#ifdef SD_INIT
static long tp_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{	
	return 0;
}

static int tp_open(struct inode *inode, struct file *file)
{
	return 0;
}
static int tp_release(struct inode *inode, struct file *file)
{
	return 0;
}

static ssize_t tp_read(struct file *file, char __user *buf, size_t count,loff_t *offset)
{
	char *kbuf;  
	uint8_t reg;
	int  ByteNo;
	int readValue;
	int i;

	kbuf = kmalloc(count,GFP_KERNEL);  
    
	 if(copy_from_user(kbuf,buf,1))  {  
		printk("no enough memory!\n");  
		return -1;  
	 }  

	reg = (uint8_t)kbuf[0];
	ByteNo = count;

	readValue = ReadRegister( /*g_tp_client, */reg, ByteNo);

	for(i = 0;i < ByteNo;i++){
		kbuf[i] = (readValue>>(8*i)) & 0xff;
	}

	if(copy_to_user(buf,kbuf,count))  {  
		printk("no enough memory!\n");  
		return -1;  
	}  

	kfree(kbuf);

	return count;
}

static ssize_t tp_write(struct file *file, const char __user *buf,size_t count, loff_t *offset)
{
	char *kbuf;   

	kbuf = kmalloc(count,GFP_KERNEL);  
      
	if(copy_from_user(kbuf,buf,count))  {  
		printk("no enough memory!\n");  
		return -1;  
	}  

	if(kbuf[1] == 0x01){
		wmt_rst_output(0);
		mdelay(5);
		wmt_rst_output(1);
		mdelay(20);
	}
	else
	{
		WriteRegister(/*g_tp_client,*/kbuf[1],kbuf[2],kbuf[3],kbuf[0]);
	}
			
	kfree(kbuf);

	return count;
}

#endif




MODULE_AUTHOR("Solomon Systech Ltd - Design Technology, Icarus Choi");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("ssd253x Touchscreen Driver 1.3");

/*****************************************************************************
 *
 * Copyright (c) 2013 mCube, Inc.  All rights reserved.
 *
 * This source is subject to the mCube Software License.
 * This software is protected by Copyright and the information and source code
 * contained herein is confidential. The software including the source code
 * may not be copied and the information contained herein may not be used or
 * disclosed except with the written permission of mCube Inc.
 *
 * All other rights reserved.
 *
 * This code and information are provided "as is" without warranty of any
 * kind, either expressed or implied, including but not limited to the
 * implied warranties of merchantability and/or fitness for a
 * particular purpose.
 *
 * The following software/firmware and/or related documentation ("mCube Software")
 * have been modified by mCube Inc. All revisions are subject to any receiver's
 * applicable license agreements with mCube Inc.
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
 *
 *****************************************************************************/
 
#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/mutex.h>
//#include <linux/earlysuspend.h>
#include <linux/delay.h>
#include <asm/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>


#include <mach/hardware.h>
#include <linux/fs.h>

#include "../sensor.h"


//#include <mach/sys_config.h>

//=== CONFIGURATIONS ==========================================================
//#define _MC3XXX_DEBUG_ON_

//=============================================================================
#ifdef _MC3XXX_DEBUG_ON_
    #define mcprintkreg(x...)     	printk(x)
    #define mcprintkfunc(x...)    	printk(x)
    #define GSE_ERR(x...) 	      	printk(x)
    #define GSE_LOG(x...) 	      	printk(x)
#else
    #define mcprintkreg(x...)
    #define mcprintkfunc(x...)
    #define GSE_ERR(x...)
    #define GSE_LOG(x...)
#endif

static int g_virtual_z = 0;
#define G_2_REVERSE_VIRTUAL_Z	0 //!!!!! 1
#define SUPPORT_VIRTUAL_Z_SENSOR //add 2013-10-23
#define LOW_RESOLUTION 1
#define HIGH_RESOLUTION 2
#define RBM_RESOLUTION 3
#ifdef SUPPORT_VIRTUAL_Z_SENSOR
#define Low_Pos_Max 127
#define Low_Neg_Max -128
#define High_Pos_Max 8191
#define High_Neg_Max -8192
#define VIRTUAL_Z	1
static int Railed = 0;
#else
#define VIRTUAL_Z	0
#endif


static struct class* l_dev_class = NULL;


#define SENSOR_NAME 				"mc3xxx"
#define SENSOR_DRIVER_VERSION    	"1.0.0"
#define SENSOR_DATA_SIZE		3

//static int mc3xxx_pin_hd;
//static char mc3xxx_on_off_str[32];
#define G_0		ABS_Y
#define G_1		ABS_X
#define G_2		ABS_Z
#define G_0_REVERSE	1
#define G_1_REVERSE	1
#define G_2_REVERSE	1

//static unsigned char    s_bResolution = 0x00;
static unsigned char    s_bPCODE      =  0x00;
static unsigned short   mc3xxx_i2c_auto_probe_addr[] = { 0x4C, 0x6C, 0x4E, 0x6D, 0x6E, 0x6F };

//=============================================================================
#define SENSOR_DMARD_IOCTL_BASE          234
#define IOCTL_SENSOR_SET_DELAY_ACCEL     _IO(SENSOR_DMARD_IOCTL_BASE, 100)
#define IOCTL_SENSOR_GET_DELAY_ACCEL     _IO(SENSOR_DMARD_IOCTL_BASE, 101)
#define IOCTL_SENSOR_GET_STATE_ACCEL     _IO(SENSOR_DMARD_IOCTL_BASE, 102)
#define IOCTL_SENSOR_SET_STATE_ACCEL     _IO(SENSOR_DMARD_IOCTL_BASE, 103)
#define IOCTL_SENSOR_GET_DATA_ACCEL      _IO(SENSOR_DMARD_IOCTL_BASE, 104)

#define IOCTL_MSENSOR_SET_DELAY_MAGNE    _IO(SENSOR_DMARD_IOCTL_BASE, 200)
#define IOCTL_MSENSOR_GET_DATA_MAGNE     _IO(SENSOR_DMARD_IOCTL_BASE, 201)
#define IOCTL_MSENSOR_GET_STATE_MAGNE    _IO(SENSOR_DMARD_IOCTL_BASE, 202)
#define IOCTL_MSENSOR_SET_STATE_MAGNE    _IO(SENSOR_DMARD_IOCTL_BASE, 203)

#define IOCTL_SENSOR_GET_NAME            _IO(SENSOR_DMARD_IOCTL_BASE, 301)
#define IOCTL_SENSOR_GET_VENDOR          _IO(SENSOR_DMARD_IOCTL_BASE, 302)
#define IOCTL_SENSOR_GET_CONVERT_PARA    _IO(SENSOR_DMARD_IOCTL_BASE, 401)
//#define SENSOR_CALIBRATION               _IOWR(SENSOR_DMARD_IOCTL_BASE, 402, int[SENSOR_DATA_SIZE])

//=============================================================================
#define MC3XXX_CONVERT_PARAMETER    (1.5f * (9.80665f) / 256.0f)
#define MC3XXX_DISPLAY_NAME         SENSOR_NAME
#define MC3XXX_DIPLAY_VENDOR        "mCube"

//=============================================================================
#define MC3XXX_AXIS_X      0
#define MC3XXX_AXIS_Y      1
#define MC3XXX_AXIS_Z      2
#define MC3XXX_AXIS_NUM    3
#define MC3XXX_DATA_LEN    6


/***********************************************
 *** REGISTER MAP
 ***********************************************/
#define MC3XXX_REG_XOUT                    			0x00
#define MC3XXX_REG_YOUT                    			0x01
#define MC3XXX_REG_ZOUT                    			0x02
#define MC3XXX_REG_TILT_STATUS             		0x03
#define MC3XXX_REG_SAMPLE_RATE_STATUS      	0x04
#define MC3XXX_REG_SLEEP_COUNT             		0x05
#define MC3XXX_REG_INTERRUPT_ENABLE        		0x06
#define MC3XXX_REG_MODE_FEATURE            		0x07
#define MC3XXX_REG_SAMPLE_RATE             		0x08
#define MC3XXX_REG_TAP_DETECTION_ENABLE    	0x09
#define MC3XXX_REG_TAP_DWELL_REJECT        	0x0A
#define MC3XXX_REG_DROP_CONTROL            		0x0B
#define MC3XXX_REG_SHAKE_DEBOUNCE          		0x0C
#define MC3XXX_REG_XOUT_EX_L               		0x0D
#define MC3XXX_REG_XOUT_EX_H              		 	0x0E
#define MC3XXX_REG_YOUT_EX_L               		0x0F
#define MC3XXX_REG_YOUT_EX_H               		0x10
#define MC3XXX_REG_ZOUT_EX_L               			0x11
#define MC3XXX_REG_ZOUT_EX_H               		0x12
#define MC3XXX_REG_RANGE_CONTROL           		0x20
#define MC3XXX_REG_SHAKE_THRESHOLD         	0x2B
#define MC3XXX_REG_UD_Z_TH                 			0x2C
#define MC3XXX_REG_UD_X_TH                 			0x2D
#define MC3XXX_REG_RL_Z_TH                 			0x2E
#define MC3XXX_REG_RL_Y_TH                 			0x2F
#define MC3XXX_REG_FB_Z_TH                 			0x30
#define MC3XXX_REG_DROP_THRESHOLD          		0x31
#define MC3XXX_REG_TAP_THRESHOLD           		0x32
#define MC3XXX_REG_PRODUCT_CODE            		0x3B

/***********************************************
 *** RETURN CODE
 ***********************************************/
#define MC3XXX_RETCODE_SUCCESS                		 	(0)
#define MC3XXX_RETCODE_ERROR_I2C               		(-1)
#define MC3XXX_RETCODE_ERROR_NULL_POINTER      	(-2)
#define MC3XXX_RETCODE_ERROR_STATUS            		(-3)
#define MC3XXX_RETCODE_ERROR_SETUP             		(-4)
#define MC3XXX_RETCODE_ERROR_GET_DATA          		(-5)
#define MC3XXX_RETCODE_ERROR_IDENTIFICATION    	(-6)


/***********************************************
 *** CONFIGURATION
 ***********************************************/
#define MC3XXX_BUF_SIZE    	256

#define MCUBE_1_5G_8BIT    	0x01		//MC3XXX_LOW_END
#define MCUBE_8G_14BIT     	0x02		//MC3XXX_HIGH_END

#define DOT_CALI


#define SENSOR_DURATION_DEFAULT    20

#define INPUT_FUZZ  0
#define INPUT_FLAT  0

/***********************************************
 *** PRODUCT ID
 ***********************************************/
#define MC3XXX_PCODE_3210     	0x90
#define MC3XXX_PCODE_3230     	0x19
#define MC3XXX_PCODE_3250     	0x88
#define MC3XXX_PCODE_3410     	0xA8
#define MC3XXX_PCODE_3410N   0xB8
#define MC3XXX_PCODE_3430     	0x29
#define MC3XXX_PCODE_3430N   0x39
#define MC3XXX_PCODE_3510B   0x40
#define MC3XXX_PCODE_3530B   0x30
#define MC3XXX_PCODE_3510C   0x10
#define MC3XXX_PCODE_3530C   0x6E

//=============================================================================
static unsigned char  is_new_mc34x0 = 0;
static unsigned char  is_mc3250 = 0;
static unsigned char  is_mc35xx = 0;
static unsigned char  Sensor_Accuracy = 0;


//=============================================================================
#ifdef DOT_CALI
#define CALIB_PATH				"/data/data/com.mcube.acc/files/mcube-calib.txt"
//MCUBE_BACKUP_FILE
#define BACKUP_CALIB_PATH		"/data/misc/mcube-calib.txt"
static char backup_buf[64];
//MCUBE_BACKUP_FILE
#define DATA_PATH			   "/sdcard/mcube-register-map.txt"

typedef struct {
	unsigned short	x;		/**< X axis */
	unsigned short	y;		/**< Y axis */
	unsigned short	z;		/**< Z axis */
} GSENSOR_VECTOR3D;

static GSENSOR_VECTOR3D gsensor_gain = { 0 };
static struct miscdevice mc3xxx_device;

static struct file * fd_file = NULL;

static mm_segment_t oldfs = { 0 };
static unsigned char offset_buf[6] = { 0 };
static signed int offset_data[3] = { 0 };
s16 G_RAW_DATA[3] = { 0 };
static signed int gain_data[3] = { 0 };
static signed int enable_RBM_calibration = 0;

#define GSENSOR                                0x95
#define GSENSOR_IOCTL_INIT                     _IO(GSENSOR,  0x01)
#define GSENSOR_IOCTL_READ_CHIPINFO            _IOR(GSENSOR, 0x02, int)
#define GSENSOR_IOCTL_READ_SENSORDATA          _IOR(GSENSOR, 0x03, int)
#define GSENSOR_IOCTL_READ_OFFSET              _IOR(GSENSOR, 0x04, GSENSOR_VECTOR3D)
#define GSENSOR_IOCTL_READ_GAIN                _IOR(GSENSOR, 0x05, GSENSOR_VECTOR3D)
#define GSENSOR_IOCTL_READ_RAW_DATA            _IOR(GSENSOR, 0x06, int)
//#define GSENSOR_IOCTL_SET_CALI                 _IOW(GSENSOR, 0x06, SENSOR_DATA)
#define GSENSOR_IOCTL_GET_CALI                 _IOW(GSENSOR, 0x07, SENSOR_DATA)
#define GSENSOR_IOCTL_CLR_CALI                 _IO(GSENSOR, 0x08)
#define GSENSOR_MCUBE_IOCTL_READ_RBM_DATA      _IOR(GSENSOR, 0x09, SENSOR_DATA)
#define GSENSOR_MCUBE_IOCTL_SET_RBM_MODE       _IO(GSENSOR, 0x0a)
#define GSENSOR_MCUBE_IOCTL_CLEAR_RBM_MODE     _IO(GSENSOR, 0x0b)
#define GSENSOR_MCUBE_IOCTL_SET_CALI           _IOW(GSENSOR, 0x0c, SENSOR_DATA)
#define GSENSOR_MCUBE_IOCTL_REGISTER_MAP       _IO(GSENSOR, 0x0d)
#define GSENSOR_IOCTL_SET_CALI_MODE            _IOW(GSENSOR, 0x0e,int)
#define GSENSOR_MCUBE_IOCTL_READ_PRODUCT_ID    _IOR(GSENSOR, 0x0f, int)
#define GSENSOR_MCUBE_IOCTL_READ_FILEPATH      _IOR(GSENSOR, 0x10, char[256])


static int MC3XXX_ReadRegMap(struct i2c_client *client, u8 *pbUserBuf);
static int mc3xxx_chip_init(struct i2c_client *client);
static int MC3XXX_ResetCalibration(struct i2c_client *client);
static int MC3XX0_ValidateSensorIC(unsigned char bPCode);


typedef struct{
	int x;
	int y;
	int z;
}SENSOR_DATA;

static int load_cali_flg = 0;
static int wake_mc3xxx_flg  = 0;

//MCUBE_BACKUP_FILE
static bool READ_FROM_BACKUP = false;
//MCUBE_BACKUP_FILE

#endif

#define MC3XXX_WAKE						1
#define MC3XXX_SNIFF						2
#define MC3XXX_STANDBY					3

struct dev_data {
	struct i2c_client *client;
};

static struct dev_data dev = { 0 };

struct acceleration {
	int x;
	int y;
	int z;
};

struct mc3xxx_data {
	struct mutex lock;
	struct i2c_client *client;
	struct delayed_work  work;
	struct workqueue_struct *mc3xxx_wq;
	struct hrtimer timer;
	struct device *device;
	struct input_dev *input_dev;
	int use_count;
	int enabled;
	volatile unsigned int duration;
	int use_irq; 
	int irq;
	unsigned long irqflags;
	int gpio;
	unsigned int map[3];
	int inv[3];
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
	// for control
	int int_gpio; //0-3
	int op;
	int samp;
	//int xyz_axis[3][3]; // (axis,direction)
	struct proc_dir_entry* sensor_proc;
	int isdbg;
	int sensor_samp; // 
	int sensor_enable;  // 0 --> disable sensor, 1 --> enable sensor
	int test_pass;
	int offset[MC3XXX_AXIS_NUM+1];	/*+1: for 4-byte alignment*/
	s16 data[MC3XXX_AXIS_NUM+1]; 
};

static struct mc3xxx_data l_sensorconfig = {
	.op = 0,
	.int_gpio = 3,
	.samp = 16,
	/*.xyz_axis = {
		{ABS_X, -1},
		{ABS_Y, 1},
		{ABS_Z, -1},
		},*/
	.sensor_proc = NULL,
	.isdbg = 1,
	.sensor_samp = 1,  // 1 sample/second
	.sensor_enable = 1, // enable sensor
	.test_pass = 0, // for test program
	//.offset={0,0,0},
};


//=============================================================================
enum mc3xx0_orientation
{
    MC3XX0_TOP_LEFT_DOWN = 0,
    MC3XX0_TOP_RIGHT_DOWN,
    MC3XX0_TOP_RIGHT_UP,
    MC3XX0_TOP_LEFT_UP,
    MC3XX0_BOTTOM_LEFT_DOWN,
    MC3XX0_BOTTOM_RIGHT_DOWN,
    MC3XX0_BOTTOM_RIGHT_UP,
    MC3XX0_BOTTOM_LEFT_UP
};


struct mc3xx0_hwmsen_convert
{
    signed int sign[3];
    unsigned int map[3];
};

// Transformation matrix for chip mounting position
static const struct mc3xx0_hwmsen_convert mc3xx0_cvt[] =
{
    {{ 1,  1,  1}, {MC3XXX_AXIS_X, MC3XXX_AXIS_Y, MC3XXX_AXIS_Z}},    // 0: top   , left-down
    {{-1,  1,  1}, {MC3XXX_AXIS_Y, MC3XXX_AXIS_X, MC3XXX_AXIS_Z}},    // 1: top   , right-down
    {{-1, -1,  1}, {MC3XXX_AXIS_X, MC3XXX_AXIS_Y, MC3XXX_AXIS_Z}},    // 2: top   , right-up
    {{ 1, -1,  1}, {MC3XXX_AXIS_Y, MC3XXX_AXIS_X, MC3XXX_AXIS_Z}},    // 3: top   , left-up
    {{-1,  1, -1}, {MC3XXX_AXIS_X, MC3XXX_AXIS_Y, MC3XXX_AXIS_Z}},    // 4: bottom, left-down
    {{ 1,  1, -1}, {MC3XXX_AXIS_Y, MC3XXX_AXIS_X, MC3XXX_AXIS_Z}},    // 5: bottom, right-down
    {{ 1, -1, -1}, {MC3XXX_AXIS_X, MC3XXX_AXIS_Y, MC3XXX_AXIS_Z}},    // 6: bottom, right-up
    {{-1, -1, -1}, {MC3XXX_AXIS_Y, MC3XXX_AXIS_X, MC3XXX_AXIS_Z}},    // 7: bottom, left-up
};

//static unsigned char mc3xx0_current_placement = MC3XX0_TOP_RIGHT_UP; // current soldered placement
static struct mc3xx0_hwmsen_convert *pCvt;

#ifdef SUPPORT_VIRTUAL_Z_SENSOR  //add 2013-10-23
int Verify_Z_Railed(int AccData, int resolution)
{
	int status = 0;
	GSE_LOG("%s: AccData = %d",__func__, AccData);
	if(resolution == 1) // Low resolution
	{
		if((AccData >= Low_Pos_Max && AccData >=0)|| (AccData <= Low_Neg_Max && AccData < 0))
		{
			status = 1;
			GSE_LOG("%s: Railed at Low Resolution",__func__);
		}
	}
	else if (resolution == 2)	//High resolution
	{
		if((AccData >= High_Pos_Max && AccData >=0) || (AccData <= High_Neg_Max && AccData < 0))
		{
			status = 1;
			GSE_LOG("%s: Railed at High Resolution",__func__);
		}
	}
	else if (resolution == 3)	//High resolution
	{
		if((AccData >= Low_Pos_Max*3 && AccData >=0) || (AccData <= Low_Neg_Max*3 && AccData < 0))
		{
			status = 1;
			GSE_LOG("%s: Railed at High Resolution",__func__);
		}
	}
	else
		GSE_LOG("%s, Wrong resolution",__func__);

	return status;
}

int SquareRoot(int x) 
{
	int lowerbound;
    int upperbound;
    int root;
	
    if(x < 0) return -1;
    if(x == 0 || x == 1) return x;
    lowerbound = 1;
    upperbound = x;
    root = lowerbound + (upperbound - lowerbound)/2;

    while(root > x/root || root+1 <= x/(root+1))
    {
        if(root > x/root)
        {
            upperbound = root;
        } 
        else 
        {
            lowerbound = root;
        }
        root = lowerbound + (upperbound - lowerbound)/2;
    }
    GSE_LOG("%s: Sqrt root is %d",__func__, root);
    return root;
}
#endif


unsigned int sample_rate_2_memsec(unsigned int rate)
{
	return (1000/rate);
}


//=============================================================================
//volatile static short sensor_duration = SENSOR_DURATION_DEFAULT;
//volatile static short sensor_state_flag = 1;

//=============================================================================
static ssize_t mc3xxx_map_show(struct device *dev, struct device_attribute *attr,char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mc3xxx_data *data = NULL;
	int i = 0;
	data = i2c_get_clientdata(client);
	for (i = 0; i< 3; i++)
	{
		if(data->inv[i] == 1)
		{
			switch(data->map[i])
			{
				case ABS_X:
					buf[i] = 'x';
					break;
				case ABS_Y:
					buf[i] = 'y';
					break;
				case ABS_Z:
					buf[i] = 'z';
					break;
				default:
					buf[i] = '_';
					break;
			}
		}
		else
		{
			switch(data->map[i])
			{
				case ABS_X:
					buf[i] = 'X';
					break;
				case ABS_Y:
					buf[i] = 'Y';
					break;
				case ABS_Z:
					buf[i] = 'Z';
					break;
				default:
					buf[i] = '-';
					break;
			}
		}
	}
	sprintf(buf+3,"\r\n");
	return 5;
}

/*****************************************
 *** show_regiter_map
 *****************************************/
static ssize_t show_regiter_map(struct device *dev, struct device_attribute *attr, char *buf)
{
    u8         _bIndex = 0;
    u8         _baRegMap[64] = { 0 };
    ssize_t    _tLength = 0;

   struct i2c_client *client = to_i2c_client(dev);

    MC3XXX_ReadRegMap(client, _baRegMap);

    for (_bIndex = 0; _bIndex < 64; _bIndex++)
        _tLength += snprintf((buf + _tLength), (PAGE_SIZE - _tLength), "Reg[0x%02X]: 0x%02X\n", _bIndex, _baRegMap[_bIndex]); 

    return (_tLength);
}

static ssize_t mc3xxx_map_store(struct device *dev, struct device_attribute *attr,const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mc3xxx_data *data = NULL;
	int i = 0;
	data = i2c_get_clientdata(client);

	if(count < 3) return -EINVAL;

	for(i = 0; i< 3; i++)
	{
		switch(buf[i])
		{
			case 'x':
				data->map[i] = ABS_X;
				data->inv[i] = 1;
				break;
			case 'y':
				data->map[i] = ABS_Y;
				data->inv[i] = 1;
				break;
			case 'z':
				data->map[i] = ABS_Z;
				data->inv[i] = 1;
				break;
			case 'X':
				data->map[i] = ABS_X;
				data->inv[i] = -1;
				break;
			case 'Y':
				data->map[i] = ABS_Y;
				data->inv[i] = -1;
				break;
			case 'Z':
				data->map[i] = ABS_Z;
				data->inv[i] = -1;
				break;
			default:
				return -EINVAL;
		}
	}

	return count;
}

//=============================================================================
static ssize_t mc3xxx_version_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%s\n", SENSOR_DRIVER_VERSION);
}

//=============================================================================
static ssize_t mc3xxx_chip_id_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    unsigned char bChipID[4] = { 0 };
    struct i2c_client *client = to_i2c_client(dev);

    i2c_smbus_read_i2c_block_data(client, 0x3C, 4, bChipID);

    return sprintf(buf, "%02X-%02X-%02X-%02X\n", bChipID[0], bChipID[1], bChipID[2], bChipID[3]);
}
/*
//=============================================================================
static ssize_t mc3xxx_position_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    printk("%s called\n", __func__);
    return sprintf(buf, "%d\n", mc3xx0_current_placement);
}

//=============================================================================
static ssize_t mc3xxx_position_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    unsigned long position = 0;

    printk("%s called\n", __func__);

    position = simple_strtoul(buf, NULL,10);

    if (position < 8)
        mc3xx0_current_placement = position;

    return count;
}
*/

static int mc3xxx_enable(struct mc3xxx_data *data, int enable)
{
	if(enable)
	{
		msleep(10);
		//mutex_lock(&data->lock);
		mc3xxx_chip_init(data->client);
		//mutex_unlock(&data->lock);
		queue_delayed_work(data->mc3xxx_wq, &data->work, msecs_to_jiffies(sample_rate_2_memsec(data->sensor_samp)));//hrtimer_start(&data->timer, ktime_set(0, sensor_duration*1000000), HRTIMER_MODE_REL);
		data->enabled = true;
	}
	else
	{
		cancel_delayed_work_sync(&l_sensorconfig.work);//hrtimer_cancel(&data->timer);
		data->enabled = false;
	}
	return 0;
}

static ssize_t mc3xxx_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = container_of(mc3xxx_device.parent, struct i2c_client, dev);
	
	struct mc3xxx_data *mc3xxx = i2c_get_clientdata(client);
	

	return sprintf(buf, "%d\n", mc3xxx->enabled);
}

static ssize_t mc3xxx_enable_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	
	bool new_enable;

	struct i2c_client *client = container_of(mc3xxx_device.parent, struct i2c_client, dev);
	
	struct mc3xxx_data *mc3xxx = i2c_get_clientdata(client);

	if (sysfs_streq(buf, "1"))
		new_enable = true;
	else if (sysfs_streq(buf, "0"))
		new_enable = false;
	else 
	{
		pr_debug("%s: invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}

	mc3xxx_enable(mc3xxx, new_enable);

	return count;
}


static DRIVER_ATTR(regmap     ,           S_IRUGO, show_regiter_map,      NULL                  );
static DEVICE_ATTR(map, S_IWUSR | S_IRUGO, mc3xxx_map_show, mc3xxx_map_store);
static DEVICE_ATTR(version , S_IRUGO                    , mc3xxx_version_show , NULL                 );
static DEVICE_ATTR(chipid  , S_IRUGO                    , mc3xxx_chip_id_show , NULL                 );
//static DEVICE_ATTR(position, S_IRUGO | S_IWUSR | S_IWGRP, mc3xxx_position_show, mc3xxx_position_store);
static DEVICE_ATTR(enable,  S_IRUGO|S_IWUSR|S_IWGRP|S_IWOTH, mc3xxx_enable_show, mc3xxx_enable_store);
static struct attribute* mc3xxx_attrs[] =
{
	&driver_attr_regmap,
	&dev_attr_map.attr,
	&dev_attr_version.attr,
	&dev_attr_chipid.attr,
	//&dev_attr_position.attr,
	&dev_attr_enable.attr,
	NULL
};

static const struct attribute_group mc3xxx_group =
{
	.attrs = mc3xxx_attrs,
};

//=============================================================================
static int mc3xxx_chip_init(struct i2c_client *client)
{
	unsigned char data = 0;	
	
	data = i2c_smbus_read_byte_data(client, MC3XXX_REG_PRODUCT_CODE);
	s_bPCODE =data;
	if((data == MC3XXX_PCODE_3230)||(data == MC3XXX_PCODE_3430)
		||(data == MC3XXX_PCODE_3430N)||(data == MC3XXX_PCODE_3530B)
		||((data|0x0E) == MC3XXX_PCODE_3530C))
		Sensor_Accuracy = MCUBE_1_5G_8BIT;	//8bit
	else if((data == MC3XXX_PCODE_3210)||(data == MC3XXX_PCODE_3410)
		||(data == MC3XXX_PCODE_3250)||(data == MC3XXX_PCODE_3410N)
		||(data == MC3XXX_PCODE_3510B)||(data == MC3XXX_PCODE_3510C))
		Sensor_Accuracy = MCUBE_8G_14BIT;	//14bit
	else
		Sensor_Accuracy = 0;

	if (data == MC3XXX_PCODE_3250)
       		 is_mc3250 = 1;

	if ((data == MC3XXX_PCODE_3430N)||(data == MC3XXX_PCODE_3410N))
		is_new_mc34x0 = 1;

	if((MC3XXX_PCODE_3510B == data) || (MC3XXX_PCODE_3510C == data)
		||(data == MC3XXX_PCODE_3530B)||((data|0x0E) == MC3XXX_PCODE_3530C))
		is_mc35xx = 1;
	

	if(MCUBE_8G_14BIT == Sensor_Accuracy)
	{
		data = 0x43;
	  	i2c_smbus_write_byte_data(client, MC3XXX_REG_MODE_FEATURE, data);
		data = 0x00;
	  	i2c_smbus_write_byte_data(client, MC3XXX_REG_SLEEP_COUNT, data);

		data = 0x00;
		if (is_mc35xx)
		{	
			data = 0x0A;
		}	
		
	  	i2c_smbus_write_byte_data(client, MC3XXX_REG_SAMPLE_RATE, data);

		data = 0x3F;
		if ((MC3XXX_PCODE_3510B == s_bPCODE) || (MC3XXX_PCODE_3510C == s_bPCODE))
			data = 0x25;
		else if ((MC3XXX_PCODE_3530B == s_bPCODE) || (MC3XXX_PCODE_3530C == (s_bPCODE|0x0E)))
			data = 0x02;
		
	  	i2c_smbus_write_byte_data(client, MC3XXX_REG_RANGE_CONTROL, data);
		data = 0x00;
	  	i2c_smbus_write_byte_data(client, MC3XXX_REG_TAP_DETECTION_ENABLE, data);
		data = 0x00;
		i2c_smbus_write_byte_data(client, MC3XXX_REG_INTERRUPT_ENABLE, data);	

	        #ifdef DOT_CALI
	            gsensor_gain.x = gsensor_gain.y = gsensor_gain.z = 1024;
	        #endif
	}
	else if(MCUBE_1_5G_8BIT == Sensor_Accuracy)
	{		
		data = 0x43;
		i2c_smbus_write_byte_data(client, MC3XXX_REG_MODE_FEATURE, data);
		data = 0x00;
		i2c_smbus_write_byte_data(client, MC3XXX_REG_SLEEP_COUNT, data);

		data = 0x00;
		if (is_mc35xx)
		{	
			data = 0x0A;
		}
		
		i2c_smbus_write_byte_data(client, MC3XXX_REG_SAMPLE_RATE, data);

		data = 0x32;
		if ((MC3XXX_PCODE_3510B == s_bPCODE) || (MC3XXX_PCODE_3510C == s_bPCODE))
			data = 0x25;
		else if ((MC3XXX_PCODE_3530B == s_bPCODE) || (MC3XXX_PCODE_3530C == (s_bPCODE|0x0E)))
			data = 0x02;
		
		i2c_smbus_write_byte_data(client, MC3XXX_REG_RANGE_CONTROL,data);
		data = 0x00;
		i2c_smbus_write_byte_data(client, MC3XXX_REG_TAP_DETECTION_ENABLE, data);
		data = 0x00;
		i2c_smbus_write_byte_data(client, MC3XXX_REG_INTERRUPT_ENABLE, data);

        #ifdef DOT_CALI
            gsensor_gain.x = gsensor_gain.y = gsensor_gain.z = 86;
            if (is_mc35xx)
            {
            	gsensor_gain.x = gsensor_gain.y = gsensor_gain.z = 64;
            }
        #endif
	}

	data = 0x41;
	i2c_smbus_write_byte_data(client, MC3XXX_REG_MODE_FEATURE, data);	

	return 0;
}

//=============================================================================
int mc3xxx_set_mode(struct i2c_client *client, unsigned char mode) 
{
	int comres = 0;
	unsigned char data = 0;

	if (mode < 4)
	{
		data = (0x40 | mode);
		comres = i2c_smbus_write_byte_data(client, MC3XXX_REG_MODE_FEATURE, data);
	} 

	return comres;
}



#ifdef DOT_CALI
//=============================================================================
struct file *openFile(char *path,int flag,int mode) 
{ 
	struct file *fp = NULL; 
	 
	fp = filp_open(path, flag, mode); 

	if (IS_ERR(fp) || !fp->f_op) 
	{
		GSE_LOG("Calibration File filp_open return NULL\n");
		return NULL; 
	}

	return fp; 
} 
 
//=============================================================================
int readFile(struct file *fp,char *buf,int readlen) 
{ 
	if (fp->f_op && fp->f_op->read) 
		return fp->f_op->read(fp,buf,readlen, &fp->f_pos); 
	else 
		return -1; 
} 

//=============================================================================
int writeFile(struct file *fp,char *buf,int writelen) 
{ 
	if (fp->f_op && fp->f_op->write) 
		return fp->f_op->write(fp,buf,writelen, &fp->f_pos); 
	else 
		return -1; 
}
 
//=============================================================================
int closeFile(struct file *fp) 
{ 
	filp_close(fp,NULL); 

	return 0; 
} 

//=============================================================================
void initKernelEnv(void) 
{ 
	oldfs = get_fs(); 
	set_fs(KERNEL_DS);
	printk(KERN_INFO "initKernelEnv\n");
} 

//=============================================================================
 int MC3XXX_WriteCalibration(struct i2c_client *client, int dat[MC3XXX_AXIS_NUM])
{
	int err = 0;
	u8 buf[9] = { 0 };
	s16 tmp = 0, x_gain = 0, y_gain = 0, z_gain = 0;
	s32 x_off = 0, y_off = 0, z_off = 0;
	int temp_cali_dat[MC3XXX_AXIS_NUM] = { 0 };
	//const struct mc3xx0_hwmsen_convert *pCvt = NULL;

	u8  bMsbFilter       = 0x3F;
	s16 wSignBitMask     = 0x2000;
	s16 wSignPaddingBits = 0xC000;
	s32 dwRangePosLimit  = 0x1FFF;
	s32 dwRangeNegLimit  = -0x2000;

	if (is_mc35xx)
	{
	    bMsbFilter       = 0x7F;
	    wSignBitMask     = 0x4000;
	    wSignPaddingBits = 0x8000;
	    dwRangePosLimit  = 0x3FFF;
	    dwRangeNegLimit  = -0x4000;
	}

	//pCvt = &mc3xx0_cvt[mc3xx0_current_placement];

	temp_cali_dat[pCvt->map[MC3XXX_AXIS_X]] = pCvt->sign[MC3XXX_AXIS_X] * dat[MC3XXX_AXIS_X];
	temp_cali_dat[pCvt->map[MC3XXX_AXIS_Y]] = pCvt->sign[MC3XXX_AXIS_Y] * dat[MC3XXX_AXIS_Y];
	temp_cali_dat[pCvt->map[MC3XXX_AXIS_Z]] = pCvt->sign[MC3XXX_AXIS_Z] * dat[MC3XXX_AXIS_Z];

	if ((is_new_mc34x0)||(is_mc35xx))
	{
	    temp_cali_dat[MC3XXX_AXIS_X] = -temp_cali_dat[MC3XXX_AXIS_X];
	    temp_cali_dat[MC3XXX_AXIS_Y] = -temp_cali_dat[MC3XXX_AXIS_Y];
	}
	else if (is_mc3250)
	{
	    s16    temp = 0;

	    temp = temp_cali_dat[MC3XXX_AXIS_X];

	    temp_cali_dat[MC3XXX_AXIS_X] = -temp_cali_dat[MC3XXX_AXIS_Y];
	    temp_cali_dat[MC3XXX_AXIS_Y] = temp;
	}

	dat[MC3XXX_AXIS_X] = temp_cali_dat[MC3XXX_AXIS_X];
	dat[MC3XXX_AXIS_Y] = temp_cali_dat[MC3XXX_AXIS_Y];
	dat[MC3XXX_AXIS_Z] = temp_cali_dat[MC3XXX_AXIS_Z];

	GSE_LOG("UPDATE dat: (%+3d %+3d %+3d)\n", 
	dat[MC3XXX_AXIS_X], dat[MC3XXX_AXIS_Y], dat[MC3XXX_AXIS_Z]);

    	// read register 0x21~0x29
	err  = i2c_smbus_read_i2c_block_data(client , 0x21 , 3 , &buf[0]);
	err |= i2c_smbus_read_i2c_block_data(client , 0x24 , 3 , &buf[3]);
	err |= i2c_smbus_read_i2c_block_data(client , 0x27 , 3 , &buf[6]);
	
     
	// get x,y,z offset
	tmp = ((buf[1] & bMsbFilter) << 8) + buf[0];
		if (tmp & wSignBitMask)
			tmp |= wSignPaddingBits;
		x_off = tmp;
					
	tmp = ((buf[3] & bMsbFilter) << 8) + buf[2];
		if (tmp & wSignBitMask)
			tmp |= wSignPaddingBits;
		y_off = tmp;
					
	tmp = ((buf[5] & bMsbFilter) << 8) + buf[4];
		if (tmp & wSignBitMask)
			tmp |= wSignPaddingBits;
		z_off = tmp;
					
	// get x,y,z gain
	x_gain = ((buf[1] >> 7) << 8) + buf[6];
	y_gain = ((buf[3] >> 7) << 8) + buf[7];
	z_gain = ((buf[5] >> 7) << 8) + buf[8];
								
	// prepare new offset
	x_off = x_off + 16 * dat[MC3XXX_AXIS_X] * 256 * 128 / 3 / gsensor_gain.x / (40 + x_gain);
	y_off = y_off + 16 * dat[MC3XXX_AXIS_Y] * 256 * 128 / 3 / gsensor_gain.y / (40 + y_gain);
	z_off = z_off + 16 * dat[MC3XXX_AXIS_Z] * 256 * 128 / 3 / gsensor_gain.z / (40 + z_gain);

	//add for over range 
	if( x_off > dwRangePosLimit) 
	{
		x_off = dwRangePosLimit;
	}
	else if( x_off < dwRangeNegLimit)
	{
		x_off = dwRangeNegLimit;
	}

	if( y_off > dwRangePosLimit) 
	{
		y_off = dwRangePosLimit;
	}
	else if( y_off < dwRangeNegLimit)
	{
		y_off = dwRangeNegLimit;
	}

	if( z_off > dwRangePosLimit) 
	{
		z_off = dwRangePosLimit;
	}
	else if( z_off < dwRangeNegLimit)
	{
		z_off = dwRangeNegLimit;
	}

	//storege the cerrunt offset data with DOT format
	offset_data[0] = x_off;
	offset_data[1] = y_off;
	offset_data[2] = z_off;

	//storege the cerrunt Gain data with GOT format
	gain_data[0] = 256*8*128/3/(40+x_gain);
	gain_data[1] = 256*8*128/3/(40+y_gain);
	gain_data[2] = 256*8*128/3/(40+z_gain);
	printk("%d %d ======================\n\n ",gain_data[0],x_gain);

	buf[0] = 0x43;
	i2c_smbus_write_byte_data(client, 0x07, buf[0]);

	buf[0] = x_off & 0xff;
	buf[1] = ((x_off >> 8) & bMsbFilter) | (x_gain & 0x0100 ? 0x80 : 0);
	buf[2] = y_off & 0xff;
	buf[3] = ((y_off >> 8) & bMsbFilter) | (y_gain & 0x0100 ? 0x80 : 0);
	buf[4] = z_off & 0xff;
	buf[5] = ((z_off >> 8) & bMsbFilter) | (z_gain & 0x0100 ? 0x80 : 0);

	i2c_smbus_write_i2c_block_data(client, 0x21,   2, &buf[0]);
	i2c_smbus_write_i2c_block_data(client, 0x21+2, 2, &buf[2]);
	i2c_smbus_write_i2c_block_data(client, 0x21+4, 2, &buf[4]);
	
	buf[0] = 0x41;
	i2c_smbus_write_byte_data(client, 0x07,buf[0]);

    msleep(50);

    return err;

}

int mcube_read_cali_file(struct i2c_client *client)
{
	int cali_data[3] = { 0 };
	int err =0;
	//char buf[64];
	printk("%s %d\n",__func__,__LINE__);
			//MCUBE_BACKUP_FILE
	READ_FROM_BACKUP = false;
	//MCUBE_BACKUP_FILE
	initKernelEnv();
	fd_file = openFile(CALIB_PATH,O_RDONLY,0); 
			//MCUBE_BACKUP_FILE
	if (fd_file == NULL) 
	{
		fd_file = openFile(BACKUP_CALIB_PATH, O_RDONLY, 0); 
		if(fd_file != NULL)
		{
			READ_FROM_BACKUP = true;
		}
	}
	//MCUBE_BACKUP_FILE
	if (fd_file == NULL) 
	{
		GSE_LOG("fail to open\n");
		cali_data[0] = 0;
		cali_data[1] = 0;
		cali_data[2] = 0;

		return -1;
	}
	else
	{
		printk("%s %d\n",__func__,__LINE__);
		memset(backup_buf,0,64); 
		if ((err = readFile(fd_file,backup_buf,128))>0) 
			GSE_LOG("buf:%s\n",backup_buf); 
		else 
			GSE_LOG("read file error %d\n",err); 
		printk("%s %d\n",__func__,__LINE__);

		set_fs(oldfs); 
		closeFile(fd_file); 

		sscanf(backup_buf, "%d %d %d",&cali_data[MC3XXX_AXIS_X], &cali_data[MC3XXX_AXIS_Y], &cali_data[MC3XXX_AXIS_Z]);
		GSE_LOG("cali_data: %d %d %d\n", cali_data[MC3XXX_AXIS_X], cali_data[MC3XXX_AXIS_Y], cali_data[MC3XXX_AXIS_Z]); 	
				
		MC3XXX_WriteCalibration(client, cali_data);
	}
	return 0;
}

//=============================================================================
static int mcube_write_log_data(struct i2c_client *client, u8 data[0x3f])
{
	#define _WRT_LOG_DATA_BUFFER_SIZE    (66 * 50)

	s16 rbm_data[3]={0}, raw_data[3]={0};
	int err =0;
	char *_pszBuffer = NULL;
	int n=0,i=0;

	initKernelEnv();
	fd_file = openFile(DATA_PATH ,O_RDWR | O_CREAT,0); 
	if (fd_file == NULL) 
	{
		GSE_LOG("mcube_write_log_data fail to open\n");	
	}
	else
	{
		rbm_data[MC3XXX_AXIS_X] = (s16)((data[0x0d]) | (data[0x0e] << 8));
		rbm_data[MC3XXX_AXIS_Y] = (s16)((data[0x0f]) | (data[0x10] << 8));
		rbm_data[MC3XXX_AXIS_Z] = (s16)((data[0x11]) | (data[0x12] << 8));

		raw_data[MC3XXX_AXIS_X] = (rbm_data[MC3XXX_AXIS_X] + offset_data[0]/2)*gsensor_gain.x/gain_data[0];
		raw_data[MC3XXX_AXIS_Y] = (rbm_data[MC3XXX_AXIS_Y] + offset_data[1]/2)*gsensor_gain.y/gain_data[1];
		raw_data[MC3XXX_AXIS_Z] = (rbm_data[MC3XXX_AXIS_Z] + offset_data[2]/2)*gsensor_gain.z/gain_data[2];

		_pszBuffer = kzalloc(_WRT_LOG_DATA_BUFFER_SIZE, GFP_KERNEL);
		if (NULL == _pszBuffer)
		{
			GSE_ERR("fail to allocate memory for buffer\n");
    		closeFile(fd_file); 
			return -1;
		}
		memset(_pszBuffer, 0, _WRT_LOG_DATA_BUFFER_SIZE); 

		n += sprintf(_pszBuffer+n, "G-sensor RAW X = %d  Y = %d  Z = %d\n", raw_data[0] ,raw_data[1] ,raw_data[2]);
		n += sprintf(_pszBuffer+n, "G-sensor RBM X = %d  Y = %d  Z = %d\n", rbm_data[0] ,rbm_data[1] ,rbm_data[2]);
		for(i=0; i<64; i++)
		{
		n += sprintf(_pszBuffer+n, "mCube register map Register[%x] = 0x%x\n",i,data[i]);
		}
		msleep(50);		
		if ((err = writeFile(fd_file,_pszBuffer,n))>0) 
			GSE_LOG("buf:%s\n",_pszBuffer); 
		else 
			GSE_LOG("write file error %d\n",err); 

		kfree(_pszBuffer);

		set_fs(oldfs); 
		closeFile(fd_file); 
	}
	return 0;
}

//=============================================================================
void MC3XXX_rbm(struct i2c_client *client, int enable)
{
	char buf1[3] = { 0 };
	if(enable == 1 )
	{
		buf1[0] = 0x43; 
		i2c_smbus_write_byte_data(client, 0x07, buf1[0]);

		buf1[0] = 0x6D; 
		i2c_smbus_write_byte_data(client, 0x1B, buf1[0]);

		buf1[0] = 0x43; 
		i2c_smbus_write_byte_data(client, 0x1B, buf1[0]);

		buf1[0] = 0x00; 
		i2c_smbus_write_byte_data(client, 0x3B, buf1[0]);

		buf1[0] = 0x02; 
		i2c_smbus_write_byte_data(client, 0x14, buf1[0]);

		buf1[0] = 0x41; 
		i2c_smbus_write_byte_data(client, 0x07, buf1[0]);

		enable_RBM_calibration = 1;

		GSE_LOG("set rbm!!\n");

		msleep(10);
    	}
	else if(enable == 0 )  
    	{
		buf1[0] = 0x43; 
		i2c_smbus_write_byte_data(client, 0x07, buf1[0]);

		buf1[0] = 0x00; 
		i2c_smbus_write_byte_data(client, 0x14, buf1[0]);
		GSE_LOG("set rbm!! %x @@@@\n",s_bPCODE);

		buf1[0] = s_bPCODE; 
		i2c_smbus_write_byte_data(client, 0x3B, buf1[0]);

		buf1[0] = 0x6D; 
		i2c_smbus_write_byte_data(client, 0x1B, buf1[0]);

		buf1[0] = 0x43; 
		i2c_smbus_write_byte_data(client, 0x1B, buf1[0]);

		buf1[0] = 0x41; 
		i2c_smbus_write_byte_data(client, 0x07, buf1[0]);

		enable_RBM_calibration = 0;

		GSE_LOG("clear rbm!!\n");

		msleep(10);
	}
}

/*----------------------------------------------------------------------------*/
 int MC3XXX_ReadData_RBM(struct i2c_client *client,int data[MC3XXX_AXIS_NUM])
{   
	u8 addr = 0x0d;
	u8 rbm_buf[MC3XXX_DATA_LEN] = {0};
	int err = 0;

	//err = p_mc3xxx->MC3XXX_BUS_READ_FUNC(p_mc3xxx->dev_addr, addr, &rbm_buf[0],6);
	err = i2c_smbus_read_i2c_block_data(client , addr , 6 , rbm_buf);
	//err = mc3xxx_read_block(client, addr, rbm_buf, 0x06);

	data[MC3XXX_AXIS_X] = (s16)((rbm_buf[0]) | (rbm_buf[1] << 8));
	data[MC3XXX_AXIS_Y] = (s16)((rbm_buf[2]) | (rbm_buf[3] << 8));
	data[MC3XXX_AXIS_Z] = (s16)((rbm_buf[4]) | (rbm_buf[5] << 8));

	GSE_LOG("rbm_buf<<<<<[%02x %02x %02x %02x %02x %02x]\n",rbm_buf[0], rbm_buf[2], rbm_buf[2], rbm_buf[3], rbm_buf[4], rbm_buf[5]);
	GSE_LOG("RBM<<<<<[%04x %04x %04x]\n", data[MC3XXX_AXIS_X], data[MC3XXX_AXIS_Y], data[MC3XXX_AXIS_Z]);
	GSE_LOG("RBM<<<<<[%04d %04d %04d]\n", data[MC3XXX_AXIS_X], data[MC3XXX_AXIS_Y], data[MC3XXX_AXIS_Z]);		
	return err;
}


 int MC3XXX_ReadRBMData(struct i2c_client *client, char *buf)
{
	int res = 0;
	int data[3];

	if (!buf)
	{
		return EINVAL;
	}
	
	mc3xxx_set_mode(client,MC3XXX_WAKE);

	res = MC3XXX_ReadData_RBM(client,data);

	if(res)
	{        
		GSE_ERR("%s I2C error: ret value=%d",__func__, res);
		return EIO;
	}
	else
	{
		sprintf(buf, "%04x %04x %04x", data[MC3XXX_AXIS_X], 
			data[MC3XXX_AXIS_Y], data[MC3XXX_AXIS_Z]);
	
	}
	
	return 0;
}
 int MC3XXX_ReadOffset(struct i2c_client *client,s16 ofs[MC3XXX_AXIS_NUM])
{    
	int err = 0;
	u8 off_data[6] = { 0 };

	if(Sensor_Accuracy == MCUBE_8G_14BIT)
	{
		err = i2c_smbus_read_i2c_block_data(client, MC3XXX_REG_XOUT_EX_L, MC3XXX_DATA_LEN, off_data);

		ofs[MC3XXX_AXIS_X] = ((s16)(off_data[0]))|((s16)(off_data[1])<<8);
		ofs[MC3XXX_AXIS_Y] = ((s16)(off_data[2]))|((s16)(off_data[3])<<8);
		ofs[MC3XXX_AXIS_Z] = ((s16)(off_data[4]))|((s16)(off_data[5])<<8);
	}
	else if(Sensor_Accuracy == MCUBE_1_5G_8BIT) 
	{
		err = i2c_smbus_read_i2c_block_data(client, 0, 3, off_data);

		ofs[MC3XXX_AXIS_X] = (s8)off_data[0];
		ofs[MC3XXX_AXIS_Y] = (s8)off_data[1];
		ofs[MC3XXX_AXIS_Z] = (s8)off_data[2];			
	}

	GSE_LOG("MC3XXX_ReadOffset %d %d %d\n", ofs[MC3XXX_AXIS_X], ofs[MC3XXX_AXIS_Y], ofs[MC3XXX_AXIS_Z]);

    return err;  
}
/*----------------------------------------------------------------------------*/
 static int MC3XXX_ResetCalibration(struct i2c_client *client)
{
	u8 buf[6] = { 0 };
	s16 tmp = 0;
	int err = 0;

	u8  bMsbFilter       = 0x3F;
	s16 wSignBitMask     = 0x2000;
	s16 wSignPaddingBits = 0xC000;

	buf[0] = 0x43;
	err = i2c_smbus_write_byte_data(client, 0x07, buf[0]);
	if(err)
	{
		GSE_ERR("error 0x07: %d\n", err);
	}

	err = i2c_smbus_write_i2c_block_data(client, 0x21, 6, offset_buf);
	if(err)
	{
		GSE_ERR("error: %d\n", err);
	}
	
	buf[0] = 0x41;
	err = i2c_smbus_write_byte_data(client, 0x07, buf[0]);
	if(err)
	{
		GSE_ERR("error: %d\n", err);
	}

	msleep(20);


	if (is_mc35xx)
	{
	    bMsbFilter       = 0x7F;
	    wSignBitMask     = 0x4000;
	    wSignPaddingBits = 0x8000;
	}

	tmp = ((offset_buf[1] & bMsbFilter) << 8) + offset_buf[0];
	if (tmp & wSignBitMask)
		tmp |= wSignPaddingBits;
	offset_data[0] = tmp;
					
	tmp = ((offset_buf[3] & bMsbFilter) << 8) + offset_buf[2];
	if (tmp & wSignBitMask)
			tmp |= wSignPaddingBits;
	offset_data[1] = tmp;
					
	tmp = ((offset_buf[5] & bMsbFilter) << 8) + offset_buf[4];
	if (tmp & wSignBitMask)
		tmp |= wSignPaddingBits;
	offset_data[2] = tmp;	

	return 0;  
}

//=============================================================================
 int MC3XXX_ReadCalibration(struct i2c_client *client,int dat[MC3XXX_AXIS_NUM])
{
	signed short MC_offset[MC3XXX_AXIS_NUM + 1] = { 0 };    // +1: for 4-byte alignment
	int err = 0;

	memset(MC_offset, 0, sizeof(MC_offset));

	err = MC3XXX_ReadOffset(client, MC_offset);

	if (err)
	{
		GSE_ERR("read offset fail, %d\n", err);
		return err;
	}    

	dat[MC3XXX_AXIS_X] = MC_offset[MC3XXX_AXIS_X];
	dat[MC3XXX_AXIS_Y] = MC_offset[MC3XXX_AXIS_Y];
	dat[MC3XXX_AXIS_Z] = MC_offset[MC3XXX_AXIS_Z];  
	                                  
	return 0;
}

//=============================================================================
int MC3XXX_ReadData(struct i2c_client *client, s16 buffer[MC3XXX_AXIS_NUM])
{
	unsigned char buf[6] = { 0 };
	signed char buf1[6] = { 0 };
	char rbm_buf[6] = { 0 };
	int ret = 0;

	#ifdef SUPPORT_VIRTUAL_Z_SENSOR
	int tempX=0;
	int tempY=0;
	int tempZ=0;
	#endif

	if (enable_RBM_calibration == 0)
	{
		//err = hwmsen_read_block(client, addr, buf, 0x06);
	}
	else if (enable_RBM_calibration == 1)
	{		
		memset(rbm_buf, 0, 6);
		i2c_smbus_read_i2c_block_data(client, 0x0d  , 2, &rbm_buf[0]);
		i2c_smbus_read_i2c_block_data(client, 0x0d+2, 2, &rbm_buf[2]);
		i2c_smbus_read_i2c_block_data(client, 0x0d+4, 2, &rbm_buf[4]);
	}

	if (enable_RBM_calibration == 0)
	{
		if(Sensor_Accuracy == MCUBE_8G_14BIT)
		{
			ret = i2c_smbus_read_i2c_block_data(client, MC3XXX_REG_XOUT_EX_L, 6, buf);
			
			buffer[0] = (signed short)((buf[0])|(buf[1]<<8));
			buffer[1] = (signed short)((buf[2])|(buf[3]<<8));
			buffer[2] = (signed short)((buf[4])|(buf[5]<<8));
		}
		else if(Sensor_Accuracy == MCUBE_1_5G_8BIT)
		{
			ret = i2c_smbus_read_i2c_block_data(client, MC3XXX_REG_XOUT, 3, buf1);
				
			buffer[0] = (signed short)buf1[0];
			buffer[1] = (signed short)buf1[1];
			buffer[2] = (signed short)buf1[2];
		}

		#ifdef SUPPORT_VIRTUAL_Z_SENSOR //add 2013-10-23
		if (g_virtual_z)
		{
			//printk("%s 1\n", __FUNCTION__);
			
			tempX = buffer[MC3XXX_AXIS_X];
			tempY = buffer[MC3XXX_AXIS_Y];
			tempZ = buffer[MC3XXX_AXIS_Z];
			//printk(" %d:Verify_Z_Railed() %d\n", (int)buffer[MC32X0_AXIS_Z], Verify_Z_Railed((int)buffer[MC32X0_AXIS_Z], LOW_RESOLUTION));
			if(1 == Verify_Z_Railed((int)buffer[MC3XXX_AXIS_Z], LOW_RESOLUTION)) // z-railed
			{
				Railed = 1;
				
				GSE_LOG("%s: Z railed", __func__);
				//printk("%s: Z railed \n", __func__);
				if (G_2_REVERSE_VIRTUAL_Z == 1)
					buffer[MC3XXX_AXIS_Z] = (s8) (  gsensor_gain.z - (abs(tempX) + abs(tempY)));
				else
					buffer[MC3XXX_AXIS_Z] = (s8) -(  gsensor_gain.z - (abs(tempX) + abs(tempY)));
			}
						else
			{
				Railed = 0;	
			}
		}	
		#endif	
		mcprintkreg("MC3XXX_ReadData: %d %d %d\n", buffer[0], buffer[1], buffer[2]);
	}
	else if (enable_RBM_calibration == 1)
	{
		buffer[MC3XXX_AXIS_X] = (s16)((rbm_buf[0]) | (rbm_buf[1] << 8));
		buffer[MC3XXX_AXIS_Y] = (s16)((rbm_buf[2]) | (rbm_buf[3] << 8));
		buffer[MC3XXX_AXIS_Z] = (s16)((rbm_buf[4]) | (rbm_buf[5] << 8));

		GSE_LOG("%s RBM<<<<<[%08d %08d %08d]\n", __func__, buffer[MC3XXX_AXIS_X], buffer[MC3XXX_AXIS_Y], buffer[MC3XXX_AXIS_Z]);

		if(gain_data[0] == 0)
		{
			buffer[MC3XXX_AXIS_X] = 0;
			buffer[MC3XXX_AXIS_Y] = 0;
			buffer[MC3XXX_AXIS_Z] = 0;

			return 0;
		}

		buffer[MC3XXX_AXIS_X] = (buffer[MC3XXX_AXIS_X] + offset_data[0]/2)*gsensor_gain.x/gain_data[0];
		buffer[MC3XXX_AXIS_Y] = (buffer[MC3XXX_AXIS_Y] + offset_data[1]/2)*gsensor_gain.y/gain_data[1];
		buffer[MC3XXX_AXIS_Z] = (buffer[MC3XXX_AXIS_Z] + offset_data[2]/2)*gsensor_gain.z/gain_data[2];

		#ifdef SUPPORT_VIRTUAL_Z_SENSOR  // add 2013-10-23
		if (g_virtual_z)
		{
			tempX = buffer[MC3XXX_AXIS_X];
			tempY = buffer[MC3XXX_AXIS_Y];
			tempZ = buffer[MC3XXX_AXIS_Z];
			//printk("%s 2\n", __FUNCTION__);
			GSE_LOG("Original RBM<<<<<[%08d %08d %08d]\n", buffer[MC3XXX_AXIS_X], buffer[MC3XXX_AXIS_Y], buffer[MC3XXX_AXIS_Z]);
			printk("Verify_Z_Railed() %d\n", Verify_Z_Railed((int)buffer[MC3XXX_AXIS_Z], RBM_RESOLUTION));
			if(1 == Verify_Z_Railed(buffer[MC3XXX_AXIS_Z], RBM_RESOLUTION)) // z-railed
			{
				GSE_LOG("%s: Z Railed in RBM mode",__FUNCTION__);
				//printk("%s: Z Railed in RBM mode\n",__FUNCTION__);
				if (G_2_REVERSE_VIRTUAL_Z == 1)
					buffer[MC3XXX_AXIS_Z] = (s16) (  gsensor_gain.z - (abs(tempX) + abs(tempY)));
				else
					buffer[MC3XXX_AXIS_Z] = (s16) -(  gsensor_gain.z - (abs(tempX) + abs(tempY)));
			}
			GSE_LOG("RBM<<<<<[%08d %08d %08d]\n", buffer[MC3XXX_AXIS_X], buffer[MC3XXX_AXIS_Y], buffer[MC3XXX_AXIS_Z]);
		}
		#endif
		
		GSE_LOG("%s offset_data <<<<<[%d %d %d]\n", __func__, offset_data[0], offset_data[1], offset_data[2]);
		GSE_LOG("%s gsensor_gain <<<<<[%d %d %d]\n", __func__, gsensor_gain.x, gsensor_gain.y, gsensor_gain.z);
		GSE_LOG("%s gain_data <<<<<[%d %d %d]\n", __func__, gain_data[0], gain_data[1], gain_data[2]);
		GSE_LOG("%s RBM->RAW <<<<<[%d %d %d]\n", __func__, buffer[MC3XXX_AXIS_X], buffer[MC3XXX_AXIS_Y], buffer[MC3XXX_AXIS_Z]);
	}
	
	return 0;
}

//=============================================================================
int MC3XXX_ReadRawData(struct i2c_client *client,  char * buf)
{
	int res = 0;
	s16 raw_buf[3] = { 0 };

	if (!buf || !client)
	{
		return -EINVAL;
	}
	
	mc3xxx_set_mode(client, MC3XXX_WAKE);
	res = MC3XXX_ReadData(client,&raw_buf[0]);
	if(res)
	{     
	printk("%s %d\n",__FUNCTION__, __LINE__);
		GSE_ERR("I2C error: ret value=%d", res);
		return -EIO;
	}
	else
	{
		//const struct mc3xx0_hwmsen_convert *pCvt = &mc3xx0_cvt[mc3xx0_current_placement];

		GSE_LOG("UPDATE dat: (%+3d %+3d %+3d)\n", 
		raw_buf[MC3XXX_AXIS_X], raw_buf[MC3XXX_AXIS_Y], raw_buf[MC3XXX_AXIS_Z]);

	        if ((is_new_mc34x0)||(is_mc35xx))
	        {
	            raw_buf[MC3XXX_AXIS_X] = -raw_buf[MC3XXX_AXIS_X];
	            raw_buf[MC3XXX_AXIS_Y] = -raw_buf[MC3XXX_AXIS_Y];
	        }
	        else if (is_mc3250)
	        {
	            s16    temp = 0;

	            temp = raw_buf[MC3XXX_AXIS_X];

	            raw_buf[MC3XXX_AXIS_X] = raw_buf[MC3XXX_AXIS_Y];
	            raw_buf[MC3XXX_AXIS_Y] = -temp;
	        }

	        G_RAW_DATA[MC3XXX_AXIS_X] = pCvt->sign[MC3XXX_AXIS_X] * raw_buf[pCvt->map[MC3XXX_AXIS_X]];
	        G_RAW_DATA[MC3XXX_AXIS_Y] = pCvt->sign[MC3XXX_AXIS_Y] * raw_buf[pCvt->map[MC3XXX_AXIS_Y]];
	        G_RAW_DATA[MC3XXX_AXIS_Z] = pCvt->sign[MC3XXX_AXIS_Z] * raw_buf[pCvt->map[MC3XXX_AXIS_Z]];

		G_RAW_DATA[MC3XXX_AXIS_Z] += gsensor_gain.z*(pCvt->sign[MC3XXX_AXIS_Z])*(1);//G_RAW_DATA[MC3XXX_AXIS_Z]+gsensor_gain.z;

		sprintf(buf, "%04x %04x %04x", G_RAW_DATA[MC3XXX_AXIS_X], 
				G_RAW_DATA[MC3XXX_AXIS_Y], G_RAW_DATA[MC3XXX_AXIS_Z]);

		GSE_LOG("G_RAW_DATA: (%+3d %+3d %+3d)\n", 
		G_RAW_DATA[MC3XXX_AXIS_X], G_RAW_DATA[MC3XXX_AXIS_Y], G_RAW_DATA[MC3XXX_AXIS_Z]);
	}

	return 0;
}

//=============================================================================
static int MC3XXX_ReadRegMap(struct i2c_client *client, u8 *pbUserBuf)
{
	u8 data[128] = {0};
	//u8 addr = 0x00;
	int err = 0;
	int i = 0;

	if(NULL == client)
	{
		err = -EINVAL;
		return err;
	}


	for(i = 0; i < 64; i++)
	{
		data[i] = i2c_smbus_read_byte_data(client, i);
		printk(KERN_INFO "mcube register map Register[%x] = 0x%x\n", i ,data[i]);
	}

	msleep(50);	
	
	mcube_write_log_data(client, data);

	msleep(50);
   
	if (NULL != pbUserBuf)
	{
		printk(KERN_INFO "copy to user buffer\n");
		memcpy(pbUserBuf, data, 64);
	}
      	
	return err;
}

//=============================================================================
void MC3XXX_Reset(struct i2c_client *client) 
{
	//s16 tmp = 0, x_gain = 0, y_gain = 0, z_gain = 0;
	u8 buf[3] = { 0 };
	int err = 0;

	buf[0] = 0x43;
  	i2c_smbus_write_byte_data(client, 0x07, buf[0]);

	i2c_smbus_read_i2c_block_data(client, 0x04, 1, buf);

	if (0x00 == (buf[0] & 0x40))
	{
		buf[0] = 0x6d;
		i2c_smbus_write_byte_data(client, 0x1b, buf[0]);
		
		buf[0] = 0x43;
		i2c_smbus_write_byte_data(client, 0x1b, buf[0]);
	}
	
	msleep(5);
	
	buf[0] = 0x43;
  	i2c_smbus_write_byte_data(client, 0x07, buf[0]);

	buf[0] = 0x80;
  	i2c_smbus_write_byte_data(client, 0x1c, buf[0]);

	buf[0] = 0x80;
  	i2c_smbus_write_byte_data(client, 0x17, buf[0]);

	msleep(5);

	buf[0] = 0x00;
  	i2c_smbus_write_byte_data(client, 0x1c, buf[0]);

	buf[0] = 0x00;
  	i2c_smbus_write_byte_data(client, 0x17, buf[0]);

	msleep(5);

	memset(offset_buf, 0, sizeof(offset_buf));

	err = i2c_smbus_read_i2c_block_data(client, 0x21, 6, offset_buf);
	
	i2c_smbus_read_i2c_block_data(client, 0x04, 1, buf);

	if (0x00 == (buf[0] & 0x40))
	{
		buf[0] = 0x6d;
		i2c_smbus_write_byte_data(client, 0x1b, buf[0]);
		
		buf[0] = 0x43;
		i2c_smbus_write_byte_data(client, 0x1b, buf[0]);
	}

	buf[0] = 0x41;
	i2c_smbus_write_byte_data(client, 0x07, buf[0]);
	
}
#endif

int mc3xxx_read_accel_xyz(struct i2c_client *client, s16 * acc)
{
	int comres = 0;
	s16 raw_data[MC3XXX_AXIS_NUM] = { 0 };
	//const struct mc3xx0_hwmsen_convert *pCvt = &mc3xx0_cvt[mc3xx0_current_placement];

#ifdef DOT_CALI
        s16 raw_buf[6] = { 0 };
        
        comres = MC3XXX_ReadData(client, &raw_buf[0]);
        
        raw_data[MC3XXX_AXIS_X] = raw_buf[0];
        raw_data[MC3XXX_AXIS_Y] = raw_buf[1];
        raw_data[MC3XXX_AXIS_Z] = raw_buf[2];
#else
        unsigned char raw_buf[6] = { 0 };
        signed char raw_buf1[3] = { 0 };

        if(Sensor_Accuracy == MCUBE_8G_14BIT)
        {
            comres = i2c_smbus_read_i2c_block_data(client, MC3XXX_REG_XOUT_EX_L, 6, raw_buf);
            
            raw_data[MC3XXX_AXIS_X] = (signed short)((raw_buf[0])|(raw_buf[1]<<8));
            raw_data[MC3XXX_AXIS_Y] = (signed short)((raw_buf[2])|(raw_buf[3]<<8));
            raw_data[MC3XXX_AXIS_Z] = (signed short)((raw_buf[4])|(raw_buf[5]<<8));
        }
        else if(Sensor_Accuracy == MCUBE_1_5G_8BIT)
        {
            comres = i2c_smbus_read_i2c_block_data(client, MC3XXX_REG_XOUT, 3, raw_buf1);
            
            raw_data[MC3XXX_AXIS_X] = (signed short)raw_buf1[0];
            raw_data[MC3XXX_AXIS_Y] = (signed short)raw_buf1[1];
            raw_data[MC3XXX_AXIS_Z] = (signed short)raw_buf1[2];
        }
#endif

	if((is_new_mc34x0)||(is_mc35xx))
	{
		raw_data[MC3XXX_AXIS_X] = -raw_data[MC3XXX_AXIS_X];
		raw_data[MC3XXX_AXIS_Y] = -raw_data[MC3XXX_AXIS_Y];
	}
	else if (is_mc3250)
	{
		s16    temp = 0;

		temp = raw_data[MC3XXX_AXIS_X];

		raw_data[MC3XXX_AXIS_X] = raw_data[MC3XXX_AXIS_Y];
		raw_data[MC3XXX_AXIS_Y] = -temp;
	}
	//printk("%s:%d %d %d\n",__FUNCTION__,raw_data[0],raw_data[1],raw_data[2]);
	acc[MC3XXX_AXIS_X] = pCvt->sign[MC3XXX_AXIS_X] * raw_data[pCvt->map[MC3XXX_AXIS_X]];
	acc[MC3XXX_AXIS_Y] = pCvt->sign[MC3XXX_AXIS_Y] * raw_data[pCvt->map[MC3XXX_AXIS_Y]];
	acc[MC3XXX_AXIS_Z] = pCvt->sign[MC3XXX_AXIS_Z] * raw_data[pCvt->map[MC3XXX_AXIS_Z]];

	return comres;	
}

//=============================================================================
static void mc3xxx_work_func(struct work_struct *work)
{
	struct mc3xxx_data *data = &l_sensorconfig;//container_of(work, struct mc3xxx_data, work);
	//int ret = 0;
	s16 raw[3] = { 0 };
	
#ifdef DOT_CALI
        if( load_cali_flg > 0)
        {
           /* ret = mcube_read_cali_file(data->client);

            if(ret == 0)
                load_cali_flg = ret;
            else 
                load_cali_flg--;

            GSE_LOG("load_cali %d\n",ret); */
            MC3XXX_WriteCalibration(data->client,l_sensorconfig.offset);
			load_cali_flg = 0;
        }  
#endif
#if 1
//gsensor not use when resume
    if(wake_mc3xxx_flg==1){
		wake_mc3xxx_flg=0;
		

	mc3xxx_chip_init(data->client);
	MC3XXX_ResetCalibration(data->client);

	MC3XXX_WriteCalibration(data->client,l_sensorconfig.offset);
	/*ret =mcube_read_cali_file(data->client);
	if(ret !=0)
		printk("*load_cali %d\n",ret); */
	}
#endif

	mc3xxx_read_accel_xyz(data->client, &raw[0]);
	//printk("%s:%d %d %d\n",__FUNCTION__,raw[0],raw[1],raw[2]);
	input_report_abs(data->input_dev, ABS_X, raw[0]);
	input_report_abs(data->input_dev, ABS_Y, raw[1]);
	input_report_abs(data->input_dev, ABS_Z, raw[2]);
	input_sync(data->input_dev);

	queue_delayed_work(data->mc3xxx_wq, &data->work, msecs_to_jiffies(sample_rate_2_memsec(data->sensor_samp)));
}
/*
//=============================================================================
static enum hrtimer_restart mc3xxx_timer_func(struct hrtimer *timer)
{
	struct mc3xxx_data *data = container_of(timer, struct mc3xxx_data, timer);

	queue_work(data->mc3xxx_wq, &data->work);

	hrtimer_start(&data->timer, ktime_set(0, sensor_duration*1000000), HRTIMER_MODE_REL);

	return HRTIMER_NORESTART;
}
*/
//MCUBE_BACKUP_FILE
static void mcube_copy_file(const char *dstFilePath)
{
	int err =0;
	initKernelEnv();

	fd_file = openFile(dstFilePath,O_RDWR,0); 
	if (fd_file == NULL) 
	{
		GSE_LOG("open %s fail\n",dstFilePath);  
		return;
	}

	if ((err = writeFile(fd_file,backup_buf,64))>0) 
		GSE_LOG("buf:%s\n",backup_buf); 
	else 
		GSE_LOG("write file error %d\n",err);

	set_fs(oldfs); ; 
	closeFile(fd_file); 

}
//MCUBE_BACKUP_FILE

extern int wmt_setsyspara(char *varname, char *varval);
static void update_var(void)
{
	char varbuf[64];
	int varlen;

	memset(varbuf, 0, sizeof(varbuf));
	varlen = sizeof(varbuf);

	sprintf(varbuf, "%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d",
				l_sensorconfig.op,
				l_sensorconfig.int_gpio,
				l_sensorconfig.samp,
				(pCvt->map[MC3XXX_AXIS_X]),
				(pCvt->sign[MC3XXX_AXIS_X]),
				(pCvt->map[MC3XXX_AXIS_Y]),
				(pCvt->sign[MC3XXX_AXIS_Y]),
				(pCvt->map[MC3XXX_AXIS_Z]),
				(pCvt->sign[MC3XXX_AXIS_Z]),
				l_sensorconfig.offset[0],
				l_sensorconfig.offset[1],
				l_sensorconfig.offset[2]
			);

	wmt_setsyspara("wmt.io.mc3230sensor",varbuf);
}

static long wmt_mc3xxx_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	short enable = 0;
	short delay = 0;
	unsigned int    temp;
	
	switch (cmd){

	case ECS_IOCTL_APP_SET_AFLAG:
			// enable/disable sensor			
			if (copy_from_user(&enable, (short*)arg, sizeof(short)))
			{
				errlog("Can't get enable flag!!!\n");
				return -EFAULT;			
			}
			if ((enable >=0) && (enable <=1))
			{
				dbg("driver: disable/enable(%d) gsensor. l_sensorconfig.sensor_samp=%d\n", enable, l_sensorconfig.sensor_samp);
			
				if (enable != l_sensorconfig.sensor_enable)
				{
				
					l_sensorconfig.sensor_enable = enable;
				
				}			
			} else {
				errlog("Wrong enable argument!!!\n");
				return -EFAULT;
			}
			break;
	case ECS_IOCTL_APP_SET_DELAY://IOCTL_SENSOR_SET_DELAY_ACCEL:
			// set the rate of g-sensor
			if (copy_from_user(&delay,(short*)arg, sizeof(short)))
			{
				errlog("Can't get set delay!!!\n");
				return -EFAULT;
			}
			dbg("Get delay=%d \n", delay);
			
			if ((delay >=0) && (delay < 20))
			{
				delay = 20;
			} else if (delay > 200) 
			{
				delay = 200;
			}
			if (delay > 0)
			{
				l_sensorconfig.sensor_samp = 1000/delay;
			} else {
				errlog("error delay argument(delay=%d)!!!\n",delay);
				return -EFAULT;
			}
		
			break;
		case WMT_IOCTL_SENSOR_GET_DRVID:
			temp = MC3230_DRVID;
			if (copy_to_user((unsigned int*)arg, &temp, sizeof(unsigned int)))
			{
				return -EFAULT;
			}
			dbg("mc32x0_driver_id:%d\n",temp);
			break;
		case WMT_IOCTL_SENOR_GET_RESOLUTION:		
			if(Sensor_Accuracy &MCUBE_1_5G_8BIT){
				if(is_mc35xx)		//mc3236:8 bit ,+/-2g
					temp = (8<<8) | 4;
				else
					temp = (8<<8) | 3; //mc3230:8 bit ,+/-1.5g

			}
			if (copy_to_user((unsigned int *)arg, &temp, sizeof(unsigned int)))
			{
				return -EFAULT;
			}
			printk("<<<<<<<resolution:0x%x\n",temp);
			break;
		default:
			return -EINVAL;
			break;
	}
	return 0;
}


static long mc3xxx_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	//int intBuf[SENSOR_DATA_SIZE] = { 0 };
	int ret = 0;
	float convert_para = 0.0f;
	
	int prod = -1;
	

#ifdef DOT_CALI
        void __user *data1 = NULL;
        char strbuf[256] = { 0 };
        //int cali[3] = { 0 };
        SENSOR_DATA sensor_data = { 0 };
        struct i2c_client *client = container_of(mc3xxx_device.parent, struct i2c_client, dev);
#endif

	struct mc3xxx_data *data = NULL;

	data = i2c_get_clientdata(client);

	switch (cmd) {
		
		
/*		case IOCTL_SENSOR_SET_DELAY_ACCEL:
			if(copy_from_user((void *)&sensor_duration, (void __user *) arg, sizeof(short))!=0){
				printk("copy from error in %s.\n",__func__);
			}

			break;

		case IOCTL_SENSOR_GET_DELAY_ACCEL:
			if(copy_to_user((void __user *) arg, (const void *)&sensor_duration, sizeof(short))!=0){
				printk("copy to error in %s.\n",__func__);
			} 

			break;

		case IOCTL_SENSOR_GET_STATE_ACCEL:
			if(copy_to_user((void __user *) arg, (const void *)&sensor_state_flag, sizeof(short))!=0){
				printk("copy to error in %s.\n",__func__);
			}

			break;

		case IOCTL_SENSOR_SET_STATE_ACCEL:
			if(copy_from_user((void *)&sensor_state_flag, (void __user *) arg, sizeof(short))!=0){
				printk("copy from error in %s.\n",__func__);
			}     

			break;*/
		case IOCTL_SENSOR_GET_NAME:
			if(copy_to_user((void __user *) arg,(const void *)MC3XXX_DISPLAY_NAME, sizeof(MC3XXX_DISPLAY_NAME))!=0){
				printk("copy to error in %s.\n",__func__);
			}     			
			break;		

		case IOCTL_SENSOR_GET_VENDOR:
			if(copy_to_user((void __user *) arg,(const void *)MC3XXX_DIPLAY_VENDOR, sizeof(MC3XXX_DIPLAY_VENDOR))!=0){
				printk("copy to error in %s.\n",__func__);
			}     			
			break;

		case IOCTL_SENSOR_GET_CONVERT_PARA:
			convert_para = MC3XXX_CONVERT_PARAMETER;
			if(copy_to_user((void __user *) arg,(const void *)&convert_para,sizeof(float))!=0){
				printk("copy to error in %s.\n",__func__);
			}     			
			break;

#ifdef DOT_CALI		
	case GSENSOR_IOCTL_READ_SENSORDATA:	
	case GSENSOR_IOCTL_READ_RAW_DATA:
	//case GSENSOR_MCUBE_IOCTL_READ_RBM_DATA:
                GSE_LOG("fwq GSENSOR_IOCTL_READ_RAW_DATA\n");
                
		//mutex_lock(&data->lock);
		MC3XXX_ReadRawData(client, strbuf);
		//mutex_unlock(&data->lock);

                if (copy_to_user((void __user *) arg, &strbuf, strlen(strbuf)+1)) 
		{
                    printk("failed to copy sense data to user space.");
                    return -EFAULT;
                }
                break;
            
	case GSENSOR_MCUBE_IOCTL_SET_CALI:
                GSE_LOG("fwq GSENSOR_MCUBE_IOCTL_SET_CALI!!\n");
                data1 = (void __user *)arg;
                
                if(data1 == NULL)
                {
                    ret = -EINVAL;
                    break;	  
                }
                if(copy_from_user(&sensor_data, data1, sizeof(sensor_data)))
                {
                    ret = -EFAULT;
                    break;	  
                }
                else
                {
		l_sensorconfig.offset[MC3XXX_AXIS_X] = sensor_data.x;
		l_sensorconfig.offset[MC3XXX_AXIS_Y] = sensor_data.y;
		l_sensorconfig.offset[MC3XXX_AXIS_Z] = sensor_data.z;	
		update_var();
		GSE_LOG("GSENSOR_MCUBE_IOCTL_SET_CALI %d  %d  %d  %d  %d  %d!!\n", l_sensorconfig.offset[MC3XXX_AXIS_X], l_sensorconfig.offset[MC3XXX_AXIS_Y],l_sensorconfig.offset[MC3XXX_AXIS_Z] ,sensor_data.x, sensor_data.y ,sensor_data.z);

		//mutex_lock(&data->lock);
		ret = MC3XXX_WriteCalibration(client, l_sensorconfig.offset);			 
		//mutex_unlock(&data->lock);
                }
                break;
            
	case GSENSOR_IOCTL_CLR_CALI:
		GSE_LOG("fwq GSENSOR_IOCTL_CLR_CALI!!\n");
		//mutex_lock(&data->lock);
		l_sensorconfig.offset[0] = 0;
		l_sensorconfig.offset[1] = 0;
		l_sensorconfig.offset[2] = 0;	

		update_var();
		ret = MC3XXX_ResetCalibration(client);
		//mutex_unlock(&data->lock);
                break;
            
	case GSENSOR_IOCTL_GET_CALI:
                GSE_LOG("fwq mc3xxx GSENSOR_IOCTL_GET_CALI\n");
                
                data1 = (unsigned char*)arg;
                
                if(data1 == NULL)
                {
                    ret = -EINVAL;
                    break;	  
                }
                
                if((ret = MC3XXX_ReadCalibration(client,l_sensorconfig.offset)))
                {
                    GSE_LOG("fwq mc3xxx MC3XXX_ReadCalibration error!!!!\n");
                    break;
                }
                
                sensor_data.x = l_sensorconfig.offset[MC3XXX_AXIS_X];
                sensor_data.y = l_sensorconfig.offset[MC3XXX_AXIS_Y];
                sensor_data.z = l_sensorconfig.offset[MC3XXX_AXIS_Z];
                
                if(copy_to_user(data1, &sensor_data, sizeof(sensor_data)))
                {
                    ret = -EFAULT;
                    break;
                }		
                break;	
            
	case GSENSOR_IOCTL_SET_CALI_MODE:
                GSE_LOG("fwq mc3xxx GSENSOR_IOCTL_SET_CALI_MODE\n");
                break;
            
	case GSENSOR_MCUBE_IOCTL_READ_RBM_DATA:
		GSE_LOG("fwq GSENSOR_MCUBE_IOCTL_READ_RBM_DATA\n");
		data1 = (void __user *) arg;
		if(data1 == NULL)
		{
			ret = -EINVAL;
			break;	  
		}
		MC3XXX_ReadRBMData(client,(char *)&strbuf);
		if(copy_to_user(data1, &strbuf, strlen(strbuf)+1))
		{
			ret = -EFAULT;
			break;	  
		}
		break;
	case GSENSOR_MCUBE_IOCTL_SET_RBM_MODE:
                GSE_LOG("fwq GSENSOR_MCUBE_IOCTL_SET_RBM_MODE\n");
		//MCUBE_BACKUP_FILE
		if(READ_FROM_BACKUP==true)
		{
			
			//mcube_copy_file(CALIB_PATH);
			
			READ_FROM_BACKUP = false;
		}
		//MCUBE_BACKUP_FILE
		//mutex_lock(&data->lock);
		MC3XXX_rbm(client, 1);
		//mutex_unlock(&data->lock);
                break;
            
	case GSENSOR_MCUBE_IOCTL_CLEAR_RBM_MODE:
		GSE_LOG("fwq GSENSOR_MCUBE_IOCTL_CLEAR_RBM_MODE\n");
		//mutex_lock(&data->lock);
		MC3XXX_rbm(client, 0);
		//mutex_unlock(&data->lock);
                break;
            
	case GSENSOR_MCUBE_IOCTL_REGISTER_MAP:
                GSE_LOG("fwq GSENSOR_MCUBE_IOCTL_REGISTER_MAP\n");
                MC3XXX_ReadRegMap(client, NULL);
                break;
            
	 case GSENSOR_MCUBE_IOCTL_READ_PRODUCT_ID:
                GSE_LOG("fwq GSENSOR_MCUBE_IOCTL_READ_PRODUCT_ID\n");
                data1 = (void __user *) arg;
                if(data1 == NULL)
                {
                    ret = -EINVAL;
                    break;	  
                }

		if (MC3XXX_RETCODE_SUCCESS != (prod = MC3XX0_ValidateSensorIC(s_bPCODE)))
			GSE_LOG("Not mCube accelerometers!\n");
				 				
                if(copy_to_user(data1, &prod, sizeof(prod)))
                {
                    GSE_LOG("%s: read pcode fail to copy!\n", __func__);
                    return -EFAULT;
                }
                break;
#endif
		
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}


static int mc3xxx_open(struct inode *inode, struct file *filp)
{
	return nonseekable_open(inode, filp);
}

static int mc3xxx_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static int wmt_mc3xxx_open(struct inode *inode, struct file *filp)
{
	return 0;
}

static int wmt_mc3xxx_release(struct inode *inode, struct file *filp)
{
	return 0;
}


//=============================================================================
static struct file_operations sensor_fops =
{
	.owner          = THIS_MODULE,
	.open       	= mc3xxx_open,
	.release    	= mc3xxx_release,
	.unlocked_ioctl = mc3xxx_ioctl,
};

static struct file_operations wmt_sensor_fops =
{
	.owner          = THIS_MODULE,
	.open       	= wmt_mc3xxx_open,
	.release    	= wmt_mc3xxx_release,
	.unlocked_ioctl = wmt_mc3xxx_ioctl,
};



static int sensor_writeproc( struct file   *file,
                           const char    *buffer,
                           unsigned long count,
                           void          *data )
{

	//int inputval = -1;
	int enable, sample = -1;
	char tembuf[8];
	//unsigned int amsr = 0;
	int test = 0;

	mutex_lock(&l_sensorconfig.lock);
	memset(tembuf, 0, sizeof(tembuf));
	// get sensor level and set sensor level
	if (sscanf(buffer, "isdbg=%d\n", &l_sensorconfig.isdbg))
	{
		// only set the dbg flag
	} else if (sscanf(buffer, "samp=%d\n", &sample))
	{
		if (sample > 0)
		{
			if (sample != l_sensorconfig.sensor_samp)
			{
				// should do sth
			}			
			//printk(KERN_ALERT "sensor samp=%d(amsr:%d) has been set.\n", sample, amsr);
		} else {
			klog("Wrong sample argumnet of sensor.\n");
		}
	} else if (sscanf(buffer, "enable=%d\n", &enable))
	{
		if ((enable < 0) || (enable > 1))
		{
			dbg("The argument to enable/disable g-sensor should be 0 or 1  !!!\n");
		} else if (enable != l_sensorconfig.sensor_enable)
		{
			//mma_enable_disable(enable);
			l_sensorconfig.sensor_enable = enable;
		}
	} else 	if (sscanf(buffer, "sensor_test=%d\n", &test))
	{ // for test begin
		l_sensorconfig.test_pass = 0;		
	} else if (sscanf(buffer, "sensor_testend=%d\n", &test))
	{	// Don nothing only to be compatible the before testing program		
	}
	mutex_unlock(&l_sensorconfig.lock);
	return count;
}

static int sensor_readproc(char *page, char **start, off_t off,
			  int count, int *eof, void *data)
{
	int len = 0;

	len = sprintf(page, 
			"test_pass=%d\nisdbg=%d\nrate=%d\nenable=%d\n",
				l_sensorconfig.test_pass,
				l_sensorconfig.isdbg,
				l_sensorconfig.sensor_samp,
				l_sensorconfig.sensor_enable
				);
	return len;
}


//#ifdef CONFIG_HAS_EARLYSUSPEND
static void mc3xxx_early_suspend(struct platform_device *pdev, pm_message_t state)
{
	/*struct mc3xxx_data *data = NULL;

	data = container_of(handler, struct mc3xxx_data, early_suspend);

	hrtimer_cancel(&data->timer);*/

	cancel_delayed_work_sync(&l_sensorconfig.work);
	mc3xxx_set_mode(l_sensorconfig.client,MC3XXX_STANDBY);
}

//=============================================================================
static void mc3xxx_early_resume(struct platform_device *pdev)
{
	struct mc3xxx_data *data = &l_sensorconfig;

	wake_mc3xxx_flg =1;
	//data = container_of(handler, struct mc3xxx_data, early_suspend);
	
	mc3xxx_set_mode(data->client,MC3XXX_WAKE);

	queue_delayed_work(data->mc3xxx_wq, &data->work, msecs_to_jiffies(sample_rate_2_memsec(data->sensor_samp)));
	//hrtimer_start(&data->timer, ktime_set(1, 0), HRTIMER_MODE_REL);
}
//#endif

//=============================================================================
static struct miscdevice mc3xxx_device =
{
	.minor = MISC_DYNAMIC_MINOR,
	.name  = SENSOR_NAME,
	.fops  = &sensor_fops,
};

static struct miscdevice mc3xxx_wmt_device =
{
	.minor = MISC_DYNAMIC_MINOR,
	.name  = "sensor_ctrl",
	.fops  = &wmt_sensor_fops,
};



/*****************************************
 *** MC3XX0_ValidateSensorIC
 *****************************************/
static int MC3XX0_ValidateSensorIC(unsigned char bPCode)
{
    unsigned char    _baOurSensorList[] = { MC3XXX_PCODE_3210 , MC3XXX_PCODE_3230 ,
                                            MC3XXX_PCODE_3250 ,
                                            MC3XXX_PCODE_3410 , MC3XXX_PCODE_3430 ,
                                            MC3XXX_PCODE_3410N, MC3XXX_PCODE_3430N,
                                            MC3XXX_PCODE_3510B, MC3XXX_PCODE_3530B,
                                            MC3XXX_PCODE_3510C, MC3XXX_PCODE_3530C
                                          };

    int    _nSensorCount = (sizeof(_baOurSensorList) / sizeof(_baOurSensorList[0]));
    int    _nCheckIndex  = 0;

    GSE_LOG("[%s] code to be verified: 0x%X, _nSensorCount: %d\n", __FUNCTION__, bPCode, _nSensorCount);

    for (_nCheckIndex = 0; _nCheckIndex < _nSensorCount; _nCheckIndex++)
    {
        if (_baOurSensorList[_nCheckIndex] == bPCode)
            return (MC3XXX_RETCODE_SUCCESS);            
    }
    
    if (MC3XXX_PCODE_3530C == (bPCode | 0x0E))
            return (MC3XXX_RETCODE_SUCCESS);

    return (MC3XXX_RETCODE_ERROR_IDENTIFICATION);
}

/*****************************************
 *** _mc3xxx_i2c_auto_probe
 *****************************************/
static int _mc3xxx_i2c_auto_probe(struct i2c_client *client)
{
    unsigned char    _baDataBuf[2] = {0};
    int              _nProbeAddrCount = (sizeof(mc3xxx_i2c_auto_probe_addr) / sizeof(mc3xxx_i2c_auto_probe_addr[0]));
    int              _nCount = 0;
    int              _nCheckCount     = 0;

    //GSE_FUN();

    s_bPCODE = 0x00;

    for (_nCount = 0; _nCount < _nProbeAddrCount; _nCount++)
    {
        _nCheckCount = 0;
        client->addr = mc3xxx_i2c_auto_probe_addr[_nCount];

        //GSE_LOG("[%s] probing addr: 0x%X\n", __FUNCTION__, client->addr);

_I2C_AUTO_PROBE_RECHECK_:
        _baDataBuf[0] = MC3XXX_REG_PRODUCT_CODE;
        if (0 > i2c_master_send(client, &(_baDataBuf[0]), 1))
        {
            //GSE_ERR("ERR: addr: 0x%X fail to communicate-2!\n", client->addr);
            continue;
        }
    
        if (0 > i2c_master_recv(client, &(_baDataBuf[0]), 1))
        {
            //GSE_ERR("ERR: addr: 0x%X fail to communicate-3!\n", client->addr);
            continue;
        }
    
        _nCheckCount++;

        //GSE_LOG("[%s][%d] addr: 0x%X ok to read REG(0x3B): 0x%X\n", __FUNCTION__, _nCheckCount, client->addr, _baDataBuf[0]);

        if (0x00 == _baDataBuf[0])
        {
            if (1 == _nCheckCount)
            {
                MC3XXX_Reset(client);
                goto _I2C_AUTO_PROBE_RECHECK_;
            }
        }

        if (MC3XXX_RETCODE_SUCCESS == MC3XX0_ValidateSensorIC(_baDataBuf[0]))
        {
            //GSE_LOG("[%s] addr: 0x%X confirmed ok to use.\n", __FUNCTION__, client->addr);

            s_bPCODE = _baDataBuf[0];

            return (MC3XXX_RETCODE_SUCCESS);
        }
    }

    return (MC3XXX_RETCODE_ERROR_I2C);
}


//=============================================================================
//static int mc3xxx_probe(struct i2c_client *client,
//		const struct i2c_device_id *id)
static int mc3xxx_probe(struct platform_device *pdev)
{
	int ret = 0;
	//int product_code = 0;
	struct mc3xxx_data *data = &l_sensorconfig;
	struct i2c_client *client = l_sensorconfig.client;
	
    #ifdef DOT_CALI
       load_cali_flg = 30;
    #endif
/*	
	if (MC3XXX_RETCODE_SUCCESS != _mc3xxx_i2c_auto_probe(client))
	{
		GSE_ERR("ERR: fail to probe mCube sensor!\n");
		goto err_check_functionality_failed;
	}

	data = kzalloc(sizeof(struct mc3xxx_data), GFP_KERNEL);
	if(data == NULL)
	{
		ret = -ENOMEM;
		goto err_alloc_data_failed;
	}
*/
	data->sensor_proc = create_proc_entry(GSENSOR_PROC_NAME, 0666, NULL/*&proc_root*/);
	if (data->sensor_proc != NULL)
	{
		data->sensor_proc->write_proc = sensor_writeproc;
		data->sensor_proc->read_proc = sensor_readproc;
	}
	
	data->mc3xxx_wq = create_singlethread_workqueue("mc3xxx_wq");
	if (!data->mc3xxx_wq )
	{
		ret = -ENOMEM;
		goto err_create_workqueue_failed;
	}
	INIT_DELAYED_WORK(&data->work, mc3xxx_work_func);
	mutex_init(&data->lock);

	//sensor_duration = SENSOR_DURATION_DEFAULT;
	//sensor_state_flag = 1;

	
	data->client = client;
	dev.client=client;

	i2c_set_clientdata(client, data);	

	data->input_dev = input_allocate_device();
	if (!data->input_dev) {
		ret = -ENOMEM;
		goto exit_input_dev_alloc_failed;
	}

    #ifdef DOT_CALI
	MC3XXX_Reset(client);
    #endif

	ret = mc3xxx_chip_init(client);
	if (ret < 0) {
		goto err_chip_init_failed;
	}

	set_bit(EV_ABS, data->input_dev->evbit);
	data->map[0] = G_0;
	data->map[1] = G_1;
	data->map[2] = G_2;
	data->inv[0] = G_0_REVERSE;
	data->inv[1] = G_1_REVERSE;
	data->inv[2] = G_2_REVERSE;

	input_set_abs_params(data->input_dev, ABS_X, -32*8, 32*8, INPUT_FUZZ, INPUT_FLAT);
	input_set_abs_params(data->input_dev, ABS_Y, -32*8, 32*8, INPUT_FUZZ, INPUT_FLAT);
	input_set_abs_params(data->input_dev, ABS_Z, -32*8, 32*8, INPUT_FUZZ, INPUT_FLAT);

	data->input_dev->name = "g-sensor";

	ret = input_register_device(data->input_dev);
	if (ret) {
		goto exit_input_register_device_failed;
	}
	
    	mc3xxx_device.parent = &client->dev;
 
	ret = misc_register(&mc3xxx_device);
	if (ret) {
		goto exit_misc_device_register_failed;
	}

	ret = misc_register(&mc3xxx_wmt_device);
	if (ret) {
		goto exit_misc_device_register_failed;
	}
	
	ret = sysfs_create_group(&data->input_dev->dev.kobj, &mc3xxx_group);
/*
	if (!data->use_irq){
		hrtimer_init(&data->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		data->timer.function = mc3xxx_timer_func;
		//hrtimer_start(&data->timer, ktime_set(1, 0), HRTIMER_MODE_REL);
	}
*/

#ifdef CONFIG_HAS_EARLYSUSPEND
	data->early_suspend.suspend = mc3xxx_early_suspend;
	data->early_suspend.resume = mc3xxx_early_resume;
	register_early_suspend(&data->early_suspend);
#endif
	data->enabled = 1;
	queue_delayed_work(data->mc3xxx_wq, &data->work, msecs_to_jiffies(sample_rate_2_memsec(data->sensor_samp)));
	//strcpy(mc3xxx_on_off_str,"gsensor_int2");
	//gpio_set_one_pin_io_status(mc3xxx_pin_hd,0,mc3xxx_on_off_str);
	
	printk("mc3xxx probe ok \n");

	return 0;
exit_misc_device_register_failed:
exit_input_register_device_failed:
	input_free_device(data->input_dev);
err_chip_init_failed:
exit_input_dev_alloc_failed:
	destroy_workqueue(data->mc3xxx_wq);	
err_create_workqueue_failed:
	kfree(data);	
//err_alloc_data_failed:
//err_check_functionality_failed:
	printk("mc3xxx probe failed \n");
	return ret;

}

//static int mc3xxx_remove(struct i2c_client *client)
static int mc3xxx_remove(struct platform_device *pdev)
{
	/*struct mc3xxx_data *data = i2c_get_clientdata(client);

	hrtimer_cancel(&data->timer);
	input_unregister_device(data->input_dev);	
//	gpio_release(mc3xxx_pin_hd, 2);
	misc_deregister(&mc3xxx_device);
	sysfs_remove_group(&data->input_dev->dev.kobj, &mc3xxx_group);
	kfree(data);
	return 0;*/

	if (NULL != l_sensorconfig.mc3xxx_wq)
	{
		cancel_delayed_work_sync(&l_sensorconfig.work);
		flush_workqueue(l_sensorconfig.mc3xxx_wq);
		destroy_workqueue(l_sensorconfig.mc3xxx_wq);
		l_sensorconfig.mc3xxx_wq = NULL;
	}
	if (l_sensorconfig.sensor_proc != NULL)
	{
		remove_proc_entry(GSENSOR_PROC_NAME, NULL);
		l_sensorconfig.sensor_proc = NULL;
	}
	misc_deregister(&mc3xxx_device);
	misc_deregister(&mc3xxx_wmt_device);
	sysfs_remove_group(&l_sensorconfig.input_dev->dev.kobj, &mc3xxx_group);
	input_unregister_device(l_sensorconfig.input_dev);	
	return 0;
}

//=============================================================================
/*
static void mc3xxx_shutdown(struct i2c_client *client)
{
	struct mc3xxx_data *data = i2c_get_clientdata(client);

	if(data->enabled)
		mc3xxx_enable(data, 0);
}
*/

//=============================================================================

static const struct i2c_device_id mc3xxx_id[] =
{
	{ SENSOR_NAME, 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, mc3xxx_id);
/*
static struct i2c_driver mc3xxx_driver =
{
	.class = I2C_CLASS_HWMON,

	.driver = {
		.owner	= THIS_MODULE,
		.name	= SENSOR_NAME,
	},
	.id_table	= mc3xxx_id,
	.probe		= mc3xxx_probe,
	.remove		= mc3xxx_remove,
	//.shutdown	= mc3xxx_shutdown,
};
*/
extern int wmt_getsyspara(char *varname, unsigned char *varval, int *varlen);
static int get_axisset(void)
{
	char varbuf[64];
	int n;
	int varlen;
	//int tmpoff[3] = {0};
	memset(varbuf, 0, sizeof(varbuf));
	varlen = sizeof(varbuf);

	pCvt = (struct mc3xx0_hwmsen_convert *)kzalloc(sizeof(struct mc3xx0_hwmsen_convert), GFP_KERNEL);
	
	if (wmt_getsyspara("wmt.io.mc3230.virtualz", varbuf, &varlen)) {
		errlog("Can't get gsensor config in u-boot!!!!\n");
		//return -1;
	} else {
		sscanf(varbuf, "%d", &g_virtual_z);
		
	}
	printk("%s g_virtual_z %d\n", __FUNCTION__, g_virtual_z);
	memset(varbuf, 0, sizeof(varbuf));
	if (wmt_getsyspara("wmt.io.mc3230sensor", varbuf, &varlen)) {
		errlog("Can't get gsensor config in u-boot!!!!\n");
		return -1; //open it for no env just,not insmod such module 2014-6-30
	} else {
		n = sscanf(varbuf, "%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d",
				&l_sensorconfig.op,
				&l_sensorconfig.int_gpio,
				&l_sensorconfig.samp,
				&(pCvt->map[MC3XXX_AXIS_X]),
				&(pCvt->sign[MC3XXX_AXIS_X]),
				&(pCvt->map[MC3XXX_AXIS_Y]),
				&(pCvt->sign[MC3XXX_AXIS_Y]),
				&(pCvt->map[MC3XXX_AXIS_Z]),
				&(pCvt->sign[MC3XXX_AXIS_Z]),
				&(l_sensorconfig.offset[0]),
				&(l_sensorconfig.offset[1]),
				&(l_sensorconfig.offset[2])
			);
		if (n != 12) {
			printk(KERN_ERR "gsensor format is error in u-boot!!!\n");
			return -1;
		}
		l_sensorconfig.sensor_samp = l_sensorconfig.samp;
		
		
		dbg("get the sensor config: %d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d\n",
			l_sensorconfig.op,
			l_sensorconfig.int_gpio,
			l_sensorconfig.samp,
			pCvt->map[MC3XXX_AXIS_X],
			pCvt->sign[MC3XXX_AXIS_X],
			pCvt->map[MC3XXX_AXIS_Y],
			pCvt->sign[MC3XXX_AXIS_Y],
			pCvt->map[MC3XXX_AXIS_Z],
			pCvt->sign[MC3XXX_AXIS_Z],
			l_sensorconfig.offset[0],
			l_sensorconfig.offset[1],
			l_sensorconfig.offset[2]
		);
	}
	return 0;
}

static void mc3xxx_platform_release(struct device *device)
{
    return;
}


static struct platform_device mc3xxx_pdevice = {
    .name           = SENSOR_NAME,
    .id             = 0,
    .dev            = {
    	.release = mc3xxx_platform_release,
    },
};

static struct platform_driver mc3xxx_pdriver = {
	.probe = mc3xxx_probe,
	.remove = mc3xxx_remove,
	.suspend	= mc3xxx_early_suspend,
	.resume		= mc3xxx_early_resume,
	//.shutdown	= mc3xxx_shutdown,
	.driver = {
		   .name = SENSOR_NAME,
		   },
};


static int __init mc3xxx_init(void)
{
	int ret = -1;
	struct i2c_client *this_client;
	printk("mc3xxx: init\n");

	//ret = i2c_add_driver(&mc3xxx_driver);

	// parse g-sensor u-boot arg
	ret = get_axisset();
	if (ret < 0)
	{
		printk("<<<<<%s user choose to no sensor chip!\n", __func__);
		return ret;
	}

	if (!(this_client = sensor_i2c_register_device(0, mc3xxx_i2c_auto_probe_addr[0], SENSOR_NAME)))
	{
		printk(KERN_ERR"Can't register gsensor i2c device!\n");
		return -1;
	}

	if (MC3XXX_RETCODE_SUCCESS != _mc3xxx_i2c_auto_probe(this_client))
	{
		GSE_ERR("ERR: fail to auto_probe mCube sensor!\n");
		sensor_i2c_unregister_device(this_client);
		return -1;
	}

	
	l_sensorconfig.client = this_client;
		
	l_dev_class = class_create(THIS_MODULE, SENSOR_NAME);
	if (IS_ERR(l_dev_class)){
		ret = PTR_ERR(l_dev_class);
		printk(KERN_ERR "Can't class_create gsensor device !!\n");
		return ret;
	}
	if((ret = platform_device_register(&mc3xxx_pdevice)))
	{
		klog("Can't register mc3xxx platform devcie!!!\n");
		return ret;
	}
	if ((ret = platform_driver_register(&mc3xxx_pdriver)) != 0)
	{
		errlog("Can't register mc3xxx platform driver!!!\n");
		return ret;
	}
	
	
	return ret;
}

static void __exit mc3xxx_exit(void)
{
	//i2c_del_driver(&mc3xxx_driver);
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&l_sensorconfig.earlysuspend);
#endif
    platform_driver_unregister(&mc3xxx_pdriver);
    platform_device_unregister(&mc3xxx_pdevice);
	sensor_i2c_unregister_device(l_sensorconfig.client);
	class_destroy(l_dev_class);
}

//=============================================================================
module_init(mc3xxx_init);
module_exit(mc3xxx_exit);

MODULE_DESCRIPTION("mc3xxx accelerometer driver");
MODULE_AUTHOR("mCube-inc");
MODULE_LICENSE("GPL");
MODULE_VERSION(SENSOR_DRIVER_VERSION);


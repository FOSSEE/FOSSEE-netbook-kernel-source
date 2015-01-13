#ifndef __VT1603_TS_H__
#define __VT1603_TS_H__
#include <linux/mfd/vt1603/core.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#define DEV_NAME  				"wmtts"
#define VT1603_DRIVER_NAME  	"vt1603-touch"

#undef abs
#define abs(x) (((x)>0)?(x):(-(x)))

#define FALSE 						0
#define TRUE   						1
#define VT1603_TS_NR_DEVS     		1
#define VT1603_TS_DEV_MAJOR   		160
#define TS_DEBOUNCE    				50
#define VT1603_FILTER_HEAD_COUNT 	2
#define VT1603_JITTER_THRESHOLD 	1200//单点前后2点之间的x/y 变化最大容许值

#define None_TOUCH  				0x00
#define Single_TOUCH 				0x01
#define Multi_TOUCH 				0x02

#define ADC_DATA(low, high)  		((((high) & 0x0F) << 8) + (low))

/* touch panel type config   */
#define PANEL_TYPE_4WIRED     	0x10
#define PANEL_TYPE_5WIRED     	0x11

/* enable calibration or not */
#define CALIBRATION_ENABLE    	0x01
#define CALIBRATION_DISABLE   	0x00

/* VT1603 working mode       */
#define VT1603_TS_MODE      	BIT1
#define VT1603_TEMP_MODE    	BIT2
#define VT1603_BAT_MODE     	BIT3

/* VT1603 touch panel state  */
#define TS_PENDOWN_STATE     	0x00
#define TS_PENUP_STATE       	0x01

struct vt1603_ts_pos {
    int x;
    int y;
};

#define VT1603_FIFO_LEN 		3
struct vt1603_fifo{
	int head;
	int full;
	int buf[VT1603_FIFO_LEN];
};

#define I2C_BUS					0x00
#define SPI_BUS					0x01

#define VT1603_SPI_FIX_CS   	0x00
#define VT1603_SPI_FAKE_CS  	0x03
#define VT1603_SPI_BUS_0    	0x00
#define VT1603_SPI_BUS_1    	0x01
#define VT1603_MAX_SPI_CLK    	(20*1000*1000)
#define SPI_DEFAULT_CLK       	(4*1000*1000)  
#define IDLE_DATA_NUM         	5

#define  VT1603_I2C_FIX_ADDR  	0x1A
#define  VT1603_I2C_FAKE_ADDR 	0xFF
#define  VT1603_TS_I2C_WCMD   	0x00
#define  VT1603_TS_I2C_RCMD   	0x01
#define  VT1603_TS_I2C_RWCMD  	0x02
#define VT1603_I2C_BUS_0     	0x00
#define VT1603_I2C_BUS_1     	0x01

///////////////////////////////
//#define TOUCH_KEY
#define KEY_DETLA 				300
#define TOUCH_KEY_NUM 			4
#define TOUCH_KEY_LED_GPIO 	    4

#define HIGH	1
#define LOW		0

struct tsc_key{
	int pos;
	int idx;
};

struct tsc_key_st{
	int key_num;
	int low;
	int upper;
    int delta;
	struct tsc_key key[TOUCH_KEY_NUM];
};

enum key_idx{
	_SEARCH,
	_BACK,
	_HOME,
	_MENU,
	_MAX_NUM,
};
/////////////////////////

enum gpio_irq_type {
      HIGH_ACTIVE         		= 0,
      LOW_ACTIVE          		= 1,
      RISING_EDGE_ACTIVE  	    = 3,
      FALLING_EDGE_ACTIVE 	    = 4,
      UNKOWN_TYPE         		= 0xFF
};

/*
 * vt1603_ts_platform_data - vt1603 configuration data
 * @panel_type:           touch panel type: 4-wired or 5-wired
 * @cal_en:                 enable calibration circuit or not
 * @cal_sel:                 calibratin capacitor control bits
 * @shfit:                    conversion data shfit
 * @sclk_div:               initial value of sclk dividor if mclk = 12.288MHZ 0x04 = 200ksps 0x08 = 100ksps
 * @soc_gpio_irq:         soc gpio interrupts, connect with vt1603 gpio1
 */
struct vt1603_ts_platform_data {
    u8 panel_type;
    u8 cal_en;
    u8 cal_sel:2;
    u8 shift;
    u8 sclk_div;
    int soc_gpio_irq;
    int gpio_num;
    enum gpio_irq_type irq_type;
};

struct vt1609_dual_st{
	int vxy;

	int scale_x;
	int scale_y;

	int F1_CNT;
	int F2_CNT;
	int F2T1_CNT;
	int SAMPLE_CNT;
	
	int THR_MIN_DX;
	int THR_MAX_DX;

    int exch;
};

struct vt1603_ts_drvdata {
    struct vt1603 *tdev;
    //spinlock_t spinlock;
    struct mutex ts_mutex;
    struct input_dev *input;
    struct work_struct work;
    struct delayed_work read_work;
    struct delayed_work dual_work;
    struct workqueue_struct *workqueue;
    struct vt1603_ts_platform_data *pdata;

    int earlysus;
#ifdef CONFIG_HAS_EARLYSUSPEND
    struct early_suspend earlysuspend;
#endif
    char dev_id[32];

    struct cdev cdev;
    int major;
    int minor;
	
    int gpio_irq;
    
    int intgpio;

    u8 pen_state;
    int ts_stamp;
    int hcnt;

    int resl_x;
    int resl_y;
	int lcd_exchg;
	
    int raw_x;
    int raw_y;

    int dual_enable;
    struct vt1609_dual_st dual_dev;

    int ledgpio;
#ifdef TOUCH_KEY
    int key_idx;
    int key_pressed;
    int touch_key_used;
    struct timer_list led_timer;
    struct tsc_key_st tsc_key;
#endif

};

/* VT1603 Register address */
#define VT1603_BTHD_REG       	0x78
#define VT1603_BCLK_REG       	0x88
#define VT1603_BAEN_REG       	0x04

#define VT1603_PWC_REG        	0xC0
#define VT1603_CR_REG         	0xC1
#define VT1603_CCCR_REG       	0xC2
#define VT1603_CDPR_REG       	0xC3
#define VT1603_TSPC_REG       	0xC4
#define VT1603_AMCR_REG       	0xC7
#define VT1603_INTCR_REG      	0xC8
#define VT1603_INTEN_REG      	0xC9
#define VT1603_INTS_REG       	0xCA
#define VT1603_DCR_REG        	0xCB

#define VT1603_TODCL_REG      	0xCC
#define VT1603_TODCH_REG      	0xCD

#define VT1603_DATL_REG       	0xCE
#define VT1603_DATH_REG       	0xCF

#define VT1603_XPL_REG        	0xD0
#define VT1603_XPH_REG        	0xD1
#define VT1603_YPL_REG        	0xD2
#define VT1603_YPH_REG        	0xD3

#define VT1603_BATL_REG       	0xD4
#define VT1603_BATH_REG       	0xD5

#define VT1603_TEMPL_REG      	0xD6
#define VT1603_TEMPH_REG      	0xD7

#define VT1603_ERR8_REG       	0xD8
#define VT1603_ERR7_REG       	0xD9
#define VT1603_ERR6_REG       	0xDA
#define VT1603_ERR5_REG       	0xDB
#define VT1603_ERR4_REG       	0xDC
#define VT1603_ERR3_REG       	0xDD
#define VT1603_ERR2_REG       	0xDE
#define VT1603_ERR1_REG       	0xDF

#define VT1603_DBG8_REG       	0xE0
#define VT1603_DBG7_REG       	0xE1
#define VT1603_DBG6_REG       	0xE2
#define VT1603_DBG5_REG       	0xE3
#define VT1603_DBG4_REG       	0xE4
#define VT1603_DBG3_REG       	0xE5
#define VT1603_DBG2_REG       	0xE6
#define VT1603_DBG1_REG       	0xE7

/* for VT1603 GPIO1 interrupt setting */
#define VT1603_IMASK_REG27    	27
#define VT1603_IMASK_REG28    	28
#define VT1603_IMASK_REG29    	29
#define VT1603_IPOL_REG33     	33
#define VT1603_ISEL_REG36     	36

struct vt1603_ts_cal_info {
    int   a1;
    int   b1;
    int   c1;
    int   a2;
    int   b2;
    int   c2;
    int   delta;
};

/* VT1603 TS and SAR-ADC IOCTL   */
#define VT1603_TS_IOC_MAGIC  	't'

/* for touch screen calibration  */
#define VT1603_TS_IOC_CAL_START      	_IO(VT1603_TS_IOC_MAGIC,  1)
#define VT1603_TS_IOC_CAL_DONE       	_IOW(VT1603_TS_IOC_MAGIC, 2, int *)
#define VT1603_TS_IOC_CAL_RAWDATA    	_IOR(VT1603_TS_IOC_MAGIC, 3, int *)
#define VT1603_TS_IOC_CAL_QUIT       	_IOW(VT1603_TS_IOC_MAGIC, 4, int *)

extern int wmt_setsyspara(char *varname, unsigned char *varval);
extern int wmt_getsyspara(char *varname, unsigned char *varval, int *varlenex);

int vt1603_clr_ts_irq(struct vt1603_ts_drvdata *ts_drv, u8 mask);
int vt1603_set_reg8(struct vt1603_ts_drvdata *ts_drv, u8 reg, u8 val);
u8 vt1603_get_reg8(struct vt1603_ts_drvdata *ts_drv, u8 reg);
void vt1603_setbits(struct vt1603_ts_drvdata *ts_drv, u8 reg, u8 mask);
void vt1603_clrbits(struct vt1603_ts_drvdata *ts_drv, u8 reg, u8 mask);

void vt1603_ts_report_pos(struct vt1603_ts_drvdata *ts_drv, struct vt1603_ts_pos *pos);
int vt1603_ts_pos_calibration(struct vt1603_ts_drvdata *ts_drv,struct vt1603_ts_pos *to_cal);

#ifdef TOUCH_KEY
void vt1603_ts_report_key(struct vt1603_ts_drvdata *ts_drv);
int vt1603_ts_get_key(struct vt1603_ts_drvdata *ts_drv,struct vt1603_ts_pos pos);
int set_key_led_gpio(struct vt1603_ts_drvdata *ts_drv,int val);
#endif

int vt1603_dual_init(struct vt1603_ts_drvdata *ts_drv);
void vt1603_dual_exit(struct vt1603_ts_drvdata *ts_drv);
void vt1603_ts_dual_support(struct work_struct* work);

#endif  /* __VT1603_TS_H__ */

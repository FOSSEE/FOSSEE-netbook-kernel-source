
#ifndef WMT_TSH_201010191758
#define WMT_TSH_201010191758

#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/suspend.h>

//#define DEBUG_WMT_TS
#ifdef DEBUG_WMT_TS
#undef dbg
#define dbg(fmt, args...) printk(KERN_ALERT "[%s]: " fmt, __FUNCTION__ , ## args)

//#define dbg(fmt, args...) if (wmt_ts_isrundbg()) printk(KERN_ALERT "[%s]: " fmt, __FUNCTION__, ## args)

#else
#define dbg(fmt, args...) 
#endif

#undef errlog
#undef klog
#define errlog(fmt, args...) printk("[%s]: " fmt, __FUNCTION__, ## args)
#define klog(fmt, args...) printk("[%s]: " fmt, __FUNCTION__, ## args)

#define DONOTHING 0xff

#define WMT_TS_I2C_NAME "lw86x0-ts"
//////////////////////////////data type///////////////////////////
typedef struct {
	short pressure;
	short x;
	short y;
	//short millisecs;
} TS_EVENT;

struct wmtts_device
{
	//data
	char* driver_name;
	char* ts_id;
	//function
	int (*init)(void);
	int (*probe)(struct platform_device *platdev);
	int (*remove)(struct platform_device *pdev);
	void (*exit)(void);
	int (*suspend)(struct platform_device *pdev, pm_message_t state);
	int (*resume)(struct platform_device *pdev);
	int (*capacitance_calibrate)(void);
	int (*wait_penup)(struct wmtts_device*tsdev); // waiting untill penup
	int penup; // 0--pendown;1--penup
	
};

//////////////////////////function interface/////////////////////////
extern  int wmt_ts_get_gpionum(void);
extern  int wmt_ts_get_resolvX(void);
extern  int wmt_ts_get_resolvY(void);
extern  int wmt_ts_set_rawcoord(unsigned short x, unsigned short y);
extern int wmt_set_gpirq(unsigned int num, int type);
extern int wmt_get_tsirqnum(void);
extern int wmt_disable_gpirq(unsigned int num);
extern int wmt_enable_gpirq(unsigned int num);
extern int wmt_is_tsirq_enable(int num);
extern void wmt_enable_rst_pull(int enable);
extern void wmt_set_rst_pull(int up);
extern void wmt_rst_output(int high);
void wmt_rst_input(void);
extern int wmt_is_tsint(int num);
extern void wmt_clr_int(int num);
extern void wmt_tsreset_init(int num);
extern int wmt_ts_get_resetgpnum(void);
extern int wmt_ts_get_lcdexchg(void);
extern unsigned char wmt_ts_get_i2caddr(void);
extern void wmt_ts_turnoff_light(void);
extern void wmt_ts_turnon_light(void);
extern int wmt_ts_enable_tskey(void);
extern int  wmt_ts_get_configfilename(char* fname);
extern int  wmt_ts_get_firmwfilename(char* fname);
extern int wmt_ts_get_xaxis(void);
extern int wmt_ts_get_xdir(void);
extern int wmt_ts_get_ydir(void);
extern int wmt_ts_get_fingernum(void);
extern int wmt_ts_enable_keyled(void);
extern int wmt_set_irq_mode(unsigned int num, int mode);
extern int wmt_disable_gpirq(unsigned int num);
extern int wmt_enable_gpirq(unsigned int num);
extern int ts_i2c_register_device (void);
extern struct i2c_client* ts_get_i2c_client(void);

extern void lw86x0_flash_write_prepare(void);
extern void lw86x0_flash_read(u8* pbData, u16 start_addr, u16 num);
extern void lw86x0_flash_write(u8* pbData,u16 start_addr, u16 num);
extern void lw86x0_flash_write_finish(u16 total_len);
#endif




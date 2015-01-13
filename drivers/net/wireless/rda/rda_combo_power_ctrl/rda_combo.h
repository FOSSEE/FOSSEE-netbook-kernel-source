#ifndef __RDA_COMBO_H__
#define __RDA_COMBO_H__

#include <linux/types.h>
#include <linux/string.h>
#include <linux/i2c.h>
#include <linux/wakelock.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <asm-generic/ioctl.h>
#include <asm/uaccess.h>

//if not RDA Platform,for example, you use Allwinner platform ,don't define RDA_KERNEL_PLATFORM
//#define RDA_KERNEL_PLATFORM        
#define RDA_BT_IOCTL_MAGIC 'u'

/* bt module */
#define RDA_BT_POWER_ON_IOCTL				   _IO(RDA_BT_IOCTL_MAGIC ,0x01)
#define RDA_BT_RF_INIT_IOCTL				   _IO(RDA_BT_IOCTL_MAGIC ,0x02)
#define RDA_BT_DC_CAL_IOCTL 				   _IO(RDA_BT_IOCTL_MAGIC ,0x03)
#define RDA_BT_RF_SWITCH_IOCTL				   _IO(RDA_BT_IOCTL_MAGIC ,0x04)
#define RDA_BT_POWER_OFF_IOCTL				   _IO(RDA_BT_IOCTL_MAGIC ,0x05)
#define RDA_BT_EN_CLK						   _IO(RDA_BT_IOCTL_MAGIC ,0x06)
#define RDA_BT_DC_DIG_RESET_IOCTL			   _IO(RDA_BT_IOCTL_MAGIC ,0x07)
#define RDA_BT_GET_ADDRESS_IOCTL			   _IO(RDA_BT_IOCTL_MAGIC ,0x08)
// add for pta
#define RDA_BT_DC_CAL_IOCTL_FIX_5991_LNA_GAIN           _IO(RDA_BT_IOCTL_MAGIC ,0x26)
// add for pta
/* wifi module */
#define RDA_WIFI_POWER_ON_IOCTL 			   _IO(RDA_BT_IOCTL_MAGIC ,0x10)
#define RDA_WIFI_POWER_OFF_IOCTL			   _IO(RDA_BT_IOCTL_MAGIC ,0x11)
#define RDA_WIFI_POWER_SET_TEST_MODE_IOCTL	   _IO(RDA_BT_IOCTL_MAGIC ,0x12)
#define RDA_WIFI_POWER_CANCEL_TEST_MODE_IOCTL  _IO(RDA_BT_IOCTL_MAGIC ,0x13)
#define RDA_WIFI_DEBUG_MODE_IOCTL			   _IO(RDA_BT_IOCTL_MAGIC ,0x14)
#define RDA_WLAN_COMBO_VERSION			   	   _IO(RDA_BT_IOCTL_MAGIC ,0x15)
#define RDA_COMBO_I2C_OPS    			   	   _IO(RDA_BT_IOCTL_MAGIC ,0x16)


//#define WLAN_USE_CRYSTAL // if use share crystal should close this
#define WLAN_USE_DCDC  // if use LDO mode, should close this
//#define WLAN_FOR_CTA		// if need pass CTA authenticate, should open this define

#define WLAN_USE_CRYSTAL
//config the correct channel according to schematic diagram
#define RDA_I2C_CHANNEL 	(4) 
#define RDA_WIFI_CORE_ADDR (0x13)
#define RDA_WIFI_RF_ADDR (0x14) //correct add is 0x14
#define RDA_BT_CORE_ADDR (0x15)
#define RDA_BT_RF_ADDR (0x16)

#define I2C_MASTER_ACK				(1<<0)
#define I2C_MASTER_RD				(1<<4)
#define I2C_MASTER_STO				(1<<8)
#define I2C_MASTER_WR				(1<<12)
#define I2C_MASTER_STA				(1<<16)
#define RDA_I2C_SPEED                100*1000
#define WLAN_VERSION_90_D (1)
#define WLAN_VERSION_90_E (2)
#define WLAN_VERSION_91   (3)
#define WLAN_VERSION_91_E (4)
#define WLAN_VERSION_91_F (5)

#define CLOCK_WLAN (1 << 0)
#define CLOCK_BT (1 << 1)
#define CLOCK_FM (1 << 2)
#define CLOCK_GPS (1 << 3)
#define CLOCK_MASK_ALL (0x0f)

#define I2C_DELAY_FLAG (0xFFFF)
#define DELAY_MS(x) {I2C_DELAY_FLAG, x},
#define RDA_WIFI_RF_I2C_DEVNAME "rda_wifi_rf_i2c"
#define RDA_WIFI_CORE_I2C_DEVNAME "rda_wifi_core_i2c"
#define RDA_BT_RF_I2C_DEVNAME "rda_bt_rf_i2c"
#define RDA_BT_CORE_I2C_DEVNAME "rda_bt_core_i2c"

extern struct i2c_client * rda_wifi_core_client;
extern struct i2c_client * rda_wifi_rf_client;
extern struct i2c_client * rda_bt_core_client;
extern struct i2c_client * rda_bt_rf_client;
extern struct completion rda_wifi_bt_comp;


int i2c_write_1_addr_2_data(struct i2c_client* client, const u8 addr, const u16 data);
int i2c_read_1_addr_2_data(struct i2c_client* client, const u8 addr, u16* data);
int rda_write_data_to_rf(struct i2c_client* client, const u16 (*data)[2], u32 count);
#define RDA_WRITE_DATA_TO_RF(CLIENT, ARRAY_DATA) rda_write_data_to_rf(CLIENT, ARRAY_DATA, sizeof(ARRAY_DATA)/sizeof(ARRAY_DATA[0]))

void enable_26m_regulator(u8 mask);
void disable_26m_regulator(u8 mask);
void enable_32k_rtc(u8 mask);
void disable_32k_rtc(u8 mask);
void enable_26m_rtc(u8 mask);
void disable_26m_rtc(u8 mask);

void rda_combo_i2c_lock(void);
void rda_combo_i2c_unlock(void);

u32 rda_wlan_version(void);
unsigned char rda_combo_wifi_in_test_mode(void);

int rda_5990_wifi_power_off(void);
int rda_5990_wifi_power_on(void);
int rda_5990_bt_power_on(void);
int rda_5990_bt_power_off(void);
int rda_5990_fm_power_on(void);
int rda_5990_fm_power_off(void);
long rda_5990_pw_ioctl(struct file *file, unsigned int cmd, unsigned long arg);

int rda_5991_wifi_power_on(void);
int rda_5991_wifi_power_off(void);
int rda_5991_bt_power_on(void);
int rda_5991_bt_power_off(void);
int rda_5991_fm_power_on(void);
int rda_5991_fm_power_off(void);
long rda_5991_pw_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
int rda_5991e_wifi_power_on(void);
int rda_5991f_wifi_power_on(void);
int rda_5991e_wifi_power_off(void);
int rda_5991f_wifi_power_off(void);
int rda_5991e_bt_power_on(void);
int rda_5991f_bt_power_on(void);
int rda_5991e_bt_power_off(void);
int rda_5991f_bt_power_off(void);
int rda_5991e_fm_power_on(void);
int rda_5991f_fm_power_on(void);
int rda_5991e_fm_power_off(void);
int rda_5991f_fm_power_off(void);
long rda_5991e_pw_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
long rda_5991f_pw_ioctl(struct file *file, unsigned int cmd, unsigned long arg);

#endif


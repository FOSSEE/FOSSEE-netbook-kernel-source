/*++
 * linux/drivers/video/wmt/devices/lcd-gl5001w.c
 * WonderMedia video post processor (VPP) driver
 *
 * Copyright c 2012  WonderMedia  Technologies, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * WonderMedia Technologies, Inc.
 * 4F, 533, Chung-Cheng Road, Hsin-Tien, Taipei 231, R.O.C
--*/


#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/power_supply.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/proc_fs.h>
#include <linux/gpio.h>//wangaq+
#include "../lcd.h"

//#include <mach/gpio-cs.h>
//#include <mach/wmt_env.h>
#include <mach/wmt_iomux.h>//wangaq+


#include "../lcd.h"//yangchen add

#define DRIVER_NAME "tc358768"
#define I2C_ADDR    0x0E
#define I2C_ADAPTER 0

#define DTYPE_DCS_SWRITE_0P     0X05
#define DTYPE_DCS_SWRITE_1P     0X15
#define DTYPE_DCS_LWRITE        0X39
#define DTYPE_GEN_LWRITE        0X29
#define DTYPE_GEN_SWRITE_2P     0X23
#define DTYPE_GEN_SWRITE_1P     0X13
#define DTYPE_GEN_SWRITE_0P     0X03


#define Gl5001w_Reset_Port      WMT_PIN_GP62_SUSGPIO0
#define Tc358768_Reset_Port     WMT_PIN_GP1_GPIO8
//#define BackLight_Power_PORT  WMT_PIN_GP0_GPIO0

static void lcd_set_power(int status);
static int save=1;
//static struct work_struct cw500_resume_work;
struct cw500_data {
    struct work_struct cw500_resume_work;
};
struct cw500_data *data;

#if 0
#define lcd_debug   printk
#else
#define lcd_debug(fmt...)   do { } while (0)
#endif

#define __unused __attribute__ ((unused))

static bool lcd_init_gl500 = false;


typedef enum {
    //LCD_BTL504880,
    LCD_YT50F62C6,
    LCD_BT050TN,
    LCD_ILI9806,
    LCD_OTM8019A,
    LCD_MAX
} lcd_panel_enum;

struct lcd_deviceid_struct{
    unsigned short id;
    unsigned char addr;
    unsigned char read_start;
    unsigned char len;
};

static struct lcd_deviceid_struct lcd_deviceid[LCD_MAX]={
    //{0x8009,0xa1,6,2},
    {0x9805,0xd3,5,2},
    {0x8009,0xa1,6,2},
    {0x9816,0xd3,5,2},
    {0x8019,0xa1,6,2}
};

//LCD_YT50F62C6
static unsigned char LCD_YT50F62C6_CMD_FF_3DATA[] = {0xFF,0xFF,0x98,0x05};        //EXTC Command Set enable register
static unsigned char LCD_YT50F62C6_CMD_FD_4DATA[] = {0xFD,0x03,0x13,0x44,0x00};
static unsigned char LCD_YT50F62C6_CMD_F8_15DATA[] = {0xF8,0x15,0x02,0x02,0x15,0x02,0x02,0x30,0x01,0x01,0x30,0x01,0x01,0x30,0x01,0x01};
static unsigned char LCD_YT50F62C6_CMD_B8_1DATA[] = {0xB8,0x73};        //DBI Type B Interface Setting
static unsigned char LCD_YT50F62C6_CMD_F1_1DATA[] = {0xF1,0x00};            //Gate Modulation
static unsigned char LCD_YT50F62C6_CMD_F2_3DATA[] = {0xF2,0x00,0x58,0x41};      //CR/EQ/PC
static unsigned char LCD_YT50F62C6_CMD_FC_3DATA[] = {0xFC,0x04,0x0F,0x01};
static unsigned char LCD_YT50F62C6_CMD_FE_1DATA[] = {0xFE,0x19};        //SRAM Repair
static unsigned char LCD_YT50F62C6_CMD_EB_2DATA[] = {0xEB,0x08,0x0F};       // 3 Gamma & Dithering
static unsigned char LCD_YT50F62C6_CMD_E0_16DATA[] = {0xE0,0x00,0x02,0x07,0x10,0x10,0x1D,0x0F,0x0B,0x00,0x03,0x02,0x0B,0x0C,0x33,0x2F,0x00};        //P-Gamma
static unsigned char LCD_YT50F62C6_CMD_E1_16DATA[] = {0xE1,0x00,0x02,0x07,0x10,0x10,0x17,0x0B,0x0B,0x00,0x03,0x02,0x0B,0x0C,0x33,0x2F,0x00};        //N-Gamma
static unsigned char LCD_YT50F62C6_CMD_C1_4DATA[] = {0xC1,0x13,0x26,0x06,0x26};     //Power Control 1
static unsigned char LCD_YT50F62C6_CMD_C7_1DATA[] = {0xC7,0xC4};
static unsigned char LCD_YT50F62C6_CMD_B1_3DATA[] = {0xB1,0x00,0x12,0x14};          //Frame Rate Control
static unsigned char LCD_YT50F62C6_CMD_B4_1DATA[] = {0xB4,0x02};            // 2 Dot Inversion
static unsigned char LCD_YT50F62C6_CMD_36_1DATA[] = {0x36,0x0A};          //Memory Access
static unsigned char LCD_YT50F62C6_CMD_3A_1DATA[] = {0x3A,0x77};            //16 & 18 & 24 bits
static unsigned char LCD_YT50F62C6_CMD_21_0DATA[] = {0x21};             //Display Inv-On
static unsigned char LCD_YT50F62C6_CMD_B0_1DATA[] = {0xB0,0x00};            //RGB I/F Polarity
static unsigned char LCD_YT50F62C6_CMD_B6_1DATA[] = {0xB6,0x01};            //CPU/RGB I/F Select
static unsigned char LCD_YT50F62C6_CMD_C2_1DATA[] = {0xC2,0x11};
static unsigned char LCD_YT50F62C6_CMD_11_0DATA[] = {0x11};                //Sleep out
static unsigned char LCD_YT50F62C6_CMD_29_0DATA[] = {0x29};                //Display on
static unsigned char LCD_YT50F62C6_CMD_2C_0DATA[] = {0x2C};                //Memory write

//LCD_BT050TN
static unsigned char LCD_BT050TN_CMD_FF00_0DATA[] = {0x00,0x00};
static unsigned char LCD_BT050TN_CMD_FF00_3DATA[] = {0xFF,0x80,0x09,0x01};
static unsigned char LCD_BT050TN_CMD_FF80_0DATA[] = {0x00,0x80};
static unsigned char LCD_BT050TN_CMD_FF80_2DATA[] = {0xFF,0x80,0x09};
static unsigned char LCD_BT050TN_CMD_FF03_0DATA[] = {0x00,0x03};
static unsigned char LCD_BT050TN_CMD_FF03_1DATA[] = {0xFF,0x01};
static unsigned char LCD_BT050TN_CMD_2100_0DATA[] = {0x00,0x00};
static unsigned char LCD_BT050TN_CMD_2100_1DATA[] = {0x21,0x00};
static unsigned char LCD_BT050TN_CMD_D800_0DATA[] = {0x00,0x00};
static unsigned char LCD_BT050TN_CMD_D800_2DATA[] = {0xD8,0x6F,0x6F};
static unsigned char LCD_BT050TN_CMD_C582_0DATA[] = {0x00,0x82};
static unsigned char LCD_BT050TN_CMD_C582_1DATA[] = {0xC5,0xA3};
static unsigned char LCD_BT050TN_CMD_C181_0DATA[] = {0x00,0x81};
static unsigned char LCD_BT050TN_CMD_C181_1DATA[] = {0xC1,0x66};
static unsigned char LCD_BT050TN_CMD_C1A1_0DATA[] = {0x00,0xA1};
static unsigned char LCD_BT050TN_CMD_C1A1_1DATA[] = {0xC1,0x08};
static unsigned char LCD_BT050TN_CMD_B4C0_0DATA[] = {0x00,0xB4};
static unsigned char LCD_BT050TN_CMD_B4C0_1DATA[] = {0xC0,0x50};
static unsigned char LCD_BT050TN_CMD_C0A3_0DATA[] = {0x00,0xA3};
static unsigned char LCD_BT050TN_CMD_C0A3_1DATA[] = {0xC0,0x00};
static unsigned char LCD_BT050TN_CMD_C489_0DATA[] = {0x00,0x89};
static unsigned char LCD_BT050TN_CMD_C489_1DATA[] = {0xC4,0x08};
static unsigned char LCD_BT050TN_CMD_C481_0DATA[] = {0x00,0x81};
static unsigned char LCD_BT050TN_CMD_C481_1DATA[] = {0xC4,0x83};
static unsigned char LCD_BT050TN_CMD_C590_0DATA[] = {0x00,0x90};
static unsigned char LCD_BT050TN_CMD_C590_3DATA[] = {0xC5,0x96,0xA7,0x01};
static unsigned char LCD_BT050TN_CMD_C5B1_0DATA[] = {0x00,0xB1};
static unsigned char LCD_BT050TN_CMD_C5B1_1DATA[] = {0xC5,0xA9};
static unsigned char LCD_BT050TN_CMD_D900_0DATA[] = {0x00,0x00};
static unsigned char LCD_BT050TN_CMD_D900_1DATA[] = {0xD9,0x15};
static unsigned char LCD_BT050TN_CMD_E100_0DATA[] = {0x00,0x00};
static unsigned char LCD_BT050TN_CMD_E100_16DATA[] = {0xE1,0x02,0x08,0x0E,0x10,0x09,0x1D,0x0E,0x0E,0x00,0x05,0x02,0x07,0x0E,0x24,0x23,0x1D};
static unsigned char LCD_BT050TN_CMD_E200_0DATA[] = {0x00,0x00};
static unsigned char LCD_BT050TN_CMD_E200_16DATA[] = {0xE2,0x02,0x08,0x0E,0x0F,0x09,0x1D,0x0E,0x0D,0x00,0x04,0x02,0x07,0x0E,0x25,0x23,0x1D};
static unsigned char LCD_BT050TN_CMD_0000_0DATA[] = {0x00,0x00};
static unsigned char LCD_BT050TN_CMD_0000_1DATA[] = {0x00,0x00};
static unsigned char LCD_BT050TN_CMD_B3A1_0DATA[] = {0x00,0xA1};
static unsigned char LCD_BT050TN_CMD_B3A1_1DATA[] = {0xB3,0x10};
static unsigned char LCD_BT050TN_CMD_B3A7_0DATA[] = {0x00,0xA7};
static unsigned char LCD_BT050TN_CMD_B3A7_1DATA[] = {0xB3,0x10};
static unsigned char LCD_BT050TN_CMD_C090_0DATA[] = {0x00,0x90};
static unsigned char LCD_BT050TN_CMD_C090_6DATA[] = {0xC0,0x00,0x44,0x00,0x00,0x00,0x03};
static unsigned char LCD_BT050TN_CMD_C1A6_0DATA[] = {0x00,0xA6};
static unsigned char LCD_BT050TN_CMD_C1A6_3DATA[] = {0xC1,0x00,0x00,0x00};
static unsigned char LCD_BT050TN_CMD_CE80_0DATA[] = {0x00,0x80};
static unsigned char LCD_BT050TN_CMD_CE80_6DATA[] = {0xCE,0x87,0x03,0x00,0x86,0x03,0x00};
static unsigned char LCD_BT050TN_CMD_CE90_0DATA[] = {0x00,0x90};
static unsigned char LCD_BT050TN_CMD_CE90_6DATA[] = {0xCE,0x33,0x1E,0x00,0x33,0x1F,0x00};
static unsigned char LCD_BT050TN_CMD_CEA0_0DATA[] = {0x00,0xA0};
static unsigned char LCD_BT050TN_CMD_CEA0_14DATA[] = {0xCE,0x38,0x03,0x03,0x20,0x00,0x00,0x00,0x38,0x02,0x03,0x21,0x00,0x00,0x00};
static unsigned char LCD_BT050TN_CMD_CEB0_0DATA[] = {0x00,0xb0};
static unsigned char LCD_BT050TN_CMD_CEB0_14DATA[] = {0xCE,0x38,0x01,0x03,0x22,0x00,0x00,0x00,0x38,0x00,0x03,0x23,0x00,0x00,0x00};
static unsigned char LCD_BT050TN_CMD_CEC0_0DATA[] = {0x00,0xC0};
static unsigned char LCD_BT050TN_CMD_CEC0_14DATA[] = {0xCE,0x30,0x00,0x03,0x24,0x00,0x00,0x00,0x30,0x01,0x03,0x25,0x00,0x00,0x00};
static unsigned char LCD_BT050TN_CMD_CED0_0DATA[] = {0x00,0xD0};
static unsigned char LCD_BT050TN_CMD_CED0_14DATA[] = {0xCE,0x30,0x02,0x03,0x26,0x00,0x00,0x00,0x30,0x03,0x03,0x27,0x00,0x00,0x00};
static unsigned char LCD_BT050TN_CMD_CFC6_0DATA[] = {0x00,0xC6};
static unsigned char LCD_BT050TN_CMD_CFC6_2DATA[] = {0xCF,0x01,0x80};
static unsigned char LCD_BT050TN_CMD_CFC9_0DATA[] = {0x00,0xC9};
static unsigned char LCD_BT050TN_CMD_CFC9_1DATA[] = {0xCF,0x00};
static unsigned char LCD_BT050TN_CMD_CBC0_0DATA[] = {0x00,0xC0};
static unsigned char LCD_BT050TN_CMD_CBC0_15DATA[] = {0xCB,0x00,0x04,0x04,0x04,0x04,0x00,0x00,0x04,0x04,0x04,0x04,0x00,0x00,0x00,0x00};
static unsigned char LCD_BT050TN_CMD_CBD0_0DATA[] = {0x00,0xD0};
static unsigned char LCD_BT050TN_CMD_CBD0_15DATA[] = {0xCB,0x00,0x00,0x00,0x00,0x00,0x00,0x04,0x04,0x04,0x04,0x00,0x00,0x04,0x04,0x04};
static unsigned char LCD_BT050TN_CMD_CBE0_0DATA[] = {0x00,0xE0};
static unsigned char LCD_BT050TN_CMD_CBE0_10DATA[] = {0xCB,0x04,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
static unsigned char LCD_BT050TN_CMD_CC80_0DATA[] = {0x00,0x80};
static unsigned char LCD_BT050TN_CMD_CC80_10DATA[] = {0xCC,0x00,0x26,0x25,0x02,0x06,0x00,0x00,0x0A,0x0E,0x0C};
static unsigned char LCD_BT050TN_CMD_CC90_0DATA[] = {0x00,0x90};
static unsigned char LCD_BT050TN_CMD_CC90_15DATA[] = {0xCC,0x10,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x26,0x25,0x01,0x05};
static unsigned char LCD_BT050TN_CMD_CCA0_0DATA[] = {0x00,0xA0};
static unsigned char LCD_BT050TN_CMD_CCA0_15DATA[] = {0xCC,0x00,0x00,0x09,0x0d,0x0b,0x0f,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
static unsigned char LCD_BT050TN_CMD_CCB0_0DATA[] = {0x00,0xB0};
static unsigned char LCD_BT050TN_CMD_CCB0_10DATA[] = {0xCC,0x00,0x25,0x26,0x05,0x01,0x00,0x00,0x0F,0x0B,0x0D};
static unsigned char LCD_BT050TN_CMD_CCC0_0DATA[] = {0x00,0xC0};
static unsigned char LCD_BT050TN_CMD_CCC0_15DATA[] = {0xCC,0x09,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x25,0x26,0x06,0x02};
static unsigned char LCD_BT050TN_CMD_CCD0_0DATA[] = {0x00,0xD0};
static unsigned char LCD_BT050TN_CMD_CCD0_15DATA[] = {0xCC,0x00,0x00,0x10,0x0c,0x0e,0x0a,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
static unsigned char LCD_BT050TN_CMD_21_0DATA[] = {0x21};
static unsigned char LCD_BT050TN_CMD_36_0DATA[] = {0x36};
static unsigned char LCD_BT050TN_CMD_00_0DATA[] = {0x00};
static unsigned char LCD_BT050TN_CMD_11_0DATA[] = {0x11};
static unsigned char LCD_BT050TN_CMD_29_0DATA[] = {0x29};
static unsigned char LCD_BT050TN_CMD_2C_0DATA[] = {0x2C};


//LCD_ILI9806
static unsigned char LCD_ILI9806_CMD_FF_3DATA[] = {0xFF,0xFF,0x98,0x16};
static unsigned char LCD_ILI9806_CMD_BA_1DATA[] = {0xBA,0x60};
static unsigned char LCD_ILI9806_CMD_F3_1DATA[] = {0xF3,0x70};
static unsigned char LCD_ILI9806_CMD_F9_3DATA[] = {0xF9,0x04,0xFB,0x84};
static unsigned char LCD_ILI9806_CMD_B0_1DATA[] = {0xB0,0x01};
static unsigned char LCD_ILI9806_CMD_BC_23DATA[] = {0xBC,0x01,0x0F,0x61,0xFE,0x01,0x01,0x0B,0x11,0x6C,0x63,0xFF,0xFF,0x01,0x01,0x00,0x00,0x55,0x53,0x01,0x00,0x00,0x43,0x0B};
static unsigned char LCD_ILI9806_CMD_BD_8DATA[] = {0xBD,0x01,0x23,0x45,0x67,0x01,0x23,0x45,0x67};
static unsigned char LCD_ILI9806_CMD_BE_17DATA[] = {0xBE,0x13,0x11,0x00,0x22,0x22,0xBA,0xAB,0x22,0x22,0x22,0x66,0x22,0x22,0x22,0x22,0x22,0x22};
static unsigned char LCD_ILI9806_CMD_ED_2DATA[] = {0xED,0x7F,0x0F};
static unsigned char LCD_ILI9806_CMD_B4_3DATA[] = {0xB4,0x02,0x02,0x02};
static unsigned char LCD_ILI9806_CMD_B5_4DATA[] = {0xB5,0x14,0x14,0x04,0x00};
static unsigned char LCD_ILI9806_CMD_C0_3DATA[] = {0xC0,0x7F,0x0B,0x04};
static unsigned char LCD_ILI9806_CMD_C1_4DATA[] = {0xC1,0x17,0x78,0x78,0x20};
static unsigned char LCD_ILI9806_CMD_D7_1DATA[] = {0xD7,0x2A};
static unsigned char LCD_ILI9806_CMD_D8_1DATA[] = {0xD8,0x28};
static unsigned char LCD_ILI9806_CMD_FC_1DATA[] = {0xFC,0x05};
static unsigned char LCD_ILI9806_CMD_E0_16DATA[] = {0xE0,0x00,0x03,0x0A,0x0E,0x11,0x15,0x0A,0x08,0x04,0x09,0x07,0x0D,0x0D,0x2E,0x28,0x00};
static unsigned char LCD_ILI9806_CMD_E1_16DATA[] = {0xE1,0x00,0x02,0x09,0x0E,0x11,0x15,0x0A,0x09,0x04,0x09,0x08,0x0C,0x0D,0x2F,0x28,0x00};
static unsigned char LCD_ILI9806_CMD_D5_8DATA[] = {0xD5,0x09,0x0A,0x0D,0x0B,0xCB,0xA5,0x01,0x04};
static unsigned char LCD_ILI9806_CMD_F7_1DATA[] = {0xF7,0x89};
static unsigned char LCD_ILI9806_CMD_C7_1DATA[] = {0xC7,0x43};
static unsigned char LCD_ILI9806_CMD_36_1DATA[] = {0x36,0x00};
static unsigned char LCD_ILI9806_CMD_51_1DATA[] = {0x51,0xFF};
static unsigned char LCD_ILI9806_CMD_53_1DATA[] = {0x53,0x24};
static unsigned char LCD_ILI9806_CMD_55_1DATA[] = {0x55,0x03};
static unsigned char LCD_ILI9806_CMD_11_0DATA[] = {0x11};
static unsigned char LCD_ILI9806_CMD_29_0DATA[] = {0x29};
static unsigned char LCD_ILI9806_CMD_2C_0DATA[] = {0x2C};                //Memory write

//OTM8019A
static unsigned char LCD_OTM8019A_CMD_1[]= {0x00,0x00};
static unsigned char LCD_OTM8019A_CMD_2[]= {0xFF,0x80,0x19,0x01};
static unsigned char LCD_OTM8019A_CMD_3[]= {0x00,0x80};
static unsigned char LCD_OTM8019A_CMD_4[]= {0xFF,0x80,0x19};
static unsigned char LCD_OTM8019A_CMD_5[]= {0x00,0x90};
static unsigned char LCD_OTM8019A_CMD_6[]= {0xB3,0x02};
static unsigned char LCD_OTM8019A_CMD_7[]= {0x00,0x92};
static unsigned char LCD_OTM8019A_CMD_8[]= {0xB3,0x45};
static unsigned char LCD_OTM8019A_CMD_9[]= {0x00,0xA2};
static unsigned char LCD_OTM8019A_CMD_10[]= {0xC0,0x04,0x00,0x02};
static unsigned char LCD_OTM8019A_CMD_11[]= {0x00,0x80};
static unsigned char LCD_OTM8019A_CMD_12[]= {0xC0,0x00,0x58,0x00,0x14,0x16};
static unsigned char LCD_OTM8019A_CMD_13[]= {0x00,0x90};
static unsigned char LCD_OTM8019A_CMD_14[]= {0xC0,0x00,0x15,0x00,0x00,0x00,0x03};
static unsigned char LCD_OTM8019A_CMD_15[]= {0x00,0xB4};
static unsigned char LCD_OTM8019A_CMD_16[]= {0xC0,0x70};//1+2 dot inversion
static unsigned char LCD_OTM8019A_CMD_17[]= {0x00,0x81};
static unsigned char LCD_OTM8019A_CMD_18[]= {0xC1,0x33};
static unsigned char LCD_OTM8019A_CMD_19[]= {0x00,0x80};
static unsigned char LCD_OTM8019A_CMD_20[]= {0xC4,0x30,0x83};
static unsigned char LCD_OTM8019A_CMD_21[]= {0x00,0x89};
static unsigned char LCD_OTM8019A_CMD_22[]= {0xC4,0x08};
static unsigned char LCD_OTM8019A_CMD_23[]= {0x00,0x82};
static unsigned char LCD_OTM8019A_CMD_24[]= {0xC5,0xB0};
static unsigned char LCD_OTM8019A_CMD_25[]= {0x00,0x90};
static unsigned char LCD_OTM8019A_CMD_26[]= {0xC5,0x4E,0x79,0x01,0x03};
static unsigned char LCD_OTM8019A_CMD_27[]= {0x00,0xB1};
static unsigned char LCD_OTM8019A_CMD_28[]= {0xC5,0xA9};
static unsigned char LCD_OTM8019A_CMD_29[]= {0x00,0x80};
static unsigned char LCD_OTM8019A_CMD_30[]= {0xCE,0x87,0x03,0x00,0x85,0x03,0x00,0x86,0x03,0x00,0x84,0x03,0x00};
static unsigned char LCD_OTM8019A_CMD_31[]= {0x00,0xA0};
static unsigned char LCD_OTM8019A_CMD_32[]= {0xCE,0x38,0x03,0x03,0x58,0x00,0x00,0x00,0x38,0x02,0x03,0x59,0x00,0x00,0x00};
static unsigned char LCD_OTM8019A_CMD_33[]= {0x00,0xB0};
static unsigned char LCD_OTM8019A_CMD_34[]= {0xCE,0x38,0x01,0x03,0x5A,0x00,0x00,0x00,0x38,0x00,0x03,0x5B,0x00,0x00,0x00};
static unsigned char LCD_OTM8019A_CMD_35[]= {0x00,0xC0};
static unsigned char LCD_OTM8019A_CMD_36[]= {0xCE,0x30,0x00,0x03,0x5C,0x00,0x00,0x00,0x30,0x01,0x03,0x5D,0x00,0x00,0x00};
static unsigned char LCD_OTM8019A_CMD_37[]= {0x00,0xD0};
static unsigned char LCD_OTM8019A_CMD_38[]= {0xCE,0x30,0x02,0x03,0x5E,0x00,0x00,0x00,0x30,0x03,0x03,0x5F,0x00,0x00,0x00};
static unsigned char LCD_OTM8019A_CMD_39[]= {0x00,0xC7};
static unsigned char LCD_OTM8019A_CMD_40[]= {0xCF,0x00};
static unsigned char LCD_OTM8019A_CMD_41[]= {0x00,0xC9};
static unsigned char LCD_OTM8019A_CMD_42[]= {0xCF,0x00};
static unsigned char LCD_OTM8019A_CMD_43[]= {0x00,0xC4};
static unsigned char LCD_OTM8019A_CMD_44[]= {0xCB,0x01,0x01,0x01,0x01,0x01,0x01};
static unsigned char LCD_OTM8019A_CMD_45[]= {0x00,0xD9};
static unsigned char LCD_OTM8019A_CMD_46[]= {0xCB,0x00,0x00,0x01,0x01,0x01,0x01};
static unsigned char LCD_OTM8019A_CMD_47[]= {0x00,0xE0};
static unsigned char LCD_OTM8019A_CMD_48[]= {0xCB,0x01,0x01};
static unsigned char LCD_OTM8019A_CMD_49[]= {0x00,0x84};
static unsigned char LCD_OTM8019A_CMD_50[]= {0xCC,0x0C,0x0A,0x10,0x0E,0x03,0x04};
static unsigned char LCD_OTM8019A_CMD_51[]= {0x00,0x9E};
static unsigned char LCD_OTM8019A_CMD_52[]= {0xCC,0x00};
static unsigned char LCD_OTM8019A_CMD_53[]= {0x00,0xA0};
static unsigned char LCD_OTM8019A_CMD_54[]= {0xCC,0x00,0x02,0x01,0x0d,0x0f,0x09,0x0b};
static unsigned char LCD_OTM8019A_CMD_55[]= {0x00,0xB4};
static unsigned char LCD_OTM8019A_CMD_56[]= {0xCC,0x0D,0x0F,0x09,0x0B,0x02,0x01};
static unsigned char LCD_OTM8019A_CMD_57[]= {0x00,0xCE};
static unsigned char LCD_OTM8019A_CMD_58[]= {0xCC,0x85};
static unsigned char LCD_OTM8019A_CMD_59[]= {0x00,0xD0};
static unsigned char LCD_OTM8019A_CMD_60[]= {0xCC,0x05,0x03,0x04,0x0c,0x0a,0x10,0x0e};
static unsigned char LCD_OTM8019A_CMD_61[]= {0x00,0x00};
static unsigned char LCD_OTM8019A_CMD_62[]= {0xD8,0x85,0x85};
static unsigned char LCD_OTM8019A_CMD_63[]= {0x00,0x00};
static unsigned char LCD_OTM8019A_CMD_64[]= {0xD9,0x61};
static unsigned char LCD_OTM8019A_CMD_65[]= {0x00,0x00};
static unsigned char LCD_OTM8019A_CMD_66[]= {0xE1,0x00,0x03,0x0a,0x1d,0x33,0x49,0x54,0x89,0x7a,0x8e,0x79,0x69,0x81,0x6d,0x73,0x6d,0x66,0x5d,0x52,0x00};
static unsigned char LCD_OTM8019A_CMD_67[]= {0x00,0x00};
static unsigned char LCD_OTM8019A_CMD_68[]= {0xE2,0x00,0x04,0x0a,0x1d,0x33,0x49,0x54,0x89,0x7a,0x8e,0x79,0x69,0x81,0x6d,0x73,0x6d,0x66,0x5d,0x52,0x00};
static unsigned char LCD_OTM8019A_CMD_69[]= {0x00,0x80};
static unsigned char LCD_OTM8019A_CMD_70[]= {0xC4,0x30};
static unsigned char LCD_OTM8019A_CMD_71[]= {0x00,0x98};
static unsigned char LCD_OTM8019A_CMD_72[]= {0xC0,0x00};
static unsigned char LCD_OTM8019A_CMD_73[]= {0x00,0xa9};
static unsigned char LCD_OTM8019A_CMD_74[]= {0xC0,0x06};
static unsigned char LCD_OTM8019A_CMD_75[]= {0x00,0xb0};
static unsigned char LCD_OTM8019A_CMD_76[]= {0xC1,0x20,0x00,0x00};
static unsigned char LCD_OTM8019A_CMD_77[]= {0x00,0xe1};
static unsigned char LCD_OTM8019A_CMD_78[]= {0xC0,0x40,0x18};
static unsigned char LCD_OTM8019A_CMD_79[]= {0x00,0x80};
static unsigned char LCD_OTM8019A_CMD_80[]= {0xC1,0x03,0x33};
static unsigned char LCD_OTM8019A_CMD_81[]= {0x00,0xA0};
static unsigned char LCD_OTM8019A_CMD_82[]= {0xC1,0xe8};
static unsigned char LCD_OTM8019A_CMD_83[]= {0x00,0x90};
static unsigned char LCD_OTM8019A_CMD_84[]= {0xb6,0xb4};
static unsigned char LCD_OTM8019A_CMD_85[]= {0x00,0x00};
static unsigned char LCD_OTM8019A_CMD_86[]= {0xfb,0x01};
static unsigned char LCD_OTM8019A_CMD_87[]= {0x00,0x00};
static unsigned char LCD_OTM8019A_CMD_88[]= {0xFF,0xFF,0xFF,0xFF};
static unsigned char LCD_OTM8019A_CMD_89[]= {0x11,0x00};
static unsigned char LCD_OTM8019A_CMD_90[]= {0x29,0x00};
//End   of
static struct i2c_client *tc358768_client;

static inline int tc358768_rd_reg_32bits(uint32_t reg_addr)
{
    int ret;
    struct i2c_msg msgs[2];
    uint8_t buf[4];
    struct i2c_client *client = tc358768_client;

    buf[0] = (reg_addr >> 8) & 0xff;
    buf[1] = reg_addr & 0xff;

    msgs[0].addr  = client->addr;
    msgs[0].flags = 0;
    msgs[0].len   = 2;
    msgs[0].buf   = buf;

    msgs[1].addr  = client->addr;
    msgs[1].flags = I2C_M_RD;
    msgs[1].len   = 2;
    msgs[1].buf   = buf;

    ret = i2c_transfer(client->adapter, msgs, 2);
    if (ret < 0) {
        printk("%s:i2c_transfer fail =%d\n", __func__, ret);
        return -1;
    }

    ret = (buf[0] << 8) | buf[1];

    return ret;
}

static inline int tc358768_wr_reg_32bits(uint32_t value)
{
    struct i2c_msg msgs;
    int ret = -1;
    uint8_t buf[4];
    struct i2c_client *client = tc358768_client;

    buf[0] = value>>24;
    buf[1] = value>>16;
    buf[2] = value>>8;
    buf[3] = value;

    msgs.addr  = client->addr;
    msgs.flags = 0;
    msgs.len   = 4;
    msgs.buf   = buf;

    ret = i2c_transfer(client->adapter, &msgs, 1);
    if(ret < 0)
        printk("%d:i2c_transfer fail = %d\n",__LINE__, ret);

    return ret;
}

static int _tc358768_wr_regs_32bits(unsigned int reg_array[], int n)
{

    int i = 0;
    lcd_debug("%s:%d\n", __func__, n);
    for(i = 0; i < n; i++) {
        if(reg_array[i] < 0x00020000) {
            if(reg_array[i] < 20000)
                udelay(reg_array[i]);
            else {
                mdelay(reg_array[i]/1000);
            }
        } else {
            tc358768_wr_reg_32bits(reg_array[i]);
        }
    }
    return 0;
}

#define tc358768_wr_regs_32bits(reg_array) \
    _tc358768_wr_regs_32bits(reg_array, ARRAY_SIZE(reg_array))

static uint32_t initialize[] = {
    // **************************************************
    // Initizlize  -> Display On after power-on
    // **************************************************
    // **************************************************
    // Power on TC358768XBG according to recommended power-on sequence
    // Relase reset (RESX="H")
    // Start input REFCK and PCLK
    // **************************************************
    // **************************************************
    // TC358768XBG Software Reset
    // **************************************************
    0x00020001, //SYSctl, S/W Reset
    10,
    0x00020000, //SYSctl, S/W Reset release

    // **************************************************
    // TC358768XBG PLL,Clock Setting
    // **************************************************
    0x00160063, //PLL Control Register 0 (PLL_PRD,PLL_FBD)
    0x00180603, //PLL_FRS,PLL_LBWS, PLL oscillation enable
    1000,
    0x00180613, //PLL_FRS,PLL_LBWS, PLL clock out enable

    // **************************************************
    // TC358768XBG DPI Input Control
    // **************************************************
    0x00060064, //FIFO Control Register

    // **************************************************
    // TC358768XBG D-PHY Setting
    // **************************************************
    0x01400000, //D-PHY Clock lane enable
    0x01420000, //
    0x01440000, //D-PHY Data lane0 enable
    0x01460000, //
    0x01480000, //D-PHY Data lane1 enable
    0x014A0000, //
    0x014C0001, //
    0x014E0001, //
    0x01500001, //
    0x01520001, //

    0x01000002, //
    0x01020000, //
    0x01040000, //
    0x01060002, //
    0x01080002, //
    0x010A0000, //
    0x010C0002, //
    0x010E0000, //
    0x01100002, //
    0x01120000, //
    // **************************************************
    // TC358768XBG DSI-TX PPI Control
    // **************************************************
    0x02100A5A, //LINEINITCNT
    0x02120000, //
    0x02140002, //LPTXTIMECNT
    0x02160000, //
    0x02180E02, //TCLK_HEADERCNT
    0x021A0000, //
    0x021C0000, //TCLK_TRAILCNT
    0x021E0000, //
    0x02200002, //THS_HEADERCNT
    0x02220000, //
    0x02244650, //TWAKEUPCNT
    0x02260000, //
    0x02280000, //TCLK_POSTCNT
    0x022A0000, //
    0x022C0001, //THS_TRAILCNT
    0x022E0000, //
    0x02300005, //HSTXVREGCNT
    0x02320000, //
    0x02340007, //HSTXVREGEN enable
    0x02360000, //
    0x02380001, //DSI clock Enable/Disable during LP
    0x023A0000, //
    0x023C0002, //BTACNTRL1
    0x023E0002, //
    0x02040001, //STARTCNTRL
    0x02060000, //

    // **************************************************
    // TC358768XBG DSI-TX Timing Control
    // **************************************************
    0x06200001, //Sync Event mode setting   Event mode
    0x06220014, //V Control Register1   VBP
    0x0624000C, //V Control Register2   not used
    0x06260356, //V Control Register3   800
    0x0628005E, //H Control Register1
    0x062A003F, //H Control Register2
    //0x062C0438, //H Control Register3   (480*18)/8=1080
    0x062C05A0, //H Control Register3   (480*24)/8=1440

    0x05180001, //DSI Start
    0x051A0000, //
};

static uint32_t start_dsi_hs_mode[] = {

    // **************************************************
    // Set to HS mode
    // **************************************************
    0x05000083, //DSI lane setting, DSI mode=HS
    0x0502A300, //bit set
    0x05008000, //Switch to DSI mode
    0x0502C300, //

    // **************************************************
    // Host: RGB(DPI) input start
    // **************************************************

    //0x00080047, //DSI-TX Format setting: RGB666
    0x00080037, //DSI-TX Format setting//3 RGB888;4 RGB666
    //0x0050001E, //DSI-TX Pixel stream packet Data Type setting
    0x0050003E, //Packed Pixel Stream, 24-bit RGB, 8-8-8 Format
    0x00320000, //HSYNC Polarity

    0x00040040, //Configuration Control Register

};

static inline void mipi_dsi_init(void)
{
    tc358768_wr_regs_32bits(initialize);
}

static inline void mipi_dsi_hs_start(void)
{
    tc358768_wr_regs_32bits(start_dsi_hs_mode);
}

static void tc_print(u32 addr)
{
    lcd_debug("+++addr->%04x: %04x\n", addr, tc358768_rd_reg_32bits(addr));
}

static int tc358768_command_tx_less8bytes(unsigned char type,
                      unsigned char *regs, int n)
{
    int i = 0;
    unsigned int command[] = {
        0x06020000,
        0x06040000,
        0x06100000,
        0x06120000,
        0x06140000,
        0x06160000,
    };

    if(n <= 2)
        command[0] |= 0x1000;   //short packet
    else {
        command[0] |= 0x4000;   //long packet
        command[1] |= n;        //word count byte
    }
    command[0] |= type;        //data type

    lcd_debug("*cmd:\n");
    lcd_debug("0x%08x\n", command[0]);
    lcd_debug("0x%08x\n", command[1]);

    for(i = 0; i < (n + 1)/2; i++) {
        command[i+2] |= regs[i*2];
        if((i*2 + 1) < n)
            command[i+2] |= regs[i*2 + 1] << 8;
        lcd_debug("0x%08x\n", command[i+2]);
    }

    _tc358768_wr_regs_32bits(command, (n + 1)/2 + 2);
    tc358768_wr_reg_32bits(0x06000001);   //Packet Transfer
    if(regs[0] == 0x29){
        //tc358768_wr_reg_32bits(0x06000001);
    }
    //wait until packet is out
    i = 100;
    while(tc358768_rd_reg_32bits(0x0600) & 0x01) {
        if(i-- == 0)
            break;
        tc_print(0x0600);
    }

    //udelay(50);
    return 0;
}

static int __unused tc358768_command_tx_more8bytes_hs(unsigned char type,
                              unsigned char regs[], int n)
{

    int i = 0;
    unsigned int dbg_data = 0x00E80000, temp = 0;
    unsigned int command[] = {
        0x05000080,     // HS data 4 lane, EOT is added
        0x0502A300,
        0x00080001,
        0x00500000,     // Data ID setting
        0x00220000,     // Transmission byte count= byte
        0x00E08000, // Enable I2C/SPI write to VB
        0x00E20048, // Total word count = 0x48 (max 0xFFF).
                // This value should be adjusted considering
                // trade off between transmission time and
                // transmission start/stop time delay
        0x00E4007F,     // Vertical blank line = 0x7F
    };


    command[3] |= type;        //data type
    command[4] |= n & 0xffff;           //Transmission byte count

    tc358768_wr_regs_32bits(command);

    for(i = 0; i < (n + 1)/2; i++) {
        temp = dbg_data | regs[i*2];
        if((i*2 + 1) < n)
            temp |= (regs[i*2 + 1] << 8);
        lcd_debug("0x%08x\n", temp);
        tc358768_wr_reg_32bits(temp);
    }
    if((n % 4 == 1) ||  (n % 4 == 2))     //4 bytes align
        tc358768_wr_reg_32bits(dbg_data);

    tc358768_wr_reg_32bits(0x00E0C000);     //Start command transmisison
    tc358768_wr_reg_32bits(0x00E00000);  //Stop command transmission. This setting should be done just after above setting to prevent multiple output
    udelay(200);
    //Re-Initialize
    //tc358768_wr_regs_32bits(re_initialize);
    return 0;
}

static int tc358768_command_tx_more8bytes_lp(unsigned char type,
                         unsigned char regs[], int n)
{

    int i = 0;
    unsigned int dbg_data = 0x00E80000, temp = 0;
    unsigned int command[] = {
        0x00080001,
        0x00500000,    //Data ID setting
        0x00220000,    //Transmission byte count= byte
        0x00E08000,    //Enable I2C/SPI write to VB
    };

    command[1] |= type;        //data type
    command[2] |= n & 0xffff;           //Transmission byte count

    tc358768_wr_regs_32bits(command);

    for(i = 0; i < (n + 1)/2; i++) {
        temp = dbg_data | regs[i*2];
        if((i*2 + 1) < n)
            temp |= (regs[i*2 + 1] << 8);
        lcd_debug("0x%08x\n", temp);
        tc358768_wr_reg_32bits(temp);

    }
    if((n % 4 == 1) ||  (n % 4 == 2))     //4 bytes align
        tc358768_wr_reg_32bits(dbg_data);

    tc358768_wr_reg_32bits(0x00E0E000);     //Start command transmisison
    udelay(1000);
    tc358768_wr_reg_32bits(0x00E02000);  //Keep Mask High to prevent short packets send out
    tc358768_wr_reg_32bits(0x00E00000);  //Stop command transmission. This setting should be done just after above setting to prevent multiple output
    udelay(10);
    return 0;
}

int _tc358768_send_packet(unsigned char type, unsigned char regs[], int n) {

    if(n <= 8) {
        tc358768_command_tx_less8bytes(type, regs, n);
    } else {
        //tc358768_command_tx_more8bytes_hs(type, regs, n);
        tc358768_command_tx_more8bytes_lp(type, regs, n);
    }
    return 0;
}

/*
 * The DCS is separated into two functional areas:
 *  the User Command Set and the Manufacturer Command Set.
 *  Each command is an eight-bit code with 00h to AFh assigned to
 *  the User Command Set and all other codes assigned to
 *  the Manufacturer Command Set.
 */
int _mipi_dsi_send_dcs_packet(unsigned char regs[], int n) {

    unsigned char type = 0;
    if(n == 1) {
        type = DTYPE_DCS_SWRITE_0P;
    } else if (n == 2) {
        type = DTYPE_DCS_SWRITE_1P;
    } else if (n > 2) {
        type = DTYPE_DCS_LWRITE;
    }
    _tc358768_send_packet(type, regs, n);
    return 0;
}

static int tc358768_command_read_bytes(unsigned char addr,unsigned char *regs, int n)
{
    unsigned short *data = (unsigned short *)regs,i;
    unsigned int command[] = {
        0x06021037,
        0x06040000,
        0x06100000,
        0x06000001,
        0x06021000,
        0x06040000,
        0x06100000,
        0x06000001,
    };

    lcd_debug("%s start addr=0x%x\n",__FUNCTION__,addr);

    command[2] |= 32;
    command[4] |= 0x14;
    command[6] |= addr;

    _tc358768_wr_regs_32bits(command,4);
    udelay(100);
    tc358768_wr_reg_32bits(0x05040010);
    tc358768_wr_reg_32bits(0x05060000);
    udelay(100);
    _tc358768_wr_regs_32bits(&command[4],4);

    while(n-- > 0){
        *data = (unsigned short)tc358768_rd_reg_32bits(0x0430);
        //printf("%s *data=0x%x\n",__FUNCTION__,*data);
        data++;
        *data = (unsigned short)tc358768_rd_reg_32bits(0x0432);
        //printf("%s *data=0x%x\n",__FUNCTION__,*data);
        data++;
        udelay(100);
    }

    return 0;
}

static lcd_panel_enum tc358768_check_lcd_type(void)
{
    unsigned char data[50]={0},i=0;
    int lcd_id;
    for(i=0;i<LCD_MAX;i++)
    {
        tc358768_command_read_bytes(lcd_deviceid[i].addr,data,9);
        lcd_id = (data[lcd_deviceid[i].read_start]<<8) + data[lcd_deviceid[i].read_start+1];
        printk("%s lcd_id=0x%x,0x%x\n",__FUNCTION__,lcd_id,lcd_deviceid[i].id);
        if(lcd_id == lcd_deviceid[i].id)
            break;
    }
    return i;
}

#define mipi_dsi_send_packet(type, regs) \
    _tc358768_send_packet(type, regs, ARRAY_SIZE(regs))

#define mipi_dsi_send_dcs_packet(regs) \
    _mipi_dsi_send_dcs_packet(regs, ARRAY_SIZE(regs))

static int gl5001w_reset(void)
{
    printk("wangaq:gl5001w_reset\n");

    gpio_direction_output(Gl5001w_Reset_Port,1);
    mdelay(2);
    gpio_direction_output(Gl5001w_Reset_Port,0);
    mdelay(15);
    gpio_direction_output(Gl5001w_Reset_Port,1);
    mdelay(5);

    return 0;
}

static int tc358768_reset(void)
{
    printk("wangaq:tc358768_reset\n");



    gpio_direction_output(Tc358768_Reset_Port,0);
    mdelay(20);
    gpio_direction_output(Tc358768_Reset_Port,1);
    mdelay(20);


    return 0;
}

static void set_backlight_power(int on)
{
    printk("set_backlight_power value=%d\n",on);
    //gpio_direction_output(BackLight_Power_PORT,on ? 1 : 0);

        printk("set_backlight_power oniff=%d\n",on ? 1 : 0);
}

static void inline backlight_on(void)
{
    set_backlight_power(1);
}

static void inline backlight_off(void)
{
    set_backlight_power(0);
}

static int gl5001w_hw_init(void)
{
    lcd_panel_enum lcd_id;

    mipi_dsi_init();
    lcd_id = tc358768_check_lcd_type();
    //lcd init
    //Start of xiaogang.zhang modified on 2013-4-22 14:22 1.0
    if(lcd_id==LCD_YT50F62C6)
    {
        printk("gl5001w_hw_init LCD_YT50F62C6\n");
        mipi_dsi_send_dcs_packet(LCD_YT50F62C6_CMD_FF_3DATA);
        mipi_dsi_send_dcs_packet(LCD_YT50F62C6_CMD_FD_4DATA);
        mipi_dsi_send_dcs_packet(LCD_YT50F62C6_CMD_F8_15DATA);
        mipi_dsi_send_dcs_packet(LCD_YT50F62C6_CMD_B8_1DATA);
        mipi_dsi_send_dcs_packet(LCD_YT50F62C6_CMD_F1_1DATA);
        mipi_dsi_send_dcs_packet(LCD_YT50F62C6_CMD_F2_3DATA);
        mipi_dsi_send_dcs_packet(LCD_YT50F62C6_CMD_FC_3DATA);
        mipi_dsi_send_dcs_packet(LCD_YT50F62C6_CMD_FE_1DATA);
        msleep(360);
        mipi_dsi_send_dcs_packet(LCD_YT50F62C6_CMD_EB_2DATA);
        mipi_dsi_send_dcs_packet(LCD_YT50F62C6_CMD_E0_16DATA);
        mipi_dsi_send_dcs_packet(LCD_YT50F62C6_CMD_E1_16DATA);
        mipi_dsi_send_dcs_packet(LCD_YT50F62C6_CMD_C1_4DATA);
        mipi_dsi_send_dcs_packet(LCD_YT50F62C6_CMD_C7_1DATA);
        mipi_dsi_send_dcs_packet(LCD_YT50F62C6_CMD_B1_3DATA);
        mipi_dsi_send_dcs_packet(LCD_YT50F62C6_CMD_B4_1DATA);
        mipi_dsi_send_dcs_packet(LCD_YT50F62C6_CMD_36_1DATA);
        mipi_dsi_send_dcs_packet(LCD_YT50F62C6_CMD_3A_1DATA);
        mipi_dsi_send_dcs_packet(LCD_YT50F62C6_CMD_21_0DATA);
        mipi_dsi_send_dcs_packet(LCD_YT50F62C6_CMD_B0_1DATA);
        mipi_dsi_send_dcs_packet(LCD_YT50F62C6_CMD_B6_1DATA);
        mipi_dsi_send_dcs_packet(LCD_YT50F62C6_CMD_C2_1DATA);

        mipi_dsi_send_dcs_packet(LCD_YT50F62C6_CMD_11_0DATA);
        msleep(120);
        mipi_dsi_send_dcs_packet(LCD_YT50F62C6_CMD_29_0DATA);

        msleep(20);
        mipi_dsi_send_dcs_packet(LCD_YT50F62C6_CMD_2C_0DATA);
    }
    else if(lcd_id==LCD_BT050TN)
    {
        printk("gl5001w_hw_init LCD_BT050TN\n");
        mipi_dsi_send_dcs_packet(LCD_BT050TN_CMD_FF00_0DATA);
        mipi_dsi_send_dcs_packet(LCD_BT050TN_CMD_FF00_3DATA);
        mipi_dsi_send_dcs_packet(LCD_BT050TN_CMD_FF80_0DATA);
        mipi_dsi_send_dcs_packet(LCD_BT050TN_CMD_FF80_2DATA);
        mipi_dsi_send_dcs_packet(LCD_BT050TN_CMD_FF03_0DATA);
        mipi_dsi_send_dcs_packet(LCD_BT050TN_CMD_FF03_1DATA);
        mipi_dsi_send_dcs_packet(LCD_BT050TN_CMD_2100_0DATA);
        mipi_dsi_send_dcs_packet(LCD_BT050TN_CMD_2100_1DATA);
        mipi_dsi_send_dcs_packet(LCD_BT050TN_CMD_D800_0DATA);
        mipi_dsi_send_dcs_packet(LCD_BT050TN_CMD_D800_2DATA);
        mipi_dsi_send_dcs_packet(LCD_BT050TN_CMD_C582_0DATA);
        mipi_dsi_send_dcs_packet(LCD_BT050TN_CMD_C582_1DATA);
        mipi_dsi_send_dcs_packet(LCD_BT050TN_CMD_C181_0DATA);
        mipi_dsi_send_dcs_packet(LCD_BT050TN_CMD_C181_1DATA);
        mipi_dsi_send_dcs_packet(LCD_BT050TN_CMD_C1A1_0DATA);
        mipi_dsi_send_dcs_packet(LCD_BT050TN_CMD_C1A1_1DATA);
        mipi_dsi_send_dcs_packet(LCD_BT050TN_CMD_B4C0_0DATA);
        mipi_dsi_send_dcs_packet(LCD_BT050TN_CMD_B4C0_1DATA);
        mipi_dsi_send_dcs_packet(LCD_BT050TN_CMD_C0A3_0DATA);
        mipi_dsi_send_dcs_packet(LCD_BT050TN_CMD_C0A3_1DATA);
        mipi_dsi_send_dcs_packet(LCD_BT050TN_CMD_C489_0DATA);
        mipi_dsi_send_dcs_packet(LCD_BT050TN_CMD_C489_1DATA);
        mipi_dsi_send_dcs_packet(LCD_BT050TN_CMD_C481_0DATA);
        mipi_dsi_send_dcs_packet(LCD_BT050TN_CMD_C481_1DATA);
        mipi_dsi_send_dcs_packet(LCD_BT050TN_CMD_C590_0DATA);
        mipi_dsi_send_dcs_packet(LCD_BT050TN_CMD_C590_3DATA);
        mipi_dsi_send_dcs_packet(LCD_BT050TN_CMD_C5B1_0DATA);
        mipi_dsi_send_dcs_packet(LCD_BT050TN_CMD_C5B1_1DATA);
        mipi_dsi_send_dcs_packet(LCD_BT050TN_CMD_D900_0DATA);
        mipi_dsi_send_dcs_packet(LCD_BT050TN_CMD_D900_1DATA);
        mipi_dsi_send_dcs_packet(LCD_BT050TN_CMD_E100_0DATA);
        mipi_dsi_send_dcs_packet(LCD_BT050TN_CMD_E100_16DATA);
        mipi_dsi_send_dcs_packet(LCD_BT050TN_CMD_E200_0DATA);
        mipi_dsi_send_dcs_packet(LCD_BT050TN_CMD_E200_16DATA);
        mipi_dsi_send_dcs_packet(LCD_BT050TN_CMD_0000_0DATA);
        mipi_dsi_send_dcs_packet(LCD_BT050TN_CMD_0000_1DATA);
        mipi_dsi_send_dcs_packet(LCD_BT050TN_CMD_B3A1_0DATA);
        mipi_dsi_send_dcs_packet(LCD_BT050TN_CMD_B3A1_1DATA);
        mipi_dsi_send_dcs_packet(LCD_BT050TN_CMD_B3A7_0DATA);
        mipi_dsi_send_dcs_packet(LCD_BT050TN_CMD_B3A7_1DATA);
        mipi_dsi_send_dcs_packet(LCD_BT050TN_CMD_C090_0DATA);
        mipi_dsi_send_dcs_packet(LCD_BT050TN_CMD_C090_6DATA);
        mipi_dsi_send_dcs_packet(LCD_BT050TN_CMD_C1A6_0DATA);
        mipi_dsi_send_dcs_packet(LCD_BT050TN_CMD_C1A6_3DATA);
        mipi_dsi_send_dcs_packet(LCD_BT050TN_CMD_CE80_0DATA);
        mipi_dsi_send_dcs_packet(LCD_BT050TN_CMD_CE80_6DATA);
        mipi_dsi_send_dcs_packet(LCD_BT050TN_CMD_CE90_0DATA);
        mipi_dsi_send_dcs_packet(LCD_BT050TN_CMD_CE90_6DATA);
        mipi_dsi_send_dcs_packet(LCD_BT050TN_CMD_CEA0_0DATA);
        mipi_dsi_send_dcs_packet(LCD_BT050TN_CMD_CEA0_14DATA);
        mipi_dsi_send_dcs_packet(LCD_BT050TN_CMD_CEB0_0DATA);
        mipi_dsi_send_dcs_packet(LCD_BT050TN_CMD_CEB0_14DATA);
        mipi_dsi_send_dcs_packet(LCD_BT050TN_CMD_CEC0_0DATA);
        mipi_dsi_send_dcs_packet(LCD_BT050TN_CMD_CEC0_14DATA);
        mipi_dsi_send_dcs_packet(LCD_BT050TN_CMD_CED0_0DATA);
        mipi_dsi_send_dcs_packet(LCD_BT050TN_CMD_CED0_14DATA);
        mipi_dsi_send_dcs_packet(LCD_BT050TN_CMD_CFC6_0DATA);
        mipi_dsi_send_dcs_packet(LCD_BT050TN_CMD_CFC6_2DATA);
        mipi_dsi_send_dcs_packet(LCD_BT050TN_CMD_CFC9_0DATA);
        mipi_dsi_send_dcs_packet(LCD_BT050TN_CMD_CFC9_1DATA);
        mipi_dsi_send_dcs_packet(LCD_BT050TN_CMD_CBC0_0DATA);
        mipi_dsi_send_dcs_packet(LCD_BT050TN_CMD_CBC0_15DATA);
        mipi_dsi_send_dcs_packet(LCD_BT050TN_CMD_CBD0_0DATA);
        mipi_dsi_send_dcs_packet(LCD_BT050TN_CMD_CBD0_15DATA);
        mipi_dsi_send_dcs_packet(LCD_BT050TN_CMD_CBE0_0DATA);
        mipi_dsi_send_dcs_packet(LCD_BT050TN_CMD_CBE0_10DATA);
        mipi_dsi_send_dcs_packet(LCD_BT050TN_CMD_CC80_0DATA);
        mipi_dsi_send_dcs_packet(LCD_BT050TN_CMD_CC80_10DATA);
        mipi_dsi_send_dcs_packet(LCD_BT050TN_CMD_CC90_0DATA);
        mipi_dsi_send_dcs_packet(LCD_BT050TN_CMD_CC90_15DATA);
        mipi_dsi_send_dcs_packet(LCD_BT050TN_CMD_CCA0_0DATA);
        mipi_dsi_send_dcs_packet(LCD_BT050TN_CMD_CCA0_15DATA);
        mipi_dsi_send_dcs_packet(LCD_BT050TN_CMD_CCB0_0DATA);
        mipi_dsi_send_dcs_packet(LCD_BT050TN_CMD_CCB0_10DATA);
        mipi_dsi_send_dcs_packet(LCD_BT050TN_CMD_CCC0_0DATA);
        mipi_dsi_send_dcs_packet(LCD_BT050TN_CMD_CCC0_15DATA);
        mipi_dsi_send_dcs_packet(LCD_BT050TN_CMD_CCD0_0DATA);
        mipi_dsi_send_dcs_packet(LCD_BT050TN_CMD_CCD0_15DATA);

        mipi_dsi_send_dcs_packet(LCD_BT050TN_CMD_21_0DATA);
        mipi_dsi_send_dcs_packet(LCD_BT050TN_CMD_36_0DATA);
        mipi_dsi_send_dcs_packet(LCD_BT050TN_CMD_00_0DATA);
        mipi_dsi_send_dcs_packet(LCD_BT050TN_CMD_11_0DATA);
        msleep(120);
        mipi_dsi_send_dcs_packet(LCD_BT050TN_CMD_29_0DATA);
        msleep(20);
        mipi_dsi_send_dcs_packet(LCD_BT050TN_CMD_2C_0DATA);
    }
    else if(lcd_id==LCD_ILI9806)
    {
        printk("gl5001w_hw_init LCD_ILI9806\n");

        mipi_dsi_send_dcs_packet(LCD_ILI9806_CMD_FF_3DATA);
        mipi_dsi_send_dcs_packet(LCD_ILI9806_CMD_BA_1DATA);
        mipi_dsi_send_dcs_packet(LCD_ILI9806_CMD_F3_1DATA);
        mipi_dsi_send_dcs_packet(LCD_ILI9806_CMD_F9_3DATA);
        msleep(10);
        mipi_dsi_send_dcs_packet(LCD_ILI9806_CMD_B0_1DATA);
        mipi_dsi_send_dcs_packet(LCD_ILI9806_CMD_BC_23DATA);
        mipi_dsi_send_dcs_packet(LCD_ILI9806_CMD_BD_8DATA);
        mipi_dsi_send_dcs_packet(LCD_ILI9806_CMD_BE_17DATA);
        mipi_dsi_send_dcs_packet(LCD_ILI9806_CMD_ED_2DATA);
        mipi_dsi_send_dcs_packet(LCD_ILI9806_CMD_B4_3DATA);
        mipi_dsi_send_dcs_packet(LCD_ILI9806_CMD_B5_4DATA);
        mipi_dsi_send_dcs_packet(LCD_ILI9806_CMD_C0_3DATA);
        mipi_dsi_send_dcs_packet(LCD_ILI9806_CMD_C1_4DATA);
        mipi_dsi_send_dcs_packet(LCD_ILI9806_CMD_D7_1DATA);
        mipi_dsi_send_dcs_packet(LCD_ILI9806_CMD_D8_1DATA);
        mipi_dsi_send_dcs_packet(LCD_ILI9806_CMD_FC_1DATA);
        mipi_dsi_send_dcs_packet(LCD_ILI9806_CMD_E0_16DATA);
        mipi_dsi_send_dcs_packet(LCD_ILI9806_CMD_E1_16DATA);
        mipi_dsi_send_dcs_packet(LCD_ILI9806_CMD_D5_8DATA);
        mipi_dsi_send_dcs_packet(LCD_ILI9806_CMD_F7_1DATA);
        mipi_dsi_send_dcs_packet(LCD_ILI9806_CMD_C7_1DATA);
        msleep(10);
        mipi_dsi_send_dcs_packet(LCD_ILI9806_CMD_36_1DATA);
        mipi_dsi_send_dcs_packet(LCD_ILI9806_CMD_51_1DATA);
        mipi_dsi_send_dcs_packet(LCD_ILI9806_CMD_53_1DATA);
        mipi_dsi_send_dcs_packet(LCD_ILI9806_CMD_55_1DATA);
        mipi_dsi_send_dcs_packet(LCD_ILI9806_CMD_11_0DATA);
        msleep(120);
        mipi_dsi_send_dcs_packet(LCD_ILI9806_CMD_29_0DATA);
        msleep(20);
        mipi_dsi_send_dcs_packet(LCD_ILI9806_CMD_2C_0DATA);
    }
    else if(lcd_id==LCD_OTM8019A)
    {
        printk("gl5001w_hw_init LCD_OTM8019A\n");

        mipi_dsi_send_dcs_packet(LCD_OTM8019A_CMD_1);
        mipi_dsi_send_dcs_packet(LCD_OTM8019A_CMD_2);
        mipi_dsi_send_dcs_packet(LCD_OTM8019A_CMD_3);
        mipi_dsi_send_dcs_packet(LCD_OTM8019A_CMD_4);
        mipi_dsi_send_dcs_packet(LCD_OTM8019A_CMD_5);
        mipi_dsi_send_dcs_packet(LCD_OTM8019A_CMD_6);
        mipi_dsi_send_dcs_packet(LCD_OTM8019A_CMD_7);
        mipi_dsi_send_dcs_packet(LCD_OTM8019A_CMD_8);
        mipi_dsi_send_dcs_packet(LCD_OTM8019A_CMD_9);
        mipi_dsi_send_dcs_packet(LCD_OTM8019A_CMD_10);
        mipi_dsi_send_dcs_packet(LCD_OTM8019A_CMD_11);
        mipi_dsi_send_dcs_packet(LCD_OTM8019A_CMD_12);
        mipi_dsi_send_dcs_packet(LCD_OTM8019A_CMD_13);
        mipi_dsi_send_dcs_packet(LCD_OTM8019A_CMD_14);
        mipi_dsi_send_dcs_packet(LCD_OTM8019A_CMD_15);
        mipi_dsi_send_dcs_packet(LCD_OTM8019A_CMD_16);
        mipi_dsi_send_dcs_packet(LCD_OTM8019A_CMD_17);
        mipi_dsi_send_dcs_packet(LCD_OTM8019A_CMD_18);
        mipi_dsi_send_dcs_packet(LCD_OTM8019A_CMD_19);
        mipi_dsi_send_dcs_packet(LCD_OTM8019A_CMD_20);
        mipi_dsi_send_dcs_packet(LCD_OTM8019A_CMD_21);
        mipi_dsi_send_dcs_packet(LCD_OTM8019A_CMD_22);
        mipi_dsi_send_dcs_packet(LCD_OTM8019A_CMD_23);
        mipi_dsi_send_dcs_packet(LCD_OTM8019A_CMD_24);
        mipi_dsi_send_dcs_packet(LCD_OTM8019A_CMD_25);
        mipi_dsi_send_dcs_packet(LCD_OTM8019A_CMD_26);
        mipi_dsi_send_dcs_packet(LCD_OTM8019A_CMD_27);
        mipi_dsi_send_dcs_packet(LCD_OTM8019A_CMD_28);
        mipi_dsi_send_dcs_packet(LCD_OTM8019A_CMD_29);
        mipi_dsi_send_dcs_packet(LCD_OTM8019A_CMD_30);
        mipi_dsi_send_dcs_packet(LCD_OTM8019A_CMD_31);
        mipi_dsi_send_dcs_packet(LCD_OTM8019A_CMD_32);
        mipi_dsi_send_dcs_packet(LCD_OTM8019A_CMD_33);
        mipi_dsi_send_dcs_packet(LCD_OTM8019A_CMD_34);
        mipi_dsi_send_dcs_packet(LCD_OTM8019A_CMD_35);
        mipi_dsi_send_dcs_packet(LCD_OTM8019A_CMD_36);
        mipi_dsi_send_dcs_packet(LCD_OTM8019A_CMD_37);
        mipi_dsi_send_dcs_packet(LCD_OTM8019A_CMD_38);
        mipi_dsi_send_dcs_packet(LCD_OTM8019A_CMD_39);
        mipi_dsi_send_dcs_packet(LCD_OTM8019A_CMD_40);
        mipi_dsi_send_dcs_packet(LCD_OTM8019A_CMD_41);
        mipi_dsi_send_dcs_packet(LCD_OTM8019A_CMD_42);
        mipi_dsi_send_dcs_packet(LCD_OTM8019A_CMD_43);
        mipi_dsi_send_dcs_packet(LCD_OTM8019A_CMD_44);
        mipi_dsi_send_dcs_packet(LCD_OTM8019A_CMD_45);
        mipi_dsi_send_dcs_packet(LCD_OTM8019A_CMD_46);
        mipi_dsi_send_dcs_packet(LCD_OTM8019A_CMD_47);
        mipi_dsi_send_dcs_packet(LCD_OTM8019A_CMD_48);
        mipi_dsi_send_dcs_packet(LCD_OTM8019A_CMD_49);
        mipi_dsi_send_dcs_packet(LCD_OTM8019A_CMD_50);
        mipi_dsi_send_dcs_packet(LCD_OTM8019A_CMD_51);
        mipi_dsi_send_dcs_packet(LCD_OTM8019A_CMD_52);
        mipi_dsi_send_dcs_packet(LCD_OTM8019A_CMD_53);
        mipi_dsi_send_dcs_packet(LCD_OTM8019A_CMD_54);
        mipi_dsi_send_dcs_packet(LCD_OTM8019A_CMD_55);
        mipi_dsi_send_dcs_packet(LCD_OTM8019A_CMD_56);
        mipi_dsi_send_dcs_packet(LCD_OTM8019A_CMD_57);
        mipi_dsi_send_dcs_packet(LCD_OTM8019A_CMD_58);
        mipi_dsi_send_dcs_packet(LCD_OTM8019A_CMD_59);
        mipi_dsi_send_dcs_packet(LCD_OTM8019A_CMD_60);
        mipi_dsi_send_dcs_packet(LCD_OTM8019A_CMD_61);
        mipi_dsi_send_dcs_packet(LCD_OTM8019A_CMD_62);
        mipi_dsi_send_dcs_packet(LCD_OTM8019A_CMD_63);
        mipi_dsi_send_dcs_packet(LCD_OTM8019A_CMD_64);
        mipi_dsi_send_dcs_packet(LCD_OTM8019A_CMD_65);
        mipi_dsi_send_dcs_packet(LCD_OTM8019A_CMD_66);
        mipi_dsi_send_dcs_packet(LCD_OTM8019A_CMD_67);
        mipi_dsi_send_dcs_packet(LCD_OTM8019A_CMD_68);
        mipi_dsi_send_dcs_packet(LCD_OTM8019A_CMD_69);
        mipi_dsi_send_dcs_packet(LCD_OTM8019A_CMD_70);
        mipi_dsi_send_dcs_packet(LCD_OTM8019A_CMD_71);
        mipi_dsi_send_dcs_packet(LCD_OTM8019A_CMD_72);
        mipi_dsi_send_dcs_packet(LCD_OTM8019A_CMD_73);
        mipi_dsi_send_dcs_packet(LCD_OTM8019A_CMD_74);
        mipi_dsi_send_dcs_packet(LCD_OTM8019A_CMD_76);
        mipi_dsi_send_dcs_packet(LCD_OTM8019A_CMD_77);
        mipi_dsi_send_dcs_packet(LCD_OTM8019A_CMD_78);
        mipi_dsi_send_dcs_packet(LCD_OTM8019A_CMD_79);
        mipi_dsi_send_dcs_packet(LCD_OTM8019A_CMD_80);
        mipi_dsi_send_dcs_packet(LCD_OTM8019A_CMD_81);
        mipi_dsi_send_dcs_packet(LCD_OTM8019A_CMD_82);
        mipi_dsi_send_dcs_packet(LCD_OTM8019A_CMD_83);
        mipi_dsi_send_dcs_packet(LCD_OTM8019A_CMD_84);
        mipi_dsi_send_dcs_packet(LCD_OTM8019A_CMD_85);
        mipi_dsi_send_dcs_packet(LCD_OTM8019A_CMD_86);
        mipi_dsi_send_dcs_packet(LCD_OTM8019A_CMD_87);
        mipi_dsi_send_dcs_packet(LCD_OTM8019A_CMD_88);
        mipi_dsi_send_dcs_packet(LCD_OTM8019A_CMD_89);
        msleep(120);
        mipi_dsi_send_dcs_packet(LCD_OTM8019A_CMD_90);
        msleep(120);

    }
    else
    {
        printk("gl5001w_hw_init can't find lcd-\n");
        return 0;
    }
    msleep(1);
    mipi_dsi_hs_start();
    msleep(10);

    return 0;
}


static void lcd_gl5001w_power_on(void)
{
    int ret;

    printk("lcd_gl5001w_power_on\n");
    gl5001w_reset();
    tc358768_reset();

    ret = tc358768_rd_reg_32bits(0);
    if(ret == 0x4401) {
        printk("+TC358768AXBG works ok\n");
    } else {
        printk("+TC358768AXBG error , read:0x%0x\n", ret);
        return;
    }

    gl5001w_hw_init();
}

static void lcd_gl5001w_power_off(void)
{
    printk("lcd_gl5001w_power_off\n");

    gpio_direction_output(Gl5001w_Reset_Port,0);

    lcd_init_gl500 = true;
}

static int tc358768_probe(struct i2c_client *client,
              const struct i2c_device_id *did)
{
    struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
    int ret = 0;

    if (!i2c_check_functionality(adapter, I2C_FUNC_I2C)) {
        dev_warn(&adapter->dev,
             "I2C-Adapter doesn't support I2C_FUNC_I2C\n");
        return -EIO;
    }

    tc358768_client = client;
    return ret;
}

static int tc358768_remove(struct i2c_client *client)
{
    return 0;
}

static const struct i2c_device_id tc358768_id[] = {
    { DRIVER_NAME, 0 },
    {},
};
MODULE_DEVICE_TABLE(i2c, tc358768_id);






static void cw500_resume_work_func(struct work_struct *work)
{
    save=0;
    //printk("-cw500_resume_work_func-\n");
    lcd_set_enable(1);
    lcd_gl5001w_power_on();
    return NULL;
}


static int  get_gpio_lcd_gl5001w(void)
{
    int ret = 0;
    ret = gpio_request(Gl5001w_Reset_Port,"Gl5001w_Reset_Port");
    if (ret){
        printk("gpio_request Gl5001w_Reset_Port for GL5001W failed\n");
        goto err_get;
    }

    ret = gpio_request(Tc358768_Reset_Port,"Tc358768_Reset_Port");
    if (ret){
        printk("gpio_request Tc358768_Reset_Port for Tc358768 failed\n");
        goto err_get;
    }
    //ret = gpio_request(BackLight_Power_PORT,"BackLight_Power_PORT");
    //if (ret){
    //  printk("gpio_request BackLight_Power_PORT for backlight failed\n");
    //  goto err_get;
    //}
    return 0;
err_get:
    return -1;
}
static void  put_gpio_lcd_gl5001w(void)
{
    gpio_free(Gl5001w_Reset_Port);
    gpio_free(Tc358768_Reset_Port);
    //gpio_free(BackLight_Power_PORT);
    return;
}


static int cw500_gl5001_suspend(struct i2c_client *c, pm_message_t state)
{
        save=1;
    printk("-cw500_gl5001_suspend-\n");
    put_gpio_lcd_gl5001w();
    return 0;
}


static int cw500_gl5001_resume(struct i2c_client *c)
{

    printk("-cw500_gl5001_resume\n");
    get_gpio_lcd_gl5001w();
    schedule_work(&data->cw500_resume_work);
    return 0;
}

static struct i2c_driver tc358768_driver = {
    .driver = {
        .name = DRIVER_NAME,
    },
    .probe = tc358768_probe,
    .remove = __devexit_p(tc358768_remove),
    .id_table = tc358768_id,
    .resume= cw500_gl5001_resume,
    .suspend= cw500_gl5001_suspend,
};

static void lcd_set_power(int status)
{


    if (!lcd_init_gl500) {
        return;
    }

    if (save==0) {
        return;
    }

    if (status == LCD_POWER_ON)
        {
        lcd_gl5001w_power_on();
        lcd_init_gl500 = false;
        }
}

static struct lcd_parm_t lcd_gl5001w_parm = {
    //.name = "GL5001W",
    //.fps = 60,                /* frame per second */
    .bits_per_pixel = 24,
    .capability = 0,
    .width = 480,
    .height = 854,
    .vmode = {
        .name = "GL5001W",          // mipi_480x800
        .refresh = 60,
        .xres = 480,
        .yres = 854,
        .pixclock = KHZ2PICOS(26000),             //pixel_clock
        .left_margin = 20,                        //hbp
        .right_margin = 30,                       //hfp
        .upper_margin = 12,                        //vbp
        .lower_margin = 8,                        //vfp
        .hsync_len = 10,                          //hsync
        .vsync_len = 8,                           //vsync
        .sync = 0,                                //?
        .vmode = 0,
        .flag = 0,
    },
    //.initial = dummy_call_path,
    //.uninitial = lcd_gl5001w_power_off,
    //.set_power = lcd_set_power,
};

static struct lcd_parm_t *lcd_gl5001w_get_parm(int arg)
{
    return &lcd_gl5001w_parm;
}

static int wmt_check_devices(void)
{
    int ret = 0;
    int param[7];
        char buf[96] = {0};
    int len = sizeof(buf);

    ret = wmt_getsyspara("wmt.display.fb0", buf, &len);
    if (ret) {
        pr_err("Read wmt.display.param Failed.\n");
        return -ENODEV;
    }

    ret = vpp_parse_param(buf, param, 6, 0);
    if (ret < 2)
        return -ENODEV;

    if (param[3] != LCD_GL5001W)
        return -ENODEV;

    return 0;
}

static struct i2c_board_info i2c_board_info = {
    I2C_BOARD_INFO(DRIVER_NAME, I2C_ADDR),
};

#if 0
static int tc358768_proc(char *page, char **start, off_t off,
        int count, int *eof, void *data)
{
    lcd_gl5001w_power_on();
    return 0;
}
#endif

static int __init gl5001w_init(void)
{
    int ret;
    struct i2c_client *client;
    struct i2c_adapter *adap;
    unsigned char buf[40];
    int buflen = 40;
    unsigned int value;
    char *endp;

    if(wmt_getsyspara("wmt.support.lcd.gl5001w", buf, &buflen) == 0) {
        value = simple_strtoul(buf, &endp, 0);
        if(value == 0)
            return -1;
    } else
        return -1;

    ret = gpio_request(Gl5001w_Reset_Port,"Gl5001w_Reset_Port");
    if (ret){
        printk("gpio_request Gl5001w_Reset_Port for GL5001W failed\n");
        goto err_get;
    }

    ret = gpio_request(Tc358768_Reset_Port,"Tc358768_Reset_Port");
    if (ret){
        printk("gpio_request Tc358768_Reset_Port for Tc358768 failed\n");
        goto err_get;
    }
    //ret = gpio_request(BackLight_Power_PORT,"BackLight_Power_PORT");
    //if (ret){
    //  printk("gpio_request BackLight_Power_PORT for backlight failed\n");
    //  goto err_get;
    //}

    data = kzalloc(sizeof(struct cw500_data), GFP_KERNEL);

    INIT_WORK(&data->cw500_resume_work, cw500_resume_work_func);

    ret = wmt_check_devices();
    if (ret) {
        pr_info("LCD GL5001W not found\n");
        return -ENODEV;
    }

    adap = i2c_get_adapter(I2C_ADAPTER);
    if (!adap)
        return -ENODEV;
    client = i2c_new_device(adap, &i2c_board_info);
    i2c_put_adapter(adap);
    if (!client) {
        printk("i2c_new_device error\n");
        return -ENODEV;
    }

    ret = i2c_add_driver(&tc358768_driver);
    if (ret) {
        return -EIO;
    }

    ret = lcd_panel_register(LCD_GL5001W,(void *) lcd_gl5001w_get_parm);
    if (ret) {
        i2c_del_driver(&tc358768_driver);
        return -ENODEV;
    }

    /* for debug */
    //create_proc_read_entry(DRIVER_NAME, 0666, NULL, tc358768_proc, NULL);
    return 0;
err_get:
    return -1;
}

static void __exit gl5001w_exit(void)
{
    struct i2c_client *client = tc358768_client;
    gpio_free(Gl5001w_Reset_Port);
    gpio_free(Tc358768_Reset_Port);
    //gpio_free(BackLight_Power_PORT);
    i2c_unregister_device(client);
    return i2c_del_driver(&tc358768_driver);
}

module_init(gl5001w_init);
module_exit(gl5001w_exit);

MODULE_DESCRIPTION("WonderMedia GL5001W LCD Driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("i2c:gl5001w");


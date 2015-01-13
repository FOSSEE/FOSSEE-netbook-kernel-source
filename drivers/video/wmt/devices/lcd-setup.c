/*
 * ==========================================================================
 *
 *       Filename:  lcd-setup.c
 *
 *    Description:  
 *
 *        Version:  0.01
 *        Created:  2014.6.6
 *
 *        Author:  SamMei
 *        Company:  
 *
 * ==========================================================================
 */

#include <linux/delay.h>
#include <linux/spi/spi.h>
#include <linux/i2c.h>
#include <mach/hardware.h>
#include <mach/wmt-spi.h>
#include <linux/gpio.h>
#include <mach/wmt_iomux.h>
#include "../lcd.h"

#define DRIVERNAME	"lcd-setup"

#undef pr_err
#undef pr_info
#undef pr_warning
#define pr_err(fmt, args...)		printk("[" DRIVERNAME "] " fmt, ##args)
#define pr_info(fmt, args...)		printk("[" DRIVERNAME "] " fmt, ##args)
#define pr_warning(fmt, args...)	printk("[" DRIVERNAME "] " fmt, ##args)

enum {
	LCD_SETUP_TS8224B,                      /* spi init */
	LCD_SETUP_CHUNGHWA,                     /* i2c init */
	LCD_SETUP_MAX,
};

static int lcd_setup_id = -1;
struct spi_device *g_spi_device;

static int wmt_lcd_setup_id(void)
{
	char buf[32];
	int len = sizeof(buf);
	int id;

	if (wmt_getsyspara("wmt.lcd.setup", buf, &len))
		id = -ENODEV;
	sscanf(buf, "%d", &id);
	return id;
}

static inline void spi_ctrl_9bit_tx(u8 val, int cmd_data)
{
	uint8_t buf[2];

	if (cmd_data)
		buf[0] = (val >> 1) | BIT7;
	else
		buf[0] = (val >> 1) & 0x7f;

	buf[1] = (val << 7);

	spi_write(g_spi_device, buf, sizeof(buf));
}

static inline void spi_9bit_tx(u8 val, int cmd_data)
{
	spi_ctrl_9bit_tx(val, cmd_data);
}

static inline int ts8224b_cmd(u8 cmd)
{
	spi_9bit_tx(cmd, 0);
	return 0;
}

static inline int ts8224b_data(u8 data)
{
	spi_9bit_tx(data, 1);
	return 0;
}

static int ts8224b_init(void)
{
	static uint16_t settings[] = {
		#include "ts8224b.h"
	};
	int i;

	printk(" ## %s, %d\n", __func__, __LINE__);

	for (i = 0; i < ARRAY_SIZE(settings); i += 2) {
		ts8224b_cmd(settings[i] >> 8);
		ts8224b_data(settings[i+1]);
	}

	ts8224b_cmd(0x11);
	msleep(120);

	ts8224b_cmd(0x29);
	msleep(50);
	ts8224b_cmd(0x2c);
	return 0;
}

extern void lcd_power_on(bool on);

//int lcd_spi_resume(struct spi_device *spi)
int lcd_spi_resume(void)
{
	//printk(" ## %s, %d\n", __func__, __LINE__);
	switch (lcd_setup_id) {
	case LCD_SETUP_TS8224B:
		printk(" ## %s, %d\n", __func__, __LINE__);
		lcd_power_on(1);
		mdelay(5);
		return ts8224b_init();
	default:
		return 0;
	}
}

static int __devinit lcd_spi_probe(struct spi_device *spi)
{
	g_spi_device = spi;

	//lcd_spi_resume();

	return 0;
}

static struct spi_driver lcd_spi_driver = {
	.driver = {
		.name = DRIVERNAME,
		.owner = THIS_MODULE,
	},
	.probe = lcd_spi_probe,
	//.resume = lcd_spi_resume,
};

static struct spi_board_info lcd_spi_info[] __initdata = {
	{
		.modalias	= DRIVERNAME,
		.bus_num	= 0,
		.chip_select	= 0,
		.max_speed_hz	= 12000000,
		.irq		= -1,
		.mode		= SPI_CLK_MODE1,
	},
};


// I2C

static int lcd_i2c_resume(struct i2c_client *client)
{
	static uint8_t init_data[] = {
		0x4b, 0x01,
		0x0c, 0x01,
		0x05, 0x03,
		0x41, 0x03,
		0x10, 0x06,
		0x11, 0xE0,
		0x12, 0x00,
		0x13, 0x3C,
		0x14, 0x06,
		0x15, 0x40,
		0x16, 0x03,
		0x17, 0x9E,
		0x18, 0x00,
		0x19, 0x10,
		0x1a, 0x03,
		0x1b, 0x84,
		0x1c, 0x80,
		0x1d, 0x0A,
		0x1e, 0x80,
		0x1f, 0x06,
		0x3c, 0x17,
		0x3e, 0x16,
		0x36, 0x00,
		0x31, 0x00,
		0x35, 0x41,
		0x30, 0xB0,
		0x30, 0xB1,
		0x00, 0x0B,
	};

	int i, ret;

	printk(" ## %s, %d\n", __FUNCTION__, __LINE__);
	for (i = 0; i < ARRAY_SIZE(init_data); i += 2) {
		ret = i2c_master_send(client, &init_data[i], 2);
		if (ret < 0)
			return ret;
	}
	return 0;
}

static int __devinit lcd_i2c_probe(struct i2c_client *client,
				   const struct i2c_device_id *id)
{
	printk(" ## %s, %d\n", __FUNCTION__, __LINE__);
	return 0;
}

static const struct i2c_device_id lcd_i2c_id[] = {
	{ "lcd-i2c", 0 },
	{ },
};

static struct i2c_driver lcd_i2c_driver = {
	.driver    = {
		.name  = "lcd-i2c",
		.owner = THIS_MODULE,
	},
	.probe     = lcd_i2c_probe,
//	.remove    = __devexit_p(lcd_i2c_remove),
//	.suspend   = lcd_i2c_suspend,
	.resume    = lcd_i2c_resume,
//	.shutdown  = lcd_i2c_shutdown,
	.id_table  = lcd_i2c_id,
};

static struct i2c_board_info lcd_i2c_board_info = {
	.type          = "lcd-i2c",
	.flags         = 0x00,
	.addr          = 0xe0 >> 1,
	.platform_data = NULL,
	.archdata      = NULL,
	.irq           = -1,
};

static struct i2c_client *i2c_client;
static struct i2c_adapter *i2c_adap;

static int __init lcd_setup_init(void)
{
	int ret;

	lcd_setup_id = wmt_lcd_setup_id();
	switch (lcd_setup_id) {

	// SPI
	case LCD_SETUP_TS8224B:
		ret = spi_register_board_info(lcd_spi_info, ARRAY_SIZE(lcd_spi_info));
		if (ret) {
			pr_err("spi_register_board_info failed\n");
			return ret;
		}

		ret = spi_register_driver(&lcd_spi_driver);
		if (ret) {
			pr_err("spi_register_driver failed\n");
			return ret;
		}

		pr_info("spi %s register success\n", DRIVERNAME);
		return 0;

	// I2C
	case LCD_SETUP_CHUNGHWA:
		printk(" ## %s, %d\n", __FUNCTION__, __LINE__);
		i2c_adap = i2c_get_adapter(1);
		if (!i2c_adap) {
			pr_err("Cannot get i2c adapter 1\n");
			return -ENODEV;
		}

		i2c_client = i2c_new_device(i2c_adap, &lcd_i2c_board_info);
		if (!i2c_client) {
			pr_err("Unable to add I2C device for 0x%x\n",
			       lcd_i2c_board_info.addr);
			return -ENODEV;
		}

		return i2c_add_driver(&lcd_i2c_driver);

	// INVALID
	default:
		return -EINVAL;
	}
}

static void lcd_setup_exit(void)
{
	switch (lcd_setup_id) {

	// SPI
	case LCD_SETUP_TS8224B:
		spi_unregister_driver(&lcd_spi_driver);
		break;

	// I2C
	case LCD_SETUP_CHUNGHWA:
		i2c_put_adapter(i2c_adap);
		i2c_del_driver(&lcd_i2c_driver);
		i2c_unregister_device(i2c_client);
		break;

	// INVALID
	default:
		return;
	}
}

module_init(lcd_setup_init);
module_exit(lcd_setup_exit);

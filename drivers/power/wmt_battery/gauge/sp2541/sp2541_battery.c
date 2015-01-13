/*
 * SP2541 battery driver
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <linux/module.h>
#include <linux/param.h>
#include <linux/jiffies.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/idr.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <asm/unaligned.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/power/wmt_battery.h>
#include <mach/wmt_env.h>
#include <linux/firmware.h>

#define RAM_READ_CMD		0x55
#define EEPROM_READ_CMD		0xF5
#define EEPROM_WRITE_CMD	0xFA

#define DRIVER_VERSION		"1.1.0"
#define SP2541_REG_TEMP		0x06
#define SP2541_REG_VOLT		0x08
#define SP2541_REG_AI		0x14
#define SP2541_REG_FLAGS	0x0A
#define SP2541_REG_TTE		0x16
#define SP2541_REG_TTF		0x18
#define SP2541_REG_TTECP	0x26
#define SP2541_REG_RSOC		0x0B /* Relative State-of-Charge */
#define SP2541_REG_SOC		0x2c

#define SP2541_FLAG_DSC		BIT(0)
#define SP2541_FLAG_CHGS	BIT(8)
#define SP2541_FLAG_FC		BIT(9)
#define SP2541_FLAG_OTD		BIT(14)
#define SP2541_FLAG_OTC		BIT(15)

#define SP2541_SPEED 		100 * 1000

struct battery_param {
	char rom_name[32];
	int i2c_bus;
	int interval;
};

static struct battery_param battery_param;

/* 0x07a0 ~ 0x07af
 * 0x07b0 ~ 0x07bf
 */
union rom_version {
	struct {
		uint32_t magic;
		uint32_t version;
	} v;
	uint8_t bytes[8];
};

#define __ROM_VERSION(a,b,c)	(((a) << 16) + ((b) << 8) + (c))
#define SP2514_MAGIC		__ROM_VERSION('W', 'M', 'T')

#define ROM_VERSION(ver)				\
	{						\
		.v.magic = SP2514_MAGIC,		\
		.v.version = ver,			\
	}

struct rom_entry {
	uint16_t address;
	uint16_t value;
};
#define ROM_ENTRY(a, l, v) { .address = a, .length = l, .value = v }

struct rom_struct {
	union rom_version rv;
	struct rom_entry *table;
	size_t table_size;
};

#define ROM_STRUCT(ver)				\
	{						\
		.rv = ROM_VERSION(ver),			\
	}

static struct rom_entry *rom_entry;
static struct rom_struct rom_table = ROM_STRUCT(0);

struct sp2541_struct {
	struct device		*dev;
	struct i2c_client	*client;
	int			capacity;
	struct rom_struct	*rom;

	struct power_supply	psy_bat;
	struct delayed_work	dwork;
	unsigned int interval;
};

static int parse_battery_param(void)
{
	char env[] = "wmt.battery.param";
	char buf[64];
	char *p;
	size_t l = sizeof(buf);
	int i;

	if (wmt_getsyspara(env, buf, &l))
		return -EINVAL;

	if (prefixcmp(buf, "sp2541_"))
		return -ENODEV;

	p = strchr(buf, ':');
	strncpy(battery_param.rom_name, buf + 7, p - buf - 7);
	pr_info("rom name -- %s\n", battery_param.rom_name);

	i = sscanf(p + 1, "%d:%d",
		   &battery_param.i2c_bus, &battery_param.interval);
	if (i < 2)
		return -EINVAL;

	return 0;
}

//#define SP2541_BIG_ENDIAN
#ifdef SP2541_BIG_ENDIAN
static u16 get_bigend_le16(const void *_ptr)
{
	const uint8_t *ptr = _ptr;
	return (ptr[0]<<8) | (ptr[1]);
}

static inline uint16_t __get_le16(const void *buf)
{
	return get_bigend_le16(buf);
}
#else
static inline uint16_t __get_le16(const void *buf)
{
	return get_unaligned_le16(buf);
}
#endif

static int sp2541_read(struct sp2541_struct *sp, uint8_t cmd, uint8_t reg,
		       uint8_t buf[], unsigned len)
{
	struct i2c_client *client = sp->client;
	struct i2c_msg xfer[2];
	char data[2] = { cmd, reg };
	int ret;

	xfer[0].addr = client->addr;
	xfer[0].flags = 0 | I2C_M_NOSTART;
	xfer[0].len = 2;
	xfer[0].buf = data;
	xfer[1].addr = client->addr;
	xfer[1].flags = I2C_M_RD;
	xfer[1].len = len;
	xfer[1].buf = buf;

	ret = i2c_transfer(client->adapter, xfer, ARRAY_SIZE(xfer));
	if (ret != ARRAY_SIZE(xfer)) {
		pr_err("read[r:%d] errcode[%d]\n", reg, ret);
		if (ret < 0)
			return ret;
		else
			return -EIO;
	}

	return 0;
}

static int sp2541_write(struct sp2541_struct *sp, uint8_t cmd, uint8_t reg,
			const uint8_t buf)
{
	struct i2c_client *client = sp->client;
	struct i2c_msg xfer;
	char data[3] = { cmd, reg, buf };
	int ret;

	xfer.addr = client->addr;
	xfer.flags = 0;
	xfer.len = 3;
	xfer.buf = data;

	ret = i2c_transfer(client->adapter, &xfer, 1);
	if (ret != 1) {
		pr_err("read[r:%d] errcode[%d]\n", reg, ret);
		if (ret < 0)
			return ret;
		else
			return -EIO;
	}

	return 0;
}

static int eeprom_read_byte(struct sp2541_struct *sp, uint16_t addr, uint8_t *value)
{
	uint8_t tmp;
	int i;

	if (sp2541_write(sp, EEPROM_WRITE_CMD, 0x00, (addr >> 0) & 0xff) ||
	    sp2541_write(sp, EEPROM_WRITE_CMD, 0x01, (addr >> 8) & 0xff) ||
	    sp2541_write(sp, EEPROM_WRITE_CMD, 0x03, 0x06))
		return -EIO;

	for (i = 0; i < 10; i++) {
		if (sp2541_read(sp, EEPROM_READ_CMD, 0x03, &tmp, 1))
			return -EIO;
		if (tmp == 0)
			break;
	}
	if (i == 10)
		return -EBUSY;

	if (sp2541_read(sp, EEPROM_READ_CMD, 0x02, value, 1))
		return -EIO;

	return 0;
}

static int eeprom_write_byte(struct sp2541_struct *sp, uint16_t addr, uint8_t value)
{
	uint8_t tmp;
	int i;

	if (sp2541_write(sp, EEPROM_WRITE_CMD, 0x00, (addr >> 0) & 0xff) ||
	    sp2541_write(sp, EEPROM_WRITE_CMD, 0x01, (addr >> 8) & 0xff) ||
	    sp2541_write(sp, EEPROM_WRITE_CMD, 0x02, value) ||
	    sp2541_write(sp, EEPROM_WRITE_CMD, 0x03, 0x05))
		return -EIO;

	for (i = 0; i < 10; i++) {
		if (sp2541_read(sp, EEPROM_READ_CMD, 0x03, &tmp, 1))
			return -EIO;
		if (tmp == 0)
			break;
	}
	if (i == 10)
		return -EBUSY;

	return 0;
}

static int eeprom_read(struct sp2541_struct *sp, uint16_t start,
		       uint8_t *buf, size_t len)
{
	int ret, i;
	for (i = 0; i < len; i++) {
		ret = eeprom_read_byte(sp, start + i, buf + len - i - 1);
		if (ret)
			return ret;
	}
	return 0;
}

static int eeprom_write(struct sp2541_struct *sp, uint16_t start,
			uint8_t *buf, size_t len)
{
	int ret, i, j;
	for (i = 0; i < len; i++) {
		//pr_info("wirte addr=0x%x,val=0x%x\n", start + i, buf[len - i - 1]);
		for (j = 0; j < 3; j++) {
			ret = eeprom_write_byte(sp, start + i, buf[len - i - 1]);
			if (ret)
				pr_err("eeprom write byte err #%d\n", j);
			else
				break;
		}
		if (j == 3)
			return ret;
	}
	return 0;
}

static int eeprom_update(struct sp2541_struct *sp,
			 struct rom_entry *table, size_t count)
{
	int ret, i;
	for (i = 0; i < count; i++) {
		ret = eeprom_write(sp, table[i].address,
				   (uint8_t *)&table[i].value,
				   1);//table[i].length);
		if (ret)
			return ret;
	}
	return 0;
}

static int sp2541_load_romtable(struct sp2541_struct *sp)
{
	char table_name[32];
	const struct firmware *fw_entry;
	int count, i;
	unsigned int val1, val2, version;

	sprintf(table_name, "%s.EEP", battery_param.rom_name);
	for (i = 0; i < 3; i++) {
		if(request_firmware(&fw_entry, table_name, sp->dev)!=0)
			pr_err("cat't request firmware #%d\n", i);
		else
			break;
	}
	if (i == 3)
		return -EINVAL;

	if (fw_entry->size <= 0) {
		pr_err("load firmware error\n");
		release_firmware(fw_entry);
		return -EINVAL;
	}
	count = (fw_entry->size - 13) / 13; //13 bytes per line, last line is version
	rom_entry = kzalloc(count * sizeof(*rom_entry), GFP_KERNEL);

	for (i = 0; i < count; i++) {
		sscanf(fw_entry->data + i * 13, "%x=%x", &val1, &val2);
		rom_entry[i].address = val1 & 0xFFFF;
		rom_entry[i].value = val2 & 0xFF;
		//pr_info("%d: %x = %x\n", i, rom_entry[i].address, rom_entry[i].value);
	}
	sscanf(fw_entry->data + i * 13, "version=%d", &version);
	//pr_info("version is %d\n", version);

	rom_table.rv.v.version = version;
	rom_table.table = rom_entry;
	rom_table.table_size = count;
	release_firmware(fw_entry);
	return 0;
}

static int sp2541_eeprom_check(struct sp2541_struct *sp)
{
	union rom_version version;
	int ret;

	if (sp2541_load_romtable(sp))
		return 0;
	sp->rom = &rom_table;

	ret = eeprom_read(sp, 0x7a0, (uint8_t *)&version, sizeof(version));
	if (ret)
		return ret;

	if (version.v.magic != SP2514_MAGIC ||
	    version.v.version != sp->rom->rv.v.version) {
		pr_info("old version %d\nnew version %d\n", version.v.version, 
				sp->rom->rv.v.version);
		ret = eeprom_update(sp, sp->rom->table, sp->rom->table_size);
		if (ret) {
			pr_err("eeprom_update failed\n");
			return ret;
		}

		// update version
		ret = eeprom_write(sp, 0x07a0,
				   (uint8_t *)&sp->rom->rv,
				   sizeof(sp->rom->rv));
		if (ret) {
			pr_err("sp2541 version update failed\n");
			return ret;
		}
	}

	if (rom_entry != NULL)
		kfree(rom_entry);
	rom_table.table = NULL;
	rom_table.table_size = 0;

	return 0;
}

static int sp2541_battery_temperature(struct sp2541_struct *sp)
{
	int ret;
	int temp = 0;
	uint8_t buf[2] ={0};

	ret = sp2541_read(sp, RAM_READ_CMD,SP2541_REG_TEMP,buf,2);
	if (ret<0) {
		dev_err(sp->dev, "error reading temperature\n");
		return ret;
	}

	temp = __get_le16(buf);

	temp = (temp/10) - 273;
	return temp;
}

static int sp2541_battery_voltage(struct sp2541_struct *sp)
{
	uint8_t buf[2] = {0};
	int volt = 0;
	int ret;

	ret = sp2541_read(sp, RAM_READ_CMD,SP2541_REG_VOLT,buf,2);
	if (ret<0) {
		dev_err(sp->dev, "error reading voltage\n");
		return ret;
	}

	volt = __get_le16(buf);

	return volt;
}

/*
 * Return the battery average current
 * Note that current can be negative signed as well
 * Or 0 if something fails.
 */
static int sp2541_battery_current(struct sp2541_struct *sp)
{
	int ret;
	int curr = 0;
	uint8_t buf[2] = {0};

	ret = sp2541_read(sp, RAM_READ_CMD,SP2541_REG_AI,buf,2);
	if (ret<0) {
		dev_err(sp->dev, "error reading current\n");
		return 0;
	}

	curr = __get_le16(buf);

	if (curr > 0x8000) {
		//curr = 0xFFFF^(curr-1);
		curr = curr-0x10000;
	}

	return curr;
}

/*
 * Return the battery Relative State-of-Charge
 * Or < 0 if something fails.
 */
static int sp2541_battery_rsoc(struct sp2541_struct *sp)
{
	int ret;
	int rsoc = 0;
	uint8_t buf[2];

	ret = sp2541_read(sp, RAM_READ_CMD,SP2541_REG_SOC,buf,2);
	if (ret<0) {
		dev_err(sp->dev, "error reading relative State-of-Charge\n");
		return ret;
	}

	rsoc = __get_le16(buf);

	return rsoc;
}

static int sp2541_battery_status(struct sp2541_struct *sp,
				 union power_supply_propval *val)
{
	int status = 0;
	int capacity;
#if 0
	uint8_t buf[2] = {0};
	int flags = 0;
	int ret = 0;

	ret = sp2541_read(sp, RAM_READ_CMD,SP2541_REG_FLAGS, buf, 2);
	if (ret < 0) {
		dev_err(sp->dev, "error reading flags\n");
		return ret;
	}

	flags = __get_le16(buf);
#endif

	status = charger_get_status();
	if (status < 0)
		return status;
	capacity = sp2541_battery_rsoc(sp);

	if (status == POWER_SUPPLY_STATUS_CHARGING && capacity == 100)//(flags & SP2541_FLAG_FC))
		status = POWER_SUPPLY_STATUS_FULL;

	val->intval = status;
	return 0;
}

static int sp2541_health_status(struct sp2541_struct *sp,
				union power_supply_propval *val)
{
	uint8_t buf[2] = {0};
	int flags = 0;
	int status;
	int ret;

	ret = sp2541_read(sp, RAM_READ_CMD,SP2541_REG_FLAGS, buf, 2);
	if (ret < 0) {
		dev_err(sp->dev, "error reading flags\n");
		return ret;
	}

	flags = __get_le16(buf);

	if ((flags & SP2541_FLAG_OTD)||(flags & SP2541_FLAG_OTC))
		status = POWER_SUPPLY_HEALTH_OVERHEAT;
	else
		status = POWER_SUPPLY_HEALTH_GOOD;

	val->intval = status;
	return 0;
}

static int sp2541_battery_time(struct sp2541_struct *sp, int reg,
			       union power_supply_propval *val)
{
	uint8_t buf[2] = {0};
	int tval = 0;
	int ret;

	ret = sp2541_read(sp, RAM_READ_CMD,reg,buf,2);
	if (ret < 0) {
		dev_err(sp->dev, "error reading register %02x\n", reg);
		return ret;
	}

	tval = __get_le16(buf);

	if (tval == 65535)
		return -ENODATA;

	val->intval = tval * 60;
	return 0;
}

#define to_sp2541_struct(x) container_of((x), struct sp2541_struct, psy_bat)

static int sp2541_battery_get_property(struct power_supply *psy,
				       enum power_supply_property psp,
				       union power_supply_propval *val)
{
	struct sp2541_struct *sp = to_sp2541_struct(psy);
	int ret = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		ret = sp2541_battery_status(sp, val);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = sp2541_battery_voltage(sp);
		if (psp == POWER_SUPPLY_PROP_PRESENT) {
			val->intval = val->intval <= 0 ? 0 : 1;
		}
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = sp2541_battery_current(sp);
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = sp2541_battery_rsoc(sp);
		break;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = sp2541_battery_temperature(sp);
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		ret = sp2541_health_status(sp, val);
		break;
	case POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW:
		ret = sp2541_battery_time(sp, SP2541_REG_TTE, val);
		break;
	case POWER_SUPPLY_PROP_TIME_TO_EMPTY_AVG:
		ret = sp2541_battery_time(sp, SP2541_REG_TTECP, val);
		break;
	case POWER_SUPPLY_PROP_TIME_TO_FULL_NOW:
		ret = sp2541_battery_time(sp, SP2541_REG_TTF, val);
		break;
	default:
		return -EINVAL;
	}

	return ret;
}

static enum power_supply_property sp2541_battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_HEALTH,
};

static void sp2541_external_power_changed(struct power_supply *psy)
{
	power_supply_changed(psy);
}

static void sp2541_battery_work(struct work_struct *work)
{
	struct sp2541_struct *sp =
		container_of(work, struct sp2541_struct, dwork.work);
	static int last_cap = -1;

	int curr_cap = sp2541_battery_rsoc(sp);
	if (curr_cap != last_cap) {
		power_supply_changed(&sp->psy_bat);
		last_cap = curr_cap;
	}

	schedule_delayed_work(&sp->dwork, sp->interval);
}

static int sp2541_battery_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct sp2541_struct *sp;
	int ret;

	sp = devm_kzalloc(&client->dev, sizeof(*sp), GFP_KERNEL);
	if (!sp) {
		dev_err(&client->dev, "failed to allocate device info data\n");
		return -ENOMEM;
	}

	sp->client = client;
	sp->dev = &client->dev;
	i2c_set_clientdata(client, sp);

	ret = sp2541_eeprom_check(sp);
	if (ret) {
		dev_err(sp->dev, "eeprom check failed\n");
		return ret;
	}

	sp->interval = msecs_to_jiffies(battery_param.interval);

	sp->psy_bat.name = "battery";
	sp->psy_bat.type = POWER_SUPPLY_TYPE_BATTERY;
	sp->psy_bat.properties = sp2541_battery_props;
	sp->psy_bat.num_properties = ARRAY_SIZE(sp2541_battery_props);
	sp->psy_bat.get_property = sp2541_battery_get_property;
	sp->psy_bat.external_power_changed = sp2541_external_power_changed;

	ret = power_supply_register(&client->dev, &sp->psy_bat);
	if (ret) {
		dev_err(&client->dev, "failed to register battery\n");
		return ret;
	}

	INIT_DELAYED_WORK(&sp->dwork, sp2541_battery_work);

	schedule_delayed_work(&sp->dwork, sp->interval);
	dev_info(&client->dev, "support ver. %s enabled\n", DRIVER_VERSION);
	return 0;
}

static int sp2541_battery_remove(struct i2c_client *client)
{
	struct sp2541_struct *sp = i2c_get_clientdata(client);
	cancel_delayed_work_sync(&sp->dwork);
	power_supply_unregister(&sp->psy_bat);
	return 0;
}

static int sp2541_i2c_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct sp2541_struct *sp = i2c_get_clientdata(client);
	cancel_delayed_work_sync(&sp->dwork);
	return 0;
}

static int sp2541_i2c_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct sp2541_struct *sp = i2c_get_clientdata(client);
	schedule_delayed_work(&sp->dwork, 1*HZ);
	return 0;
}

static SIMPLE_DEV_PM_OPS(sp2541_dev_pm_ops,
			 sp2541_i2c_suspend, sp2541_i2c_resume);

static const struct i2c_device_id sp2541_id[] = {
	{ "sp2541", 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, sp2541_id);

static struct i2c_driver sp2541_battery_driver = {
	.driver    = {
		.name = "sp2541",
		.owner = THIS_MODULE,
		.pm = &sp2541_dev_pm_ops,
	},
	.probe     = sp2541_battery_probe,
	.remove    = __devexit_p(sp2541_battery_remove),
	.id_table  = sp2541_id,
};

static struct i2c_board_info sp2541_i2c_info = {
	I2C_BOARD_INFO("sp2541", 0x14),
};

static struct i2c_client *i2c_client;

static int __init sp2541_battery_init(void)
{
	struct i2c_adapter *i2c_adap;
	int ret;

	if (parse_battery_param())
		return -ENODEV;

	i2c_adap = i2c_get_adapter(battery_param.i2c_bus);
	if (!i2c_adap) {
		pr_err("get i2c%d adapter failed\n", battery_param.i2c_bus);
		return -ENODEV;
	}
	i2c_client = i2c_new_device(i2c_adap, &sp2541_i2c_info);
	i2c_put_adapter(i2c_adap);
	if (!i2c_client) {
		pr_err("Unable to add I2C device for 0x%x\n", sp2541_i2c_info.addr);
		return -ENODEV;
	}

	ret = i2c_add_driver(&sp2541_battery_driver);
	if (ret) {
		pr_err("Unable to register sp2541_struct driver\n");
	}

	return ret;
}

static void __exit sp2541_battery_exit(void)
{
	i2c_del_driver(&sp2541_battery_driver);
	i2c_unregister_device(i2c_client);
}

module_init(sp2541_battery_init);
module_exit(sp2541_battery_exit);

MODULE_AUTHOR("clb");
MODULE_DESCRIPTION("sp2541_struct battery monitor driver");
MODULE_LICENSE("GPL");


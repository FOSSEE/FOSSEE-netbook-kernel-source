/*
 * adc_battery.c - WonderMedia Adc Battery Driver.
 *
 * Copyright (C) 2013  WonderMedia Technologies, Inc.  
 *
 * This program is free software; you can redistribute it and/or modify it
 * under  the terms of the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the License, or (at your
 * option) any later version.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/power_supply.h>
#include <linux/platform_device.h>
#include <linux/power/wmt_battery.h>

#define DRVNAME		"adc-batt"

#undef pr_err
#define pr_err(fmt, args...)	printk("[" DRVNAME "] " fmt, ##args)
#undef pr_info
#define pr_info(fmt, args...)	printk("[" DRVNAME "] " fmt, ##args)

enum {
	COMPENSATION_VOLUME = 0,
	COMPENSATION_BRIGHTNESS,
	COMPENSATION_WIFI,
	COMPENSATION_VIDEO,
	COMPENSATION_USB,
	COMPENSATION_HDMI,
	COMPENSATION_COUNT
};

static const char *compensation_strings[] = {
	"volume",
	"brightness",
	"wifi",
	"video",
	"usb",
	"hdmi"
};

struct adc_device_info {
	struct device 		*dev;
	struct power_supply	ps_bat;
	struct mutex		mutex;

	int compensation[COMPENSATION_COUNT];
	int capacity;
	int sleeping;
	int debug;
};

static struct adc_device_info *adc_dev_info = NULL;

static inline int adc_manual_read_volt(void)
{
	extern unsigned int ReadBattery(void);
	return ReadBattery();
}

static inline int volt_reg_to_mV(int value)
{
	// voltage = adc * (3300/128) * (1430/1000) = adc * 4719 / 128

	return ((value * 4719) / 128);
}

static int adc_bat_read_voltage(struct adc_device_info *di, int *intval)
{
	int ret;
	
	ret = adc_manual_read_volt();
	if (ret < 0)
		return ret;

	*intval = volt_reg_to_mV(ret);
	return 0;
}

static int adc_bat_read_status(struct adc_device_info *di, int *intval)
{
	int status;

	status = charger_get_status();
	if (status < 0)
		return status;

	if (status == POWER_SUPPLY_STATUS_CHARGING && di->capacity == 100)
		status = POWER_SUPPLY_STATUS_FULL;

	*intval = status;
	return 0;
}

static int adc_proc_read(char *buf, char **start, off_t offset, int len,
			    int *eof, void *data)
{
	int l = 0, i;
	int ret, status, dcin, voltage, full;
	struct adc_device_info *di = adc_dev_info;

	mutex_lock(&di->mutex);

	ret = adc_bat_read_status(di, &status);
	if (ret) {
		pr_err("adc_bat_read_status failed\n");
		return 0;
	}
	ret = adc_bat_read_voltage(di, &voltage);
	if (ret) {
		pr_err("adc_bat_read_voltage failed\n");
		return 0;
	}
	dcin = power_supply_is_system_supplied();
	full = charger_is_full();

	l += sprintf(buf + l, "status   : %d\n", status);
	l += sprintf(buf + l, "dcin     : %d\n", dcin);
	l += sprintf(buf + l, "voltage  : %d\n", voltage);
	l += sprintf(buf + l, "full     : %d\n", full);
	l += sprintf(buf + l, "sleeping : %d\n", di->sleeping);
	l += sprintf(buf + l, "debug    : %d\n", di->debug);

	for (i = 0; i < COMPENSATION_COUNT; i++) {
		l += sprintf(buf +l, "compensation %10s : %d\n",
			     compensation_strings[i], di->compensation[i]);
	}

	/* clear after read */
	di->sleeping = 0;

	mutex_unlock(&di->mutex);
	return l;
}

static int adc_proc_write(struct file *file, const char *buffer,
			unsigned long count, void *data)
{
	int bm, usage;
	struct adc_device_info *di = adc_dev_info;

	if (sscanf(buffer, "capacity=%d", &di->capacity)) {
		power_supply_changed(&di->ps_bat);
		goto out;
	}

	if (sscanf(buffer, "debug=%d", &di->debug))
		goto out;

	if (sscanf(buffer, "MODULE_CHANGE:%d-%d", &bm, &usage) < 2) {
		return 0;
	}

	if (bm < 0 || bm >= COMPENSATION_COUNT) {
		printk("bm %d error, [0, %d)\n", bm, COMPENSATION_COUNT);
		return 0;
	}

	if (usage > 100 || usage < 0) {
		printk("usage %d error\n", usage);
		return 0;
	}

	mutex_lock(&di->mutex);
	di->compensation[bm] = usage;
	mutex_unlock(&di->mutex);
out:
	return count;
}

#define BATTERY_PROC_NAME	"battery_calibration"

static void adc_proc_init(void)
{
	struct proc_dir_entry *entry;

	entry = create_proc_entry(BATTERY_PROC_NAME, 0666, NULL);
	if (entry) {
		entry->read_proc = adc_proc_read;
		entry->write_proc = adc_proc_write;
	}
}

static void adc_proc_cleanup(void)
{
	remove_proc_entry(BATTERY_PROC_NAME, NULL);
}

#define to_adc_device_info(x) container_of((x), \
				struct adc_device_info, ps_bat);

static int adc_battery_get_property(struct power_supply *psy,
				       enum power_supply_property psp,
				       union power_supply_propval *val)
{
	int ret = 0;
	struct adc_device_info *di = to_adc_device_info(psy);

	mutex_lock(&di->mutex);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		ret = adc_bat_read_status(di, &val->intval);
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = di->capacity;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		ret = adc_bat_read_voltage(di, &val->intval);
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = 1;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	mutex_unlock(&di->mutex);
	return ret;
}

static void adc_external_power_changed(struct power_supply *psy)
{
	power_supply_changed(psy);
}

static enum power_supply_property adc_battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_TECHNOLOGY,
};

static int __devinit adc_batt_probe(struct platform_device *pdev)
{
	struct adc_device_info *di;
	int ret;

	di = kzalloc(sizeof(*di), GFP_KERNEL);
	if (!di) {
		dev_err(&pdev->dev, "no memery\n");
		return -ENOMEM;
	}

	di->dev	= &pdev->dev;
	di->capacity = 50;

	mutex_init(&di->mutex);

	di->ps_bat.name = "battery";
	di->ps_bat.type = POWER_SUPPLY_TYPE_BATTERY;
	di->ps_bat.properties = adc_battery_props;
	di->ps_bat.num_properties = ARRAY_SIZE(adc_battery_props);
	di->ps_bat.get_property = adc_battery_get_property;
	di->ps_bat.external_power_changed = adc_external_power_changed;

	ret = power_supply_register(di->dev, &di->ps_bat);
	if (ret) {
		dev_err(di->dev, "failed to register battery: %d\n", ret);
		kfree(di);
		return ret;
	}

	platform_set_drvdata(pdev, di);
	adc_dev_info = di;

	adc_proc_init();

	pr_info("ADC Battery Driver Installed!\n");
	return 0;
}

static int __devexit adc_batt_remove(struct platform_device *pdev)
{
	struct adc_device_info *di = platform_get_drvdata(pdev);
	adc_proc_cleanup();
	power_supply_unregister(&di->ps_bat);
	kfree(di);
	adc_dev_info = NULL;
	pr_info("ADC Battery Driver Removed!\n");
	return 0;
}

static int adc_suspend(struct platform_device *pdev, pm_message_t state)
{
	return 0;
}

static int adc_resume(struct platform_device *pdev)
{
	struct adc_device_info *di = platform_get_drvdata(pdev);
	di->sleeping = 1;
	return 0;
}

static struct platform_driver adc_batt_driver = {
	.driver	= {
		.name = DRVNAME,
	},
	.probe		= adc_batt_probe,
	.remove		= __devexit_p(adc_batt_remove),
	.suspend	= adc_suspend,
	.resume		= adc_resume,
};

static struct platform_device *pdev;

static int __init adc_batt_init(void)
{
	pdev = platform_device_register_simple(DRVNAME, -1, NULL, 0);
	if (IS_ERR(pdev)) {
		return PTR_ERR(pdev);
	}
	return platform_driver_register(&adc_batt_driver);
}

static void __exit adc_batt_exit(void)
{
	platform_driver_unregister(&adc_batt_driver);
	platform_device_unregister(pdev);
}

module_init(adc_batt_init);
module_exit(adc_batt_exit);

MODULE_AUTHOR("WonderMedia");
MODULE_DESCRIPTION("WonderMedia Adc Battery Driver");
MODULE_LICENSE("GPL");


/*
 * mp2625_charger.c - WonderMedia Charger Driver.
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

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/power_supply.h>
#include <linux/platform_device.h>
#include <mach/wmt_env.h>
#include <mach/hardware.h>
#include <mach/wmt_iomux.h>
#include <linux/gpio.h>
#include <linux/power/wmt_battery.h>

#define DRVNAME	"mp2625-charger"

#undef pr_err
#undef pr_info
#define pr_err(fmt, args...)	printk("[" DRVNAME "] " fmt, ##args)
#define pr_info(fmt, args...)	printk("[" DRVNAME "] " fmt, ##args)

struct mp2625_charger {
	struct device *dev;
	int	ac_online;
	int	usb_online;
	int	charger_status;
	int	sleeping;

	union {
		struct {
			unsigned int cable_type:4;
			unsigned int current_sw_mode:1;
			unsigned int pc_charging:1;
		};
		uint32_t flag;
	};

	int	full_pin;
	int	full_level;
	int	current_pin;
	int	current_large_level;

	struct power_supply psy_ac;
	struct power_supply psy_usb;
	struct delayed_work dwork;
};

static struct mp2625_charger *g_charger;

static int parse_charger_param(struct mp2625_charger *ch)
{
	static char uboot_env[] = "wmt.charger.param";
	char buf[64];
	size_t l = sizeof(buf);
	int ret;

	if (wmt_getsyspara(uboot_env, buf, &l))
		return -ENODEV;
	if (prefixcmp(buf, "mp2625:"))
		return -ENODEV;
	if (!ch)
		return 0;

	ret = sscanf(buf + 7, "%x:%d:%d:%d:%d",
		    &ch->flag,
		    &ch->full_pin, &ch->full_level,
		    &ch->current_pin, &ch->current_large_level);
	if (ret < 5) {
		pr_err("Invalid uboot env: %s\n", uboot_env);
		return -EINVAL;
	}

	if (ch->cable_type != CABLE_TYPE_DC &&
	    ch->cable_type != CABLE_TYPE_USB) {
		pr_err("Invalid type %d\n", ch->cable_type);
		return -EINVAL;
	}

	if (gpio_is_valid(ch->full_pin)) {
		ret = devm_gpio_request(ch->dev, ch->full_pin, "charger full");
		if (ret) {
			pr_err("gpio%d request fail %d\n", ch->full_pin, ret);
			return ret;
		}
		wmt_gpio_setpull(ch->full_pin, (ch->full_level) ?
				 WMT_GPIO_PULL_DOWN : WMT_GPIO_PULL_UP);
		gpio_direction_input(ch->full_pin);
	}

	if (gpio_is_valid(ch->current_pin)) { 
		ret = devm_gpio_request(ch->dev, ch->current_pin, "charger current");
		if (ret) {
			pr_err("gpio%d request fail %d\n", ch->current_pin, ret);
			return ret;
		}
		gpio_direction_output(ch->current_pin, !ch->current_large_level);
	}

	pr_info("charger match " DRVNAME ", %s cable, full %d, current %d\n"
		"%s current switch, PC connected is %scharging\n",
		(ch->cable_type == CABLE_TYPE_DC) ? "DC" : "USB",
		ch->full_pin, ch->current_pin,
		(ch->current_sw_mode == CURRENT_SWITCH_DYNAMIC) ? "dynamic" : "sleep",
		(ch->pc_charging == PC_CONNECTED_NOT_CHARGING) ? "not " : "");
	return 0;
}

static inline void set_current(struct mp2625_charger *ch)
{
	int large;
	int charging = (ch->charger_status == POWER_SUPPLY_STATUS_CHARGING ||
			ch->charger_status == POWER_SUPPLY_STATUS_FULL);

	if (ch->current_sw_mode == CURRENT_SWITCH_DYNAMIC)
		large = charging;
	else
		large = ch->sleeping ? 1 : 0;

	if (ch->cable_type == CABLE_TYPE_USB && wmt_is_pc_connected())
		large = 0;

	if (gpio_is_valid(ch->current_pin)) {
		gpio_direction_output(ch->current_pin,
				      large ? ch->current_large_level :
				      !ch->current_large_level);
		printk(KERN_DEBUG "set %s current\n", large ? "large" : "small");
	}
}

static int ac_get_property(struct power_supply *psy,
				  enum power_supply_property psp,
				  union power_supply_propval *val)
{
	struct mp2625_charger *ch =
		container_of(psy, struct mp2625_charger, psy_ac);
	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = ch->ac_online;
		break;
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = ch->charger_status;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static enum power_supply_property ac_properties[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_STATUS,
};

static int usb_get_property(struct power_supply *psy,
				  enum power_supply_property psp,
				  union power_supply_propval *val)
{
	struct mp2625_charger *ch =
		container_of(psy, struct mp2625_charger, psy_usb);
	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = ch->usb_online;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static enum power_supply_property usb_properties[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static char *power_supplied_to[] = {
	"battery",
};

static int power_supply_init(struct mp2625_charger *ch)
{
	int ret;

	ch->psy_ac.name = "ac",
	ch->psy_ac.type = POWER_SUPPLY_TYPE_MAINS,
	ch->psy_ac.supplied_to = power_supplied_to,
	ch->psy_ac.num_supplicants = ARRAY_SIZE(power_supplied_to),
	ch->psy_ac.properties = ac_properties,
	ch->psy_ac.num_properties = ARRAY_SIZE(ac_properties),
	ch->psy_ac.get_property = ac_get_property,
	ret = power_supply_register(ch->dev, &ch->psy_ac);
	if (ret) {
		dev_err(ch->dev, "register ac power supply failed.\n");
		return ret;
	}

	if (ch->pc_charging == PC_CONNECTED_CHARGING) {
		ch->psy_usb.name = "usb",
		ch->psy_usb.type = POWER_SUPPLY_TYPE_USB,
		ch->psy_usb.supplied_to = power_supplied_to,
		ch->psy_usb.num_supplicants = ARRAY_SIZE(power_supplied_to),
		ch->psy_usb.properties = usb_properties,
		ch->psy_usb.num_properties = ARRAY_SIZE(usb_properties),
		ch->psy_usb.get_property = usb_get_property,
		ret = power_supply_register(ch->dev, &ch->psy_usb);
		if (ret) {
			dev_err(ch->dev, "register ac power supply failed.\n");
			return ret;
		}
	}

	return 0;
}

static void power_supply_release(struct mp2625_charger *ch)
{
	power_supply_unregister(&ch->psy_ac);
	if (ch->pc_charging == PC_CONNECTED_CHARGING)
		power_supply_unregister(&ch->psy_usb);
}

static void mp2625_charger_work(struct work_struct *work)
{
	struct mp2625_charger *ch =
		container_of(work, struct mp2625_charger, dwork.work);
	int ac_online = 0;
	int usb_online = 0;
	int charger_status = 0;

	if (wmt_is_dc_plugin()) {
		if (ch->cable_type == CABLE_TYPE_USB && wmt_is_pc_connected()) {
			if (ch->pc_charging == PC_CONNECTED_CHARGING) {
				charger_status = POWER_SUPPLY_STATUS_CHARGING;
				usb_online = 1;
			} else
				charger_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
		} else {
			charger_status = POWER_SUPPLY_STATUS_CHARGING;
			ac_online = 1;
		}

		if (charger_status == POWER_SUPPLY_STATUS_CHARGING &&
		    gpio_get_value(ch->full_pin) == ch->full_level)
			charger_status = POWER_SUPPLY_STATUS_FULL;
	} else
		charger_status = POWER_SUPPLY_STATUS_DISCHARGING;

	if (ch->ac_online != ac_online ||
	    ch->charger_status != charger_status) {
		ch->ac_online = ac_online;
		ch->charger_status = charger_status;
		power_supply_changed(&ch->psy_ac);
	}

	if (ch->pc_charging == PC_CONNECTED_CHARGING &&
	    ch->usb_online != usb_online) {
		ch->usb_online = usb_online;
		power_supply_changed(&ch->psy_usb);
	}

	set_current(ch);
	led_power_enable(charger_status == POWER_SUPPLY_STATUS_CHARGING ||
			 charger_status == POWER_SUPPLY_STATUS_FULL);
}

void mp2625_pc_connected(void)
{
	if (g_charger)
		schedule_delayed_work(&g_charger->dwork, 0);
}

static irqreturn_t dcdet_irq(int irq, void *data)
{
	struct mp2625_charger *ch = data;

	if (PMCIS_VAL & BIT27) {
		pmc_clear_intr_status(WKS_DCDET);
		schedule_delayed_work(&ch->dwork, HZ/2);
		return IRQ_HANDLED;
	}
	return IRQ_NONE;
}

static int irqs_init(struct mp2625_charger *ch)
{
	unsigned long iflag = IRQF_SHARED;
	int ret;

	if (REG32_VAL(0xfe120000) == 0x35100101)
		iflag |= IRQF_NO_SUSPEND;

	ret = devm_request_irq(ch->dev, IRQ_PMC_WAKEUP, dcdet_irq,
			      iflag, "WMT-DCDET", ch);
	if (ret < 0) {
		pr_err("register DCDET irq failed\n");
		return ret;
	}

	wmt_dcdet_irq_enable();
	return 0;
}

static void irqs_release(struct mp2625_charger *ch)
{
	wmt_dcdet_irq_disable();
}

static int mp2625_probe(struct platform_device *pdev)
{
	struct mp2625_charger *ch;
	int ret;

	ch = devm_kzalloc(&pdev->dev, sizeof(*ch), GFP_KERNEL);
	if (!ch)
		return -ENOMEM;

	ch->dev = &pdev->dev;
	platform_set_drvdata(pdev, ch);

	ret = parse_charger_param(ch);
	if (ret)
		return ret;;
	parse_charger_led();

	if ((ret = power_supply_init(ch)))
		return ret;

	INIT_DELAYED_WORK(&ch->dwork, mp2625_charger_work);

	if ((ret = irqs_init(ch))) {
		power_supply_release(ch);
		return ret;
	}

	g_charger = ch;
	schedule_delayed_work(&ch->dwork, 0);

	pr_info(DRVNAME " install success.\n");
	return 0;
}

static int __devexit mp2625_remove(struct platform_device *pdev)
{
	struct mp2625_charger *ch = platform_get_drvdata(pdev);
	irqs_release(ch);
	cancel_delayed_work_sync(&ch->dwork);
	power_supply_release(ch);
	g_charger = NULL;
	return 0;
}

static int mp2625_suspend(struct device *dev)
{
	struct mp2625_charger *ch = dev_get_drvdata(dev);
	cancel_delayed_work_sync(&ch->dwork);
	ch->sleeping = 1;
	set_current(ch);
	return 0;
}

static int mp2625_resume(struct device *dev)
{
	struct mp2625_charger *ch = dev_get_drvdata(dev);
	schedule_delayed_work(&ch->dwork, HZ/2);
	ch->sleeping = 0;
	set_current(ch);
	return 0;
}

static const struct dev_pm_ops mp2625_pm_ops = {
	.suspend = mp2625_suspend,
	.resume = mp2625_resume,
};

static struct platform_driver mp2625_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = DRVNAME,
		.pm = &mp2625_pm_ops,
	},
	.probe = mp2625_probe,
	.remove = mp2625_remove,
};

static struct platform_device *pdev;

static int __init mp2625_init(void)
{
	int ret;

	ret = parse_charger_param(NULL);
	if (ret)
		return ret;

	ret = platform_driver_register(&mp2625_driver);
	if (ret)
		return ret;

	pdev = platform_device_register_simple(DRVNAME, -1, NULL, 0);
	if (IS_ERR(pdev)) {
		ret = PTR_ERR(pdev);
		platform_driver_unregister(&mp2625_driver);
	}
	return ret;
}

static void __exit mp2625_exit(void)
{
	platform_device_unregister(pdev);
	platform_driver_unregister(&mp2625_driver);
}

module_init(mp2625_init);
module_exit(mp2625_exit);

MODULE_AUTHOR("WonderMedia");
MODULE_DESCRIPTION("MP2625 Charger Driver");
MODULE_LICENSE("GPL");


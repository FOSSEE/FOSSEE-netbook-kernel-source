/*
 * g2214_charger.c - WonderMedia Charger Driver.
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
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/power_supply.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <asm/uaccess.h>
#include <mach/hardware.h>
#include <mach/wmt_env.h>
#include <linux/power/wmt_battery.h>
#include <mach/gmt-core.h>
#include <linux/reboot.h>

#define DRVNAME	"gmt-charger"

#undef pr_err
#define pr_err(fmt, args...)	printk("[" DRVNAME "] " fmt, ##args)

#define REG_A0     0x00
#define REG_A1     0x01
#define REG_A2     0x02
#define REG_A3     0x03
#define REG_A4     0x04
#define REG_A5     0x05
#define REG_A6     0x06
#define REG_A7     0x07
#define REG_A8     0x08
#define REG_A9     0x09
#define REG_A10    0x0A
#define REG_A11    0x0B
#define REG_A12    0x0C
#define REG_A13    0x0D

struct g2214_charger {
	struct gmt2214_dev	*gmt_dev;
	struct device		*dev;
	struct mutex		lock;

	struct power_supply	psy_ac;
	struct power_supply	psy_usb;
	struct delayed_work	monitor_work;

	int		ac_online;
	int		usb_online;
	int		charger_status;
	int		sleeping;

	union {
		struct {
			unsigned int cable_type:4;
			unsigned int current_sw_mode:1;
			unsigned int pc_charging:1;
		};
		uint32_t flag;
	};

	int		iset_dcin;
	int		iset_vbus;
	int		vseta;
	int		iseta_small;
	int		iseta_large;
	int		safety_time;
	int		otg_power;
	int		pc_power_mode;
};

static struct g2214_charger *g_charger = NULL;

static int parse_charger_param(struct g2214_charger *ch)
{
	static const char uboot_env[] = "wmt.charger.param";
	char buf[64];
	size_t l = sizeof(buf);
	int n;

	if (wmt_getsyspara((char *)uboot_env, buf, &l))
		return -ENODEV;
	if (prefixcmp(buf, "g2214:"))
		return -ENODEV;
	if (!ch)
		return 0;

	n = sscanf(buf + 6, "%x:%d:%d:%d:%d:%d:%d:%d:%d",
		   &ch->flag,
		   &ch->iset_dcin, &ch->iset_vbus, &ch->vseta,
		   &ch->iseta_small, &ch->iseta_large,
		   &ch->safety_time, &ch->otg_power, &ch->pc_power_mode);
	if (n < 8) {
		pr_err("%s invalid\n", uboot_env);
		return -EINVAL;
	}

	pr_info("charger match g2214, %s cable, %s current switch\n"
		"PC connected is %scharging\n"
		"dcin %d mA, vbus %d mA, %d mV\n"
		"charging current %d~%d mA %d hour, %s otg power\n"
		"pc power in %s mode\n",
		(ch->cable_type == CABLE_TYPE_DC) ? "DC" : "USB",
		(ch->current_sw_mode == CURRENT_SWITCH_DYNAMIC) ? "dynamic" : "sleep",
		(ch->pc_charging == PC_CONNECTED_NOT_CHARGING) ? "not " : "",
		ch->iset_dcin, ch->iset_vbus, ch->vseta,
		ch->iseta_small, ch->iseta_large,
		ch->safety_time, ch->otg_power ? "switch" : "no",
		ch->pc_power_mode ? "lowpower" : "normal");
	return 0;
}

static int g2214_read(struct g2214_charger *ch, uint8_t reg)
{
	unsigned int rt_value = 0;
	gmt2214_reg_read(ch->gmt_dev, reg, &rt_value);
	return rt_value;
}

static int g2214_write(struct g2214_charger *ch, uint8_t reg, uint8_t val)
{
	return gmt2214_reg_write(ch->gmt_dev, reg, val);
}

static inline void g2214_enotg_config(struct g2214_charger *ch, int enable)
{
	int val = g2214_read(ch, REG_A8);
	if (enable)
		val |= BIT3;
	else
		val &= ~BIT3;
	g2214_write(ch, REG_A8, val);
}

static inline void g2214_vseta_config(struct g2214_charger *ch)
{
	int val, vseta;

	if (ch->vseta < 4150)
		vseta = 0;
	else if (ch->vseta < 4200)
		vseta = 1;
	else if (ch->vseta < 4350)
		vseta = 2;
	else
		vseta = 3;

	val = g2214_read(ch, REG_A8);
	val &= ~(3 << 6);
	val |= vseta << 6;
	g2214_write(ch, REG_A8, val);
}

static inline void g2214_current_config(struct g2214_charger *ch,
					int dcin_mA, int vbus_mA, int charge_mA)
{
	int iset_dcin, iset_vbus, iseta;

	if (dcin_mA <= 1000)
		iset_dcin = 0;
	else if (dcin_mA <= 1500)
		iset_dcin = 1;
	else if (dcin_mA <= 2000)
		iset_dcin = 2;
	else
		iset_dcin = 3;

	if (vbus_mA <= 95)
		iset_vbus = 0;
	else if (vbus_mA <= 475)
		iset_vbus = 1;
	else if (vbus_mA <= 950)
		iset_vbus = 2;
	else
		iset_vbus = 3;

	if (charge_mA < 300 || charge_mA > 1800)
		iseta = 2;
	else
		iseta = ((charge_mA - 300) / 100);

	g2214_write(ch, REG_A5, iset_dcin << 6 | iset_vbus << 4 | iseta);
}

static void current_refresh(struct g2214_charger *ch) 
{
	int dcin_mA, vbus_mA, charge_mA;

	dcin_mA = ch->iset_dcin;

	switch (ch->charger_status) {
	case POWER_SUPPLY_STATUS_DISCHARGING:
		vbus_mA = ch->pc_power_mode ? 95 : 475;
		charge_mA = ch->iseta_small;
		break;
	case POWER_SUPPLY_STATUS_FULL:
	case POWER_SUPPLY_STATUS_CHARGING:
		vbus_mA = ch->iset_vbus;
		if (ch->current_sw_mode == CURRENT_SWITCH_DYNAMIC) {
			charge_mA = ch->iseta_large;
		} else {
			charge_mA = ch->sleeping ? ch->iseta_large
						 : ch->iseta_small;
		}
		if (ch->cable_type == CABLE_TYPE_USB && wmt_is_pc_connected()) {
			vbus_mA = 475;
			charge_mA = ch->iseta_small;
		}
		break;
	default:
		return;
	}

	printk(KERN_DEBUG " ## %s: dcin_mA %d, vbus_mA %d, charge_mA %d\n",
		 __func__, dcin_mA, vbus_mA, charge_mA);
	g2214_current_config(ch, dcin_mA, vbus_mA, charge_mA);
	g2214_vseta_config(ch);
}

static void g2214_endpm_config(struct g2214_charger *ch, int en)
{
	int val = g2214_read(ch, REG_A0);
	if (en)
		val |= BIT3;
	else
		val &= ~BIT3;
	g2214_write(ch, REG_A0, val);
}

static void g2214_safety_time_init(struct g2214_charger *ch)
{
	int val;
	int safety_time = ch->safety_time - 1;

	if (safety_time < 0)
		safety_time = 0;
	else if (safety_time > 16)
		safety_time = 15;

	val = g2214_read(ch, REG_A6);
	val &= (~(BIT4 | BIT5 | BIT6 | BIT7));
	val |= (safety_time << 4);
	g2214_write(ch, REG_A6, val);
}

static void g2214_ntc_init(struct g2214_charger *ch)
{
	int val;
	val = g2214_read(ch, REG_A0);
	val &= ~BIT1;		//Enable Auto NTC-R Type Detection 
	g2214_write(ch, REG_A0, val);
	g2214_write(ch, REG_A7, 0);		//Set HOT boundary to 60
}

static int g2214_reg_init(struct g2214_charger *ch)
{
	g2214_safety_time_init(ch);
	g2214_enotg_config(ch, 0);
	g2214_current_config(ch, ch->iset_dcin, ch->iset_vbus, ch->iseta_small);
	g2214_endpm_config(ch, ch->current_sw_mode == CURRENT_SWITCH_DYNAMIC);
	g2214_ntc_init(ch);
	return 0;
}

static void g2214_regs_dump(struct g2214_charger *ch)
{
	int reg;
	for (reg = REG_A0; reg <= REG_A13; reg++)
		printk(KERN_DEBUG " ## reg A%d: 0x%x\n ",
		       reg, g2214_read(ch, reg));
}

static void g2214_charge_enable(struct g2214_charger *ch, int en)
{
	int val = g2214_read(ch, REG_A6);
	if (en)
		val &= ~BIT3;
	else
		val |= BIT3;
	g2214_write(ch, REG_A6, val);
}

static inline int g2214_is_full(struct g2214_charger *ch)
{
	return !!(g2214_read(ch, REG_A12) & 0x10);
}

static int g2214_read_status(struct g2214_charger *ch, int *pst)
{
	if (!wmt_is_dc_plugin()) {
		*pst = POWER_SUPPLY_STATUS_DISCHARGING;
		return 0;
	}

	switch (ch->cable_type) {
	case CABLE_TYPE_DC:
		*pst = POWER_SUPPLY_STATUS_CHARGING;
		break;
	case CABLE_TYPE_USB:
		if (wmt_is_otg_plugin()) {
			*pst = POWER_SUPPLY_STATUS_DISCHARGING;
		} else if (wmt_is_pc_connected()) {
			if (ch->pc_charging == PC_CONNECTED_CHARGING)
				*pst = POWER_SUPPLY_STATUS_CHARGING;
			else
				*pst = POWER_SUPPLY_STATUS_DISCHARGING;
		} else {
			*pst = POWER_SUPPLY_STATUS_CHARGING;
		}
		break;
	default:
		return -EINVAL;
	}

	if (*pst == POWER_SUPPLY_STATUS_CHARGING) {
		if (g2214_is_full(ch))
			*pst = POWER_SUPPLY_STATUS_FULL;
	}
	return 0;
}

static void charger_monitor_work(struct work_struct *work)
{
	struct g2214_charger *ch=
		container_of(work, struct g2214_charger, monitor_work.work);
	int ac_online = 0;
	int usb_online = 0;
	int charger_status;
	int ts_meter = 0;

	g2214_regs_dump(ch);

	if (wmt_is_otg_plugin() && ch->otg_power)
		g2214_enotg_config(ch,  1);
	else {
		g2214_enotg_config(ch,  0);
		msleep(30);
	}

	g2214_write(ch, REG_A9, 0xFF);

	g2214_read_status(ch, &charger_status);

	ts_meter = (g2214_read(ch, REG_A10) & 0xE0) >> 5;
	if (ts_meter == 0x11) {
		charger_status = POWER_SUPPLY_STATUS_DISCHARGING;
		g2214_charge_enable(ch, 0);
		printk("Battery Overheat, Charge Disable\n");
	} else if (ts_meter <= 0)
		g2214_charge_enable(ch, 1);

	if (charger_status == POWER_SUPPLY_STATUS_CHARGING ||
		charger_status == POWER_SUPPLY_STATUS_FULL) {
		if (ch->cable_type == CABLE_TYPE_USB &&
		    ch->pc_charging == PC_CONNECTED_CHARGING &&
			wmt_is_pc_connected())
			usb_online = 1;
		else
			ac_online = 1;
	}

	if (ch->charger_status != charger_status ||
	    ch->ac_online != ac_online) {
		ch->charger_status = charger_status;
		ch->ac_online = ac_online;
		power_supply_changed(&ch->psy_ac);
	}

	if (ch->pc_charging == PC_CONNECTED_CHARGING &&
	    ch->usb_online != usb_online) {
		ch->usb_online = usb_online;
		power_supply_changed(&ch->psy_usb);
	}

	current_refresh(ch);
	led_power_enable(charger_status == POWER_SUPPLY_STATUS_CHARGING ||
			 charger_status == POWER_SUPPLY_STATUS_FULL);

	g2214_write(ch, REG_A9, 0xF0);
	g2214_write(ch, REG_A12, g2214_read(ch, REG_A12) & (~BIT0));
}

void g2214_pc_connected(void)
{
	if (g_charger)
		schedule_delayed_work(&g_charger->monitor_work, 1.5*HZ);
}

static irqreturn_t otg_irq(int irq, void *data)
{
	struct g2214_charger *ch = data;

	if (REG8_VAL(USB_BASE_ADD + 0x7F1) & BIT7) {
		REG8_VAL(USB_BASE_ADD + 0x7F1) = BIT7;
		schedule_delayed_work(&ch->monitor_work, 0);
		return IRQ_HANDLED;
	}
	return IRQ_NONE;
}

// g2214 + dcdet use wakeup0 interrupt
static irqreturn_t g2214_dcdet_irq(int irq, void *data)
{
	struct g2214_charger *ch = data;

	// turn off the led immediately
	if (!wmt_is_dc_plugin())
		led_power_enable(0);

	if (PMCIS_VAL & BIT0) {
		pmc_clear_intr_status(WKS_WK0);
		schedule_delayed_work(&ch->monitor_work, 0);
		return IRQ_HANDLED;
	}

	if (PMCIS_VAL & BIT27) {
		pmc_clear_intr_status(WKS_DCDET);
		schedule_delayed_work(&ch->monitor_work, 1.5*HZ);
		return IRQ_HANDLED;
	}

	return IRQ_NONE;
}

static inline void pid_check_irq_enable(void) 
{
	REG8_VAL(USB_BASE_ADD+0x7F2) |= BIT1;
}

static inline void pid_check_irq_disable(void) 
{
	REG8_VAL(USB_BASE_ADD+0x7F2) &= ~BIT1;
}

static int irqs_init(struct g2214_charger *ch)
{
	unsigned long iflag = IRQF_SHARED;
	int ret;

	ret = devm_request_irq(ch->dev, IRQ_UHDC, otg_irq, iflag, "USBOTG", ch);
	if (ret < 0) {
		dev_err(ch->dev, "IRQ_UHDC irq request failed %d\n", ret);
		return ret;
	}

	if (REG32_VAL(0xfe120000) == 0x35100101)
		iflag |= IRQF_NO_SUSPEND;

	ret = devm_request_irq(ch->dev, IRQ_PMC_WAKEUP, g2214_dcdet_irq, iflag,
			 "G2214-DCDET", ch);
	if (ret < 0) {
		pr_err("register DCDET irq failed\n");
		return ret;
	}

	pid_check_irq_enable();
	pmc_enable_wakeup_isr(WKS_WK0, 2);
	wmt_dcdet_irq_enable();
	return 0;
}

static void irqs_release(struct g2214_charger *ch)
{
	pid_check_irq_disable();
	wmt_dcdet_irq_disable();
	pmc_disable_wakeup_isr(WKS_WK0);
}

static enum power_supply_property ac_properties[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_STATUS,
};

static int ac_get_property(struct power_supply *psy,
			   enum power_supply_property prop,
			   union power_supply_propval *val)
{
	struct g2214_charger *ch =
		container_of(psy, struct g2214_charger, psy_ac);
	switch (prop) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = ch->ac_online;
		break;
	case POWER_SUPPLY_PROP_STATUS:
		return g2214_read_status(ch, &val->intval);
	default:
		return -EINVAL;
	}
	return 0;
}

static int usb_get_property(struct power_supply *psy,
				  enum power_supply_property psp,
				  union power_supply_propval *val)
{
	struct g2214_charger *ch =
		container_of(psy, struct g2214_charger, psy_usb);
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

static int power_supply_init(struct g2214_charger *ch)
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

	return ret;
}

static void power_supply_release(struct g2214_charger *ch)
{
	power_supply_unregister(&ch->psy_ac);
}

#define G2214_PROC_NAME "driver/g2214_regs"

static int g2214_proc_show(struct seq_file *seq, void *offset)
{
	int reg;
	for (reg = REG_A0; reg <= REG_A13; reg++)
		seq_printf(seq, "reg A%d: 0x%x\n ",
			   reg, g2214_read(g_charger, reg));
	return 0;
}

static int g2214_proc_open(struct inode *inode, struct file *file)
{
	int ret;
	if (!try_module_get(THIS_MODULE))
		return -ENODEV;

	ret = single_open(file, g2214_proc_show, NULL);
	if (ret)
		module_put(THIS_MODULE);
	return ret;
}

static ssize_t g2214_proc_write(struct file *file, const char __user *buffer,
				size_t count, loff_t *pos)
{
	char cmd[32];
	unsigned long len = count;
	int reg, val;

	if (len > sizeof(cmd))
		len = sizeof(cmd);

	if (copy_from_user(cmd, buffer, len))
		return -EFAULT;
	if (sscanf(cmd, "r%d=0x%02x", &reg, &val) != 2)
		return -EINVAL;
	if (reg > REG_A13)
		return -EINVAL;

	if (g_charger) {
		g2214_write(g_charger, reg, val);
		pr_info("## %s: reg %d -> 0x%02x\n", __func__, reg, val);
	}

	return count;
}

static int g2214_proc_release(struct inode *inode, struct file *file)
{
	int res = single_release(inode, file);
	module_put(THIS_MODULE);
	return res;
}

static const struct file_operations g2214_proc_fops = {
	.open = g2214_proc_open,
	.read = seq_read,
	.write = g2214_proc_write,
	.llseek = seq_lseek,
	.release = g2214_proc_release,
};

static inline void g2214_proc_setup(void)
{
	proc_create(G2214_PROC_NAME, 0, NULL, &g2214_proc_fops);
}

static inline void g2214_proc_cleanup(void)
{
	remove_proc_entry(G2214_PROC_NAME, NULL);
}

static int g2214_reboot_notifie(struct notifier_block *nb, unsigned long event, void *unused)
{
	struct g2214_charger *ch = g_charger;
	cancel_delayed_work_sync(&ch->monitor_work);
	g2214_enotg_config(ch, 0);
	g2214_write(ch, REG_A9, 0xFF);
	g2214_write(ch, REG_A11, 0xFF);
	return NOTIFY_OK;
}

static struct notifier_block g2214_reboot_notifier = {
	.notifier_call = g2214_reboot_notifie,
};


static int __devinit g2214_probe(struct platform_device *pdev)
{
	struct g2214_charger *ch;
	int ret;

	ch = devm_kzalloc(&pdev->dev, sizeof(*ch), GFP_KERNEL);
	if (!ch)
		return -ENOMEM;

	if ((ret = parse_charger_param(ch)))
		return ret;
	parse_charger_led();

	ch->dev = &pdev->dev;
	ch->gmt_dev = dev_get_drvdata(pdev->dev.parent);
	platform_set_drvdata(pdev, ch);

	if ((ret = power_supply_init(ch)))
		return ret;

	INIT_DELAYED_WORK(&ch->monitor_work, charger_monitor_work);

	if ((ret = irqs_init(ch))) {
		power_supply_release(ch);
		return ret;
	}

	g_charger = ch;
	g2214_reg_init(ch);
	g2214_proc_setup();

	register_reboot_notifier(&g2214_reboot_notifier);
	schedule_delayed_work(&ch->monitor_work, 0);
	pr_info(DRVNAME " install success.\n");
	return 0;
}

static int __devexit g2214_remove(struct platform_device *pdev)
{
	struct g2214_charger *ch = platform_get_drvdata(pdev);
	irqs_release(ch);
	cancel_delayed_work_sync(&ch->monitor_work);
	power_supply_release(ch);
	g2214_proc_cleanup();
	g_charger = NULL;
	return 0;
}

static int g2214_suspend(struct device *dev)
{
	struct g2214_charger *ch = dev_get_drvdata(dev);
	cancel_delayed_work_sync(&ch->monitor_work);
	ch->sleeping = 1;
	current_refresh(ch);
	g2214_write(ch, REG_A11, 0xCF);
	return 0;
}

static int g2214_resume(struct device *dev)
{
	struct g2214_charger *ch = dev_get_drvdata(dev);

	// turn off the led immediately
	if (!wmt_is_dc_plugin())
		led_power_enable(0);

	pid_check_irq_enable();
	ch->sleeping = 0;
	current_refresh(ch);
	schedule_delayed_work(&ch->monitor_work, HZ);
	g2214_write(ch, REG_A11, 0xFF);
	return 0;
}

static const struct dev_pm_ops g2214_pm_ops = {
	.suspend = g2214_suspend,
	.resume	= g2214_resume,
};

static struct platform_driver g2214_driver = {
	.driver = {
		.name	= DRVNAME,
		.owner	= THIS_MODULE,
		.pm	= &g2214_pm_ops,
	},
	.probe	= g2214_probe,
	.remove	= __devexit_p(g2214_remove),
};

static int __init g2214_init(void)
{
	if (parse_charger_param(NULL))
		return -ENODEV;
	return platform_driver_register(&g2214_driver);
}

static void __exit g2214_exit(void)
{
	return platform_driver_unregister(&g2214_driver);
}

module_init(g2214_init);
module_exit(g2214_exit);

MODULE_AUTHOR("WonderMedia Technologies, Inc.");
MODULE_DESCRIPTION("GMT2144 battery charger driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("i2c:g2214");


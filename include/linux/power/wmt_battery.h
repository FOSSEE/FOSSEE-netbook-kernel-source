/*
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

#ifndef __LINUX_POWER_WMT_BATTERY_H__
#define __LINUX_POWER_WMT_BATTERY_H__

static inline int prefixcmp(const char *str, const char *prefix)
{
	for (; ; str++, prefix++)
		if (!*prefix)
			return 0;
		else if (*str != *prefix)
			return (unsigned char)*prefix - (unsigned char)*str;
}

#include <linux/power_supply.h>
#include <mach/hardware.h>

enum cable_type {
	CABLE_TYPE_DC,
	CABLE_TYPE_USB,
	CABLE_TYPE_UNKNOWN,
};

enum {
	CURRENT_SWITCH_DYNAMIC,
	CURRENT_SWITCH_SLEEP,
};

enum {
	PC_CONNECTED_NOT_CHARGING,
	PC_CONNECTED_CHARGING,
};

#define USB_BASE_ADD	(0xD8007800+WMT_MMAP_OFFSET)

static inline int wmt_is_otg_plugin(void)
{
	return !(REG8_VAL(USB_BASE_ADD + 0x7F2) & BIT0);
}

static inline int wmt_is_dc_plugin(void)
{
	return (REG8_VAL(PM_CTRL_BASE_ADDR + 0x005d) & 0x01);
}

static inline void wmt_dcdet_irq_enable(void)
{
	uint32_t val;
	// dcdet wakeup interrupt
	PMTC_VAL |= BIT27;
	val = PMWTC_VAL;
	val &= ~(0xf << 12);
	val |= 0x04 << 12;
	PMWTC_VAL = val;
	PMWE_VAL |= BIT27;

	// dcdet interrupt
	/* Register dcdet interrupt, edge trigger */
	PMCIE_VAL |= BIT27;
	WK_TRG_EN_VAL |= BIT27;
	pmc_enable_wakeup_isr(WKS_DCDET, 4);
}

static inline void wmt_dcdet_irq_disable(void)
{
	pmc_disable_wakeup_isr(WKS_DCDET);
}

static inline int charger_get_status(void)
{
	struct power_supply *charger;
	union power_supply_propval val;
	
	charger = power_supply_get_by_name("ac");
	if (!charger) {
		pr_err("get ac power supply failed\n");
		return -ENODEV;
	}

	charger->get_property(charger, POWER_SUPPLY_PROP_STATUS, &val);
	return val.intval;
}

static inline int charger_is_full(void)
{
	return charger_get_status() == POWER_SUPPLY_STATUS_FULL;
}

static inline int wmt_charger_is_dc_plugin(void)
{
	return power_supply_is_system_supplied();
}

extern int  wmt_is_pc_connected(void);
extern void wmt_do_pc_connected(void);

extern int parse_charger_led(void);
extern void led_power_enable(int enable);

#endif

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

#include <linux/gpio.h>
#include <mach/wmt_iomux.h>
#include <mach/wmt_env.h>
#include <linux/power/wmt_battery.h>

extern void mp2625_pc_connected(void);
extern void g2214_pc_connected(void);

void wmt_do_pc_connected(void)
{
	mp2625_pc_connected();
	g2214_pc_connected();
}

static struct {
	int led_power;
	int led_gpio_level;
} charger_led;

void led_power_enable(int enable)
{
	if (gpio_is_valid(charger_led.led_power)) {
		if (enable)
			gpio_direction_output(charger_led.led_power,
					      charger_led.led_gpio_level);
		else
			gpio_direction_output(charger_led.led_power,
					      !charger_led.led_gpio_level);
	}
}

int parse_charger_led(void)
{
	static const char uboot_env[] = "wmt.charger.led";
	char buf[64];
	size_t l = sizeof(buf);
	int id;

	if (wmt_getsyspara((char *)uboot_env, buf, &l) ||
	    (sscanf(buf, "%d:%d:%d", &id,
		    &charger_led.led_power, &charger_led.led_gpio_level) != 3) ||
	    id != 0 || !gpio_is_valid(charger_led.led_power) ||
	    gpio_request(charger_led.led_power, "led power")) {
		charger_led.led_power = -1;
		return -EINVAL;
	}

	led_power_enable(0);
	return 0;
}


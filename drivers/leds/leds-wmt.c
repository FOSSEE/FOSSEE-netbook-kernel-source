
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/leds.h>
#include <linux/gpio.h>
#include <linux/gpio.h>
#include <mach/wmt_iomux.h>
#include <mach/wmt_env.h>

static struct gpio_led gpio_leds[] = {
	{
		.name			= "green",
		.default_trigger	= "timer",
		.gpio			= WMT_PIN_GP62_WAKEUP3,
		.active_low		= 0,
		.default_state		= LEDS_GPIO_DEFSTATE_OFF,
	},
};

static struct gpio_led_platform_data gpio_led_info = {
	.leds		= gpio_leds,
	.num_leds	= ARRAY_SIZE(gpio_leds),
};

static struct platform_device wmt_leds = {
	.name	= "leds-gpio",
	.id	= -1,
	.dev	= {
		.platform_data	= &gpio_led_info,
	}
};

static int parse_charger_param(void)
{
	static const char uboot_env[] = "wmt.led.param";
	char buf[64];
	size_t l = sizeof(buf);
	int gpio;

	if (wmt_getsyspara((char *)uboot_env, buf, &l))
		return -ENODEV;

	sscanf(buf, "%d", &gpio);

	if (!gpio_is_valid(gpio))
		return -EINVAL;

	gpio_leds[0].gpio = gpio;
	return 0;
}

static int __init wmt_leds_init(void)
{
	if (parse_charger_param())
		return -EINVAL;

	return platform_device_register(&wmt_leds);
}

module_init(wmt_leds_init);

MODULE_LICENSE("GPL");


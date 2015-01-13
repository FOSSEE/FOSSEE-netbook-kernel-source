/*
 * ==========================================================================
 *
 *       Filename:  flash_gpio.c
 *
 *    Description:  wmt.camera.flash gpio:6
 *
 *        Version:  0.01
 *        Created:  2013年07月02日 15时00分12秒
 *
 *         Author:  smmei (), 
 *        Company:  
 *
 * ==========================================================================
 */

#include <linux/gpio.h>
#include <mach/wmt_env.h>
#include "../cmos-subdev.h"
#include "flash.h"

static int fl_gpio = -1;

static int flash_dev_set_mode(int mode)
{
	if (fl_gpio < 0)
		return -EINVAL;

	switch (mode) {
	case FLASH_MODE_OFF:
		gpio_direction_output(fl_gpio, 0);
		break;
	case FLASH_MODE_ON:
		gpio_direction_output(fl_gpio, 1);
		break;
	case FLASH_MODE_STROBE:
		gpio_direction_output(fl_gpio, 1);
		break;
	case FLASH_MODE_TORCH:
		gpio_direction_output(fl_gpio, 1);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int parse_charger_param(void)
{
	char *dev_name = "gpio";
	char *env = "wmt.camera.flash";
	char s[64];
	size_t l = sizeof(s);
	int gpio;
	int rc;

	if (wmt_getsyspara(env, s, &l)) {
		//pr_err("read %s fail!\n", env);
		return -EINVAL;
	}

	if (strncmp(s, dev_name, strlen(dev_name))) {
		return -EINVAL;
	}

	rc = sscanf(s, "gpio:%d", &gpio);
	if (rc < 1) {
		pr_err("bad uboot env: %s\n", env);
		return -EINVAL;
	}

	rc = gpio_request(gpio, "flash gpio");
	if (rc) {
		pr_err("flash gpio(%d) request failed\n", gpio);
		return rc;
	}

	fl_gpio = gpio;
	printk("flash gpio%d register success\n", fl_gpio);
	return 0;
}

static int flash_dev_init(void)
{
	return parse_charger_param();
}

static void flash_dev_exit(void)
{
	if (fl_gpio >= 0) {
		gpio_free(fl_gpio);
		fl_gpio = -1;
	}
}

struct flash_dev flash_dev_gpio = {
	.name		= "gpio",
	.init		= flash_dev_init,
	.set_mode	= flash_dev_set_mode,
	.exit		= flash_dev_exit,
};


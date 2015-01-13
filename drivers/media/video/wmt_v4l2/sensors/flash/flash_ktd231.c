
/* wmt.camera.flash <flash>:<en>:<flen>
 * wmt.camera.flash ktd231:2
 */

#include <linux/gpio.h>
#include <mach/wmt_env.h>
#include <linux/pwm.h>
#include "../cmos-subdev.h"
#include "../../wmt-vid.h"
#include "flash.h"
#include <asm/io.h>

struct ktd231_struct {
	int gpio_en;
	struct pwm_device	*pwm;
};

static struct ktd231_struct *ktd;

static void config_pwm1_pin(int enable)
{
	int val;
	if(enable) {
		val = readb(0xd8110200+WMT_MMAP_OFFSET);
		val &= ~(1 << 7);
		writeb(val, 0xd8110200+WMT_MMAP_OFFSET);
	}else{
		val = readb(0xd8110200+WMT_MMAP_OFFSET);
		val |= (1 << 7);
		writeb(val, 0xd8110200+WMT_MMAP_OFFSET);
	}
}

static void enable_pwm1_32KHz(int enable)
{
	if(enable) {
		pwm_config(ktd->pwm, 15625, 31250);// configuration output 32Khz
		pwm_enable(ktd->pwm);
		//config_pwm1_pin(0x01);
		printk("enable 32khz output\n");
	}else{
		pwm_disable(ktd->pwm);
		//config_pwm1_pin(0x00);
		printk("disable 32khz output\n");
	}
}

/*
 * TODO register flash as a subdev
 */
static int flash_dev_set_mode(int mode)
{
	if (!ktd)
		return -EINVAL;

	switch (mode) {
	case FLASH_MODE_OFF:
		gpio_direction_output(ktd->gpio_en, 0);
		enable_pwm1_32KHz(0);
		break;
	case FLASH_MODE_ON:
		gpio_direction_output(ktd->gpio_en, 1);
		enable_pwm1_32KHz(0);
		break;
	case FLASH_MODE_STROBE:
		break;
	case FLASH_MODE_TORCH:
		gpio_direction_output(ktd->gpio_en, 0);
		enable_pwm1_32KHz(1);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int parse_charger_param(void)
{
	char *dev_name = "ktd231";
	char *env = "wmt.camera.flash";
	char s[64];
	size_t l = sizeof(s);
	int gpio_en;
	int rc;
	struct pwm_device	*pwm;

	if (wmt_getsyspara(env, s, &l)) {
	//	pr_err("read %s fail!\n", env);
		return -EINVAL;
	}

	if (strncmp(s, dev_name, strlen(dev_name))) {
		return -EINVAL;
	}

	rc = sscanf(s, "ktd231:%d", &gpio_en);
	if (rc < 1) {
		pr_err("bad uboot env: %s\n", env);
		return -EINVAL;
	}

	rc = gpio_request(gpio_en, "flash ktd231 en");
	if (rc) {
		pr_err("flash en gpio(%d) request failed\n", gpio_en);
		return rc;
	}
	gpio_direction_output(gpio_en, 0);

	pwm = pwm_request(1, "pwm_flash");
	if (IS_ERR(pwm)) {
		pr_err("unable to request PWM for flash\n");
		gpio_free(gpio_en);
		return -EINVAL;
	}

	ktd = kzalloc(sizeof(*ktd), GFP_KERNEL);
	if (!ktd)
		return -ENOMEM;

	config_pwm1_pin(0x01);

	ktd->gpio_en = gpio_en;
	ktd->pwm = pwm;
	enable_pwm1_32KHz(0);
	printk("flash ktd231 register ok: en %d\n",ktd->gpio_en);

	return 0;
}

static int flash_dev_init(void)
{
	if (ktd)
		return -EINVAL;

	return parse_charger_param();
}

static void flash_dev_exit(void)
{
	if (ktd) {
		enable_pwm1_32KHz(0);
		gpio_free(ktd->gpio_en);
		pwm_free(ktd->pwm);
		kfree(ktd);
		ktd = NULL;
	}
}

struct flash_dev flash_dev_ktd231 = {
	.name		= "ktd231",
	.init		= flash_dev_init,
	.set_mode	= flash_dev_set_mode,
	.exit		= flash_dev_exit,
};


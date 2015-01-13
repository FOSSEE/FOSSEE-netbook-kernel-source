
/* wmt.camera.flash <flash>:<en>:<flen>
 * wmt.camera.flash eup3618:14:15
 */

#include <linux/gpio.h>
#include <mach/wmt_env.h>
#include "../cmos-subdev.h"
#include "../../wmt-vid.h"
#include "flash.h"

struct eup3618_struct {
	int gpio_en;
	int gpio_flen;
};

static struct eup3618_struct *eup;

/*
 * TODO register flash as a subdev
 */
static int flash_dev_set_mode(int mode)
{
	if (!eup)
		return -EINVAL;

	switch (mode) {
	case FLASH_MODE_OFF:
		gpio_direction_output(eup->gpio_en, 0);
		gpio_direction_output(eup->gpio_flen, 0);
		break;
	case FLASH_MODE_ON:
		gpio_direction_output(eup->gpio_en, 1);
		gpio_direction_output(eup->gpio_flen, 1);
		break;
	case FLASH_MODE_STROBE:
		gpio_direction_output(eup->gpio_en, 0);
		gpio_direction_output(eup->gpio_flen, 0);
		break;
	case FLASH_MODE_TORCH:
		gpio_direction_output(eup->gpio_en, 1);
		gpio_direction_output(eup->gpio_flen, 1);
		msleep(1);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int parse_charger_param(void)
{
	char *dev_name = "eup3618";
	char *env = "wmt.camera.flash";
	char s[64];
	size_t l = sizeof(s);
	int gpio_en, gpio_flen;
	int rc;
	
	if (wmt_getsyspara(env, s, &l)) {
	//	pr_err("read %s fail!\n", env);
		return -EINVAL;
	}

	if (strncmp(s, dev_name, strlen(dev_name))) {
		return -EINVAL;
	}

	rc = sscanf(s, "eup3618:%d:%d", &gpio_en, &gpio_flen);
	if (rc < 2) {
		pr_err("bad uboot env: %s\n", env);
		return -EINVAL;
	}

	rc = gpio_request(gpio_en, "flash eup3618 en");
	if (rc) {
		pr_err("flash en gpio(%d) request failed\n", gpio_en);
		return rc;
	}

	rc = gpio_request(gpio_flen, "flash eup3618 flen");
	if (rc) {
		pr_err("flash flen gpio(%d) request failed\n", gpio_flen);
		gpio_free(gpio_en);
		return rc;
	}

	eup = kzalloc(sizeof(*eup), GFP_KERNEL);
	if (!eup)
		return -ENOMEM;

	eup->gpio_en = gpio_en;
	eup->gpio_flen = gpio_flen;

	printk("flash eup3618 register ok: en %d, flen %d\n",
	       eup->gpio_en, eup->gpio_flen);
	return 0;
}

static int flash_dev_init(void)
{
	if (eup)
		return -EINVAL;

	return parse_charger_param();
}

static void flash_dev_exit(void)
{
	if (eup) {
		gpio_free(eup->gpio_en);
		gpio_free(eup->gpio_flen);
		kfree(eup);
		eup = NULL;
	}
}

struct flash_dev flash_dev_eup3618 = {
	.name		= "eup3618",
	.init		= flash_dev_init,
	.set_mode	= flash_dev_set_mode,
	.exit		= flash_dev_exit,
};


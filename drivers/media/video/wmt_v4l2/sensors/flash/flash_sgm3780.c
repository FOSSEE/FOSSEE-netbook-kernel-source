
/* wmt.camera.flash <flash>:<en>:<flen>:<torch_key>
 * wmt.camera.flash sgm3780:3:2:12
 */

#include <linux/gpio.h>
#include <mach/wmt_iomux.h>
#include <linux/irq.h>
#include <mach/wmt_env.h>
#include "../cmos-subdev.h"
#include "../../wmt-vid.h"
#include "flash.h"

enum key_status {
	Idle,Pending,Pressure
};

struct key_struct{
	int gpio;
	enum key_status sts;
	bool need_on;
	struct work_struct work;
};

struct sgm3780_struct {
	int gpio_en;
	int gpio_flen;
	struct key_struct torch_key; 
};

static struct sgm3780_struct *eup;

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
		gpio_direction_output(eup->gpio_flen, 0);
		msleep(1);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static void flash_key_scan_work(struct work_struct *work)
{
	struct sgm3780_struct *peup = container_of(work, struct sgm3780_struct,torch_key.work);

	if(peup->torch_key.sts !=Idle){
		pr_err("torch key is busy!!!\n");
		goto work_done;
	}
	peup->torch_key.sts = Pending;
	msleep(50);
	if(peup->torch_key.sts == Pending){
		if(!gpio_get_value(peup->torch_key.gpio)){
			peup->torch_key.sts = Pressure;
		}	
	}

	if(peup->torch_key.sts == Pressure){
		if(peup->torch_key.need_on == true){
			flash_dev_set_mode(FLASH_MODE_TORCH);
			peup->torch_key.need_on = false;
		}else{
			flash_dev_set_mode(FLASH_MODE_OFF);
			peup->torch_key.need_on = true;
		}
	}
	
work_done:
	peup->torch_key.sts = Idle;
	wmt_gpio_unmask_irq(peup->torch_key.gpio);
}

static irqreturn_t flash_key_ext_irq (int irq, void *dev_id)
{
	struct sgm3780_struct *peup = dev_id;

	if(!is_gpio_irqenable(peup->torch_key.gpio) || !gpio_irqstatus(peup->torch_key.gpio))
		return IRQ_NONE;

	wmt_gpio_ack_irq(peup->torch_key.gpio);
	wmt_gpio_mask_irq(peup->torch_key.gpio);
	schedule_work(&peup->torch_key.work);
	
	return IRQ_HANDLED;
}

static void key_setup_extirq(struct sgm3780_struct *peup)
{
	int ret;
	
	wmt_gpio_setpull(peup->torch_key.gpio,WMT_GPIO_PULL_UP);
	wmt_gpio_set_irq_type(peup->torch_key.gpio,IRQ_TYPE_EDGE_FALLING);
	wmt_gpio_mask_irq(peup->torch_key.gpio);
	wmt_gpio_ack_irq(peup->torch_key.gpio);
	ret = request_irq(IRQ_GPIO, flash_key_ext_irq, IRQF_SHARED,"sgm3780_key_irq", peup);
	if (ret) {
        	printk("request irq type IRQ_TYPE_EDGE_FALLING fail!\n");
		return;
	 }
       wmt_gpio_ack_irq(peup->torch_key.gpio);
	wmt_gpio_unmask_irq(peup->torch_key.gpio);
}

static int parse_charger_param(void)
{
	char *dev_name = "sgm3780";
	char *env = "wmt.camera.flash";
	char s[64];
	size_t l = sizeof(s);
	int gpio_en, gpio_flen,gpio_key;
	int rc;
	
	if (wmt_getsyspara(env, s, &l)) {
	//	pr_err("read %s fail!\n", env);
		return -EINVAL;
	}

	if (strncmp(s, dev_name, strlen(dev_name))) {
		return -EINVAL;
	}

	rc = sscanf(s, "sgm3780:%d:%d:%d", &gpio_en, &gpio_flen,&gpio_key);
	if (rc < 2) {
		pr_err("bad uboot env: %s\n", env);
		return -EINVAL;
	}

	rc = gpio_request(gpio_en, "flash sgm3780 en");
	if (rc) {
		pr_err("flash en gpio(%d) request failed\n", gpio_en);
		return rc;
	}

	rc = gpio_request(gpio_flen, "flash sgm3780 flen");
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
	eup->torch_key.gpio = gpio_key;
	eup->torch_key.need_on = true;
	INIT_WORK(&eup->torch_key.work,flash_key_scan_work);
	key_setup_extirq(eup);

	printk("flash sgm3780 register ok: en %d, flen %d, key %d\n",
	       eup->gpio_en, eup->gpio_flen,eup->torch_key.gpio);
	
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
		cancel_work_sync(&eup->torch_key.work);
		free_irq(IRQ_GPIO,eup);
		kfree(eup);
		eup = NULL;
	}
}

struct flash_dev flash_dev_sgm3780 = {
	.name		= "sgm3780",
	.init		= flash_dev_init,
	.set_mode	= flash_dev_set_mode,
	.exit		= flash_dev_exit,
};


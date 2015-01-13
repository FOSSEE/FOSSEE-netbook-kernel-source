/*
 * Copyright (c) 2013 Espressif System.
 *
 *  sdio stub code for customer
 */

#include <linux/gpio.h>
#include <mach/wmt_iomux.h>

#define SDIO_ID 2

extern void wmt_detect_sdio2(void);
extern void force_remove_sdio2(void);


void sif_platform_rescan_card(unsigned insert)
{

	printk("%s id %d %u\n", __func__, SDIO_ID, insert);
	if (insert)
	{
		wmt_detect_sdio2();
	}
	else
	{
		force_remove_sdio2();
	}
}

void sif_platform_reset_target(void)
{
	int err;

	err = gpio_request(WMT_PIN_GP62_SUSGPIO1, "wifi_chip_en");
        if(err < 0) {
                printk("reques gpio:%x failed!!! for wifi\n",WMT_PIN_GP62_SUSGPIO1);
        }else{
                printk("request gpio:%d for wifi success!!!\n",WMT_PIN_GP62_SUSGPIO1);
        }

	gpio_direction_output(WMT_PIN_GP62_SUSGPIO1, 0);
	msleep(100);
	gpio_direction_output(WMT_PIN_GP62_SUSGPIO1, 1);
	mdelay(100);
}

void sif_platform_target_poweroff(void)
{
	gpio_direction_output(WMT_PIN_GP62_SUSGPIO1, 0);
	mdelay(100);
	printk("eagle wifi module power off!\n");

	gpio_free(WMT_PIN_GP62_SUSGPIO1);
}

void sif_platform_target_poweron(void)
{
	int err;

	err = gpio_request(WMT_PIN_GP62_SUSGPIO1, "wifi_chip_en");
        if(err < 0) {
                printk("reques gpio:%x failed!!! for wifi\n",WMT_PIN_GP62_SUSGPIO1);
        }else{
                printk("request gpio:%d for wifi success!!!\n",WMT_PIN_GP62_SUSGPIO1);
        }

	gpio_direction_output(WMT_PIN_GP62_SUSGPIO1, 0);
	msleep(200);
	gpio_direction_output(WMT_PIN_GP62_SUSGPIO1, 1);
	mdelay(100);
	printk("eagle wifi module power on!\n");
}

void sif_platform_target_speed(int high_speed)
{
}

void sif_platform_check_r1_ready(struct esp_pub *epub)
{
}

#ifdef ESP_ACK_INTERRUPT
extern void eagle_ack_interrupt(void);
void sif_platform_ack_interrupt(struct esp_pub *epub)
{
        eagle_ack_interrupt();
}
#endif //ESP_ACK_INTERRUPT

module_init(esp_sdio_init);
module_exit(esp_sdio_exit);

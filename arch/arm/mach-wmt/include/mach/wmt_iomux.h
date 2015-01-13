
#ifndef __MACH_WMT_IOMUX_H__
#define __MACH_WMT_IOMUX_H__

#include <linux/types.h>

#undef	WMT_PIN
#define WMT_PIN(__gp, __bit, __irq, __name)	__name,
enum iomux_pins {
	#include "iomux.h"
};

/* use gpiolib dispatchers */
#define gpio_get_value		__gpio_get_value
#define gpio_set_value		__gpio_set_value
#define gpio_cansleep		__gpio_cansleep

enum wmt_gpio_pulltype {
	WMT_GPIO_PULL_NONE = 0,
	WMT_GPIO_PULL_UP,
	WMT_GPIO_PULL_DOWN,
};
extern int wmt_gpio_setpull(unsigned int gpio, enum wmt_gpio_pulltype pull);
extern int wmt_gpio_getpull(unsigned int gpio);
extern const char *wmt_gpio_name(int gpio);

/* below for gpio irq */

extern void wmt_gpio_ack_irq(unsigned int gpio);
extern void wmt_gpio_mask_irq(unsigned int gpio);
extern void wmt_gpio_unmask_irq(unsigned int gpio);
extern int is_gpio_irqenable(u32 irqindex);
extern int gpio_irqstatus(unsigned int gpio);
/*
 * current support type: (in <linux/irq.h>)
 *	IRQ_TYPE_EDGE_RISING
 *	IRQ_TYPE_EDGE_FALLING
 *	IRQ_TYPE_EDGE_BOTH
 *	IRQ_TYPE_LEVEL_LOW
 *	IRQ_TYPE_LEVEL_HIGH
 */
extern int wmt_gpio_set_irq_type(unsigned int gpio, u32 type);

#endif /* #ifndef __MACH_WMT_IOMUX_H__ */


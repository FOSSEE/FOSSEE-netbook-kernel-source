/*
 * ==========================================================================
 *
 *       Filename:  flash.c
 *
 *    Description:  
 *
 *        Version:  0.01
 *        Created:  2013年07月02日 14时16分08秒
 *
 *         Author:  smmei (), 
 *        Company:  
 *
 * ==========================================================================
 */

#include <linux/init.h>
#include <linux/errno.h>
#include "flash.h"

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif

static struct flash_dev *fl_devices[] = {
	&flash_dev_gpio,
	&flash_dev_eup2471,
	&flash_dev_eup3618,
	&flash_dev_ktd231,
	&flash_dev_sgm3780,
};

struct flash_dev *flash_instantiation(void)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(fl_devices); i++) {
		struct flash_dev *fl = fl_devices[i];
		if (fl->init && fl->init() == 0)
			return fl;
	}
	return NULL;
}

int flash_set_mode(struct flash_dev *fl, int mode)
{
	return (fl && fl->set_mode) ? fl->set_mode (mode) : -EINVAL;
}

void flash_destroy(struct flash_dev *fl)
{
	if (fl && fl->exit)
		fl->exit();
}


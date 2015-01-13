/*
 * Copyright (C) 2010 Trusted Logic S.A.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/jiffies.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/miscdevice.h>
#include <linux/spinlock.h>
#include <linux/nfc/pn544.h>

#include <mach/wmt_iomux.h>

#include <mach/hardware.h>
//#include <plat/gpio-core.h>
//#include <plat/gpio-cfg.h>
//#include <plat/gpio-cfg-helpers.h>


#undef pr_err
#define pr_err printk
//#define pr_debug printk
//#define pr_warning printk

#define DRIVER_DESC	"NFC driver for PN544"

#define CLIENT_ADDR 0x28 //0x2b mod 2014-7-10
#define WMT_PN544_I2C_CHANNEL 0

struct i2c_client *pn544_client;
static int g_chip_nr = 7;

#define MAX_BUFFER_SIZE	512

extern  int wmt_getsyspara(char *varname, unsigned char *varval, int *varlen);

struct pn544_dev	{
	wait_queue_head_t	read_wq;
	struct mutex		read_mutex;
	struct i2c_client	*client;
	struct miscdevice	pn544_device;
	
	
	int 		ven_gpio;
	int 		ven_active;
	int 		ven_on_off;
	
	int 		firm_gpio;
	int 		firm_active;
	
	int		irq_gpio;
	int 		irq_active;
	
	
	bool			irq_enabled;
	spinlock_t		irq_enabled_lock;
};

//**********************add 2014-7-16
static int irq_gpio = 0;

void wmt_clr_int(void)
{
	int num = irq_gpio; 
	
	int reg_shift = 20;
	if (num > /*11*/42)
	{
		return;
	}
	
	if (num < 20)
	{
		REG32_VAL(__GPIO_BASE+0x0360) = 1<<num;
	}
	else
	{
		switch (num)
		{
			case WMT_PIN_GP5_VDOUT11:
				REG32_VAL(__GPIO_BASE+0x0360) = 1<<reg_shift;
				break;
			case WMT_PIN_GP5_VDOUT12:
				REG32_VAL(__GPIO_BASE+0x0360) = 1<<(reg_shift+1);
				break;
			case WMT_PIN_GP6_VDOUT18:
				REG32_VAL(__GPIO_BASE+0x0360) = 1<<(reg_shift+2);
				break;
			case WMT_PIN_GP6_VDOUT19:
				REG32_VAL(__GPIO_BASE+0x0360) = 1<<(reg_shift+3);
				break;
			case WMT_PIN_GP6_VDOUT20:
				REG32_VAL(__GPIO_BASE+0x0360) = 1<<(reg_shift+4);
				break;
			case WMT_PIN_GP6_VDOUT21:
				REG32_VAL(__GPIO_BASE+0x0360) = 1<<(reg_shift+5);
				break;
			
		}
	}	
	//gpio 0-19 vdout11 12 18:21
	//REG32_VAL(__GPIO_BASE+0x0360) = 1<<num; //interrupt status register ,1:active 0:inactive
					// write 1:to clear 
}

void wmt_set_irqinput(void)
{
	int num = irq_gpio;
	
	if (num > /*19*/42)
		return;
	if (num < 20)
	{	
		REG32_VAL(__GPIO_BASE+0x0040) |= (1<<num); //enable gpio
		REG32_VAL(__GPIO_BASE+0x0080) &= ~(1<<num); //set input
	}
	
	//GPIO GP5 Enable Register for VDOUT[15:8] 
	//GPIO GP6 Enable Register for VDOUT[23:16] 

	switch (num) //gpio 0-19 vdout11 12 18:21 -->gpio 31 32  38:41
	{
		case WMT_PIN_GP5_VDOUT11:
			REG32_VAL(__GPIO_BASE+0x0044) |= (1<<(num-20)); //enable gpio
			REG32_VAL(__GPIO_BASE+0x0084) &= ~(1<<(num-20)); //set input	
			break;
		case WMT_PIN_GP5_VDOUT12:
			REG32_VAL(__GPIO_BASE+0x0044) |= (1<<(num-20)); //enable gpio
			REG32_VAL(__GPIO_BASE+0x0084) &= ~(1<<(num-20)); //set input		
			break;
		case WMT_PIN_GP6_VDOUT18:
			REG32_VAL(__GPIO_BASE+0x0044) |= (1<<(num-20)); //enable gpio
			REG32_VAL(__GPIO_BASE+0x0084) &= ~(1<<(num-20)); //set input		
			break;
		case WMT_PIN_GP6_VDOUT19:
			REG32_VAL(__GPIO_BASE+0x0044) |= (1<<(num-20)); //enable gpio
			REG32_VAL(__GPIO_BASE+0x0084) &= ~(1<<(num-20)); //set input		
			break;
		case WMT_PIN_GP6_VDOUT20:
			REG32_VAL(__GPIO_BASE+0x0044) |= (1<<(num-20)); //enable gpio
			REG32_VAL(__GPIO_BASE+0x0084) &= ~(1<<(num-20)); //set input		
			break;
		case WMT_PIN_GP6_VDOUT21:
			REG32_VAL(__GPIO_BASE+0x0044) |= (1<<(num-20)); //enable gpio
			REG32_VAL(__GPIO_BASE+0x0084) &= ~(1<<(num-20)); //set input		
			break;
		
	}	
}

static void wmt_disable_irqinput(void)
{
	int num = irq_gpio;
	
	if (num > /*19*/42)
		return;
	if (num < 20)
	{	
		REG32_VAL(__GPIO_BASE+0x0040) &= ~(1<<num); //enable gpio
		REG32_VAL(__GPIO_BASE+0x0080) &= ~(1<<num); //set input
	}
	
	//GPIO GP5 Enable Register for VDOUT[15:8] 
	//GPIO GP6 Enable Register for VDOUT[23:16] 

	switch (num) //gpio 0-19 vdout11 12 18:21 -->gpio 31 32  38:41
	{
		case WMT_PIN_GP5_VDOUT11:
			REG32_VAL(__GPIO_BASE+0x0044) &= ~(1<<(num-20)); //disable gpio
			REG32_VAL(__GPIO_BASE+0x0084) &= ~(1<<(num-20)); //set input	
			break;
		case WMT_PIN_GP5_VDOUT12:
			REG32_VAL(__GPIO_BASE+0x0044) &= ~(1<<(num-20)); //enable gpio
			REG32_VAL(__GPIO_BASE+0x0084) &= ~(1<<(num-20)); //set input		
			break;
		case WMT_PIN_GP6_VDOUT18:
			REG32_VAL(__GPIO_BASE+0x0044) &= ~(1<<(num-20)); //enable gpio
			REG32_VAL(__GPIO_BASE+0x0084) &= ~(1<<(num-20)); //set input		
			break;
		case WMT_PIN_GP6_VDOUT19:
			REG32_VAL(__GPIO_BASE+0x0044) &= ~(1<<(num-20)); //enable gpio
			REG32_VAL(__GPIO_BASE+0x0084) &= ~(1<<(num-20)); //set input		
			break;
		case WMT_PIN_GP6_VDOUT20:
			REG32_VAL(__GPIO_BASE+0x0044) &= ~(1<<(num-20)); //enable gpio
			REG32_VAL(__GPIO_BASE+0x0084) &= ~(1<<(num-20)); //set input		
			break;
		case WMT_PIN_GP6_VDOUT21:
			REG32_VAL(__GPIO_BASE+0x0044) &= ~(1<<(num-20)); //enable gpio
			REG32_VAL(__GPIO_BASE+0x0084) &= ~(1<<(num-20)); //set input		
			break;
		
	}		
}

static void wmt_pullup(void)
{
	int num = irq_gpio;
	
	if(num >/*11*//*19*/42)
		return;
	if (num < 20)
	{
		REG32_VAL(__GPIO_BASE+0x04c0) |= (1<<num); //pull  up!
		REG32_VAL(__GPIO_BASE+0x0480) |= (1<<num); //enable pull up/down
	}
	switch (num)
	{	//vdout11 12 18:21-->gpio 31 32  38:41
		case WMT_PIN_GP5_VDOUT11:
			REG32_VAL(__GPIO_BASE+0x04c4) |= (1<<(num-20)); //pull  up!
			REG32_VAL(__GPIO_BASE+0x0484) |= (1<<(num-20)); //enable pull up/down
			break;
		case WMT_PIN_GP5_VDOUT12:
			REG32_VAL(__GPIO_BASE+0x04c4) |= (1<<(num-20)); //pull  up!
			REG32_VAL(__GPIO_BASE+0x0484) |= (1<<(num-20)); //enable pull up/down
			break;
		case WMT_PIN_GP6_VDOUT18:
			REG32_VAL(__GPIO_BASE+0x04c4) |= (1<<(num-20)); //pull  up!
			REG32_VAL(__GPIO_BASE+0x0484) |= (1<<(num-20)); //enable pull up/down	
			break;
		case WMT_PIN_GP6_VDOUT19:
			REG32_VAL(__GPIO_BASE+0x04c4) |= (1<<(num-20)); //pull  up!
			REG32_VAL(__GPIO_BASE+0x0484) |= (1<<(num-20)); //enable pull up/down	
			break;
		case WMT_PIN_GP6_VDOUT20:
			REG32_VAL(__GPIO_BASE+0x04c4) |= (1<<(num-20)); //pull  up!
			REG32_VAL(__GPIO_BASE+0x0484) |= (1<<(num-20)); //enable pull up/down	
			break;
		case WMT_PIN_GP6_VDOUT21:
			REG32_VAL(__GPIO_BASE+0x04c4) |= (1<<(num-20)); //pull  up!
			REG32_VAL(__GPIO_BASE+0x0484) |= (1<<(num-20)); //enable pull up/down
			break;
		
	}	
}

int wmt_set_gpirq(int type) 
{
	int shift;
	int offset;
	unsigned long reg;
	int num = irq_gpio;
	
	if(num >/*11*//*19*/42)
		return -1;
	//if (num > 9)
		//GPIO_PIN_SHARING_SEL_4BYTE_VAL &= ~BIT4; // gpio10,11 as gpio
#if 0		
	REG32_VAL(__GPIO_BASE+0x0040) &= ~(1<<num); // gpio disable
	REG32_VAL(__GPIO_BASE+0x0080) &= ~(1<<num); //set input
#endif	
	wmt_disable_irqinput();//replace 2014-8-22
#if 0	
	REG32_VAL(__GPIO_BASE+0x04c0) |= (1<<num); //pull  up!
	REG32_VAL(__GPIO_BASE+0x0480) |= (1<<num); //enable pull up/down
#endif	
	wmt_pullup();//replace 2014-8-22
	
	//set gpio irq triger type
	if(num < 4){//[0,3]
		shift = num;
		offset = 0x0300;
	}else if(num >= 4 && num < 8){//[4,7]
		shift = num-4;
		offset = 0x0304;
	}else if (num >= 8 && num <12){// [8,11]
		shift = num-8;
		offset = 0x0308;
	}
	else if (num>=12 && num < 16)
	{
		shift = num -12;
		offset = 0x030c;
	}
	else if (num>=16 && num <20)
	{
		shift = num -16;
		offset = 0x0310;
	}
	else ////vdout11 12 18:21-->gpio 31 32  38:41
	{	// 0x0314----0x0319
		switch (num)
		{
			case WMT_PIN_GP5_VDOUT11:
				shift = 0;
				offset = 0x0314;
				break;
			case WMT_PIN_GP5_VDOUT12:
				shift = 1;
				offset = 0x0314;
				break;
			
			case WMT_PIN_GP6_VDOUT18:
				shift = 2;
				offset = 0x0314;
				break;
				
			case WMT_PIN_GP6_VDOUT19:
				shift = 3;
				offset = 0x0314;
				break;
			case WMT_PIN_GP6_VDOUT20:
				shift = 0;
				offset = 0x0318;
				break;
			
			case WMT_PIN_GP6_VDOUT21:
			
				shift = 1;
				offset = 0x0318;
				break;
		}
	#if 0
		else if (num>=20 && num<24) //videoOut11 12  18 19
		{
		shift = num -20;
		offset = 0x0314;
		}
		else if (num>=24 && num<28) //videoOut20 21
		{
		shift = num -24;
		offset = 0x0318;
		}
	#endif	
	}
	
	reg = REG32_VAL(__GPIO_BASE + offset);

	switch(type){
		case IRQ_TYPE_LEVEL_LOW:
			reg &= ~(1<<(shift*8+2)); 
			reg &= ~(1<<(shift*8+1));
			reg &= ~(1<<(shift*8));
			break;
		case IRQ_TYPE_LEVEL_HIGH:
			reg &= ~(1<<(shift*8+2)); 
			reg &= ~(1<<(shift*8+1));
			reg |= (1<<(shift*8));
			break;
		case IRQ_TYPE_EDGE_FALLING:
			reg &= ~(1<<(shift*8+2)); 
			reg |= (1<<(shift*8+1));
			reg &= ~(1<<(shift*8));
			break;
		case IRQ_TYPE_EDGE_RISING:
			reg &= ~(1<<(shift*8+2)); 
			reg |= (1<<(shift*8+1));
			reg |= (1<<(shift*8));
			break;
		default://both edge
			reg |= (1<<(shift*8+2)); 
			reg &= ~(1<<(shift*8+1));
			reg &= ~(1<<(shift*8));
			break;
			
	}
	//reg |= 1<<(shift*8+7);//enable interrupt
	reg &= ~(1<<(shift*8+7)); //disable int

	REG32_VAL(__GPIO_BASE + offset) = reg; 
	wmt_clr_int(); //replace 2014-8-22
	//REG32_VAL(__GPIO_BASE+0x0360) = 1<<num; //clear interrupt status// 1 bit per int
	msleep(5);
	return 0;
}

int wmt_enable_gpirq(void)
{
	int num = irq_gpio;
	
	if(num > /*11*/ /*19*/42)
		return -1;

	if(num<4)
		REG32_VAL(__GPIO_BASE+0x0300) |= 1<<(num*8+7); //enable interrupt 
	else if(num >= 4 && num < 8)
		REG32_VAL(__GPIO_BASE+0x0304) |= 1<<((num-4)*8+7); //enable interrupt 
	else if (num >= 8 && num < 12)
		REG32_VAL(__GPIO_BASE+0x0308) |= 1<<((num-8)*8+7); //enable interrupt
	else if (num >= 12 && num < 16)
		REG32_VAL(__GPIO_BASE+0x030c) |= 1<<((num-12)*8+7); //enable interrupt 
	else if (num >= 16 && num < 20)
		REG32_VAL(__GPIO_BASE+0x0310) |= 1<<((num-16)*8+7); //enable interrupt 
	else ////vdout11 12 18:21-->gpio 31 32  38:41
	{	// 0x0314----0x0319
		switch (num)
		{
			case WMT_PIN_GP5_VDOUT11:
				REG32_VAL(__GPIO_BASE+0x0314) |= 1<<(0*8+7); //enable interrupt 
				
				break;
			case WMT_PIN_GP5_VDOUT12:
				REG32_VAL(__GPIO_BASE+0x0314) |= 1<<(1*8+7); //enable interrupt 
				break;
			
			case WMT_PIN_GP6_VDOUT18:
				REG32_VAL(__GPIO_BASE+0x0314) |= 1<<(2*8+7); //enable interrupt 
				break;
				
			case WMT_PIN_GP6_VDOUT19:
				REG32_VAL(__GPIO_BASE+0x0314) |= 1<<(3*8+7); //enable interrupt 
				break;
			case WMT_PIN_GP6_VDOUT20:
				REG32_VAL(__GPIO_BASE+0x0318) |= 1<<(0*8+7); //enable interrupt 
				
				break;
			
			case WMT_PIN_GP6_VDOUT21:
			
				REG32_VAL(__GPIO_BASE+0x0318) |= 1<<(1*8+7); //enable interrupt 
				break;
		}	
			
	}		
	
	return 0;
}

int wmt_disable_gpirq(void)
{
	int num = irq_gpio;
	
	if(num > /*11*//*19*/42)
		return -1;
	
	if(num<4)
		REG32_VAL(__GPIO_BASE+0x0300) &= ~(1<<(num*8+7)); //disable interrupt 
	else if(num >= 4 && num < 8)
		REG32_VAL(__GPIO_BASE+0x0304) &= ~(1<<((num-4)*8+7)); //enable interrupt 
	else if (num >= 8 && num <12)
		REG32_VAL(__GPIO_BASE+0x0308) &= ~(1<<((num-8)*8+7)); //enable interrupt
	else if (num >= 12 && num <16)
		REG32_VAL(__GPIO_BASE+0x030c) &= ~(1<<((num-8)*8+7)); //enable interrupt 
	else if (num >= 16 && num <20)
		REG32_VAL(__GPIO_BASE+0x0310) &= ~(1<<((num-8)*8+7)); //enable interrupt
	else ////vdout11 12 18:21-->gpio 31 32  38:41
	{	// 0x0314----0x0319
		switch (num)
		{
			case WMT_PIN_GP5_VDOUT11:
				REG32_VAL(__GPIO_BASE+0x0314) &= ~(1<<(0*8+7)); //enable interrupt 
				
				break;
			case WMT_PIN_GP5_VDOUT12:
				REG32_VAL(__GPIO_BASE+0x0314) &= ~(1<<(1*8+7)); //enable interrupt 
				break;
			
			case WMT_PIN_GP6_VDOUT18:
				REG32_VAL(__GPIO_BASE+0x0314) &= ~(1<<(2*8+7)); //enable interrupt 
				break;
				
			case WMT_PIN_GP6_VDOUT19:
				REG32_VAL(__GPIO_BASE+0x0314) &= ~(1<<(3*8+7)); //enable interrupt 
				break;
			case WMT_PIN_GP6_VDOUT20:
				REG32_VAL(__GPIO_BASE+0x0318) &= ~(1<<(0*8+7)); //enable interrupt 
				
				break;
			
			case WMT_PIN_GP6_VDOUT21:
			
				REG32_VAL(__GPIO_BASE+0x0318) &= ~(1<<(1*8+7)); //enable interrupt 
				break;
		}	
			
	}			
	
	return 0;
}

static int pn544_wmt_is_int(int num)
{
	if (num > 42)
	{
		return 0;
	}
	if (num < 20)
	{
		return REG32_VAL(__GPIO_BASE+0x0360) & (1<<num) ? 1 : 0;
	}
	else
	{
		switch (num)
		{
			case WMT_PIN_GP5_VDOUT11:
				return REG32_VAL(__GPIO_BASE+0x0360) & (1<<20) ? 1 : 0;
				break;
			case WMT_PIN_GP5_VDOUT12:
				return REG32_VAL(__GPIO_BASE+0x0360) & (1<<21) ? 1 : 0;
				break;
			case WMT_PIN_GP6_VDOUT18:
				return REG32_VAL(__GPIO_BASE+0x0360) & (1<<22) ? 1 : 0;
				break;
			case WMT_PIN_GP6_VDOUT19:
				return REG32_VAL(__GPIO_BASE+0x0360) & (1<<23) ? 1 : 0;
				break;
			case WMT_PIN_GP6_VDOUT20:
				return REG32_VAL(__GPIO_BASE+0x0360) & (1<<24) ? 1 : 0;
				break;
			case WMT_PIN_GP6_VDOUT21:
				return REG32_VAL(__GPIO_BASE+0x0360) & (1<<25) ? 1 : 0;
				break;
			
		}
		
	}
	//return (REG32_VAL(__GPIO_BASE+0x0360) & (1<<num)) ? 1: 0; 
}
//*************add end


static void pn544_disable_irq(struct pn544_dev *pn544_dev)
{
	unsigned long flags;

	spin_lock_irqsave(&pn544_dev->irq_enabled_lock, flags);
	if (pn544_dev->irq_enabled) {
		wmt_disable_gpirq();
		//disable_irq_nosync(pn544_dev->client->irq);
		pn544_dev->irq_enabled = false;
	}
	spin_unlock_irqrestore(&pn544_dev->irq_enabled_lock, flags);
}

static irqreturn_t pn544_dev_irq_handler(int irq, void *dev_id)
{
	struct pn544_dev *pn544_dev = dev_id;
	//printk("<<<<<<<%s!\n", __func__);
#if 0	
	if (!gpio_get_value(pn544_dev->irq_gpio)) {
		
		return IRQ_NONE;
		//return IRQ_HANDLED;//???
	}
#endif	
	if (pn544_wmt_is_int(pn544_dev->irq_gpio))
	{
		wmt_clr_int();
		pn544_disable_irq(pn544_dev);
	
	//printk("<<<<<<<%s! wakeup reader!\n", __func__);
	/* Wake up waiting readers */
		wake_up(&pn544_dev->read_wq);
		return IRQ_HANDLED;
	}
	else
	{
		return IRQ_NONE;
	}	
}

static ssize_t pn544_dev_read(struct file *filp, char __user *buf,
		size_t count, loff_t *offset)
{
	struct pn544_dev *pn544_dev = filp->private_data;
	char tmp[MAX_BUFFER_SIZE];
	int ret,i;

	if (count > MAX_BUFFER_SIZE)
		count = MAX_BUFFER_SIZE;

	printk("%s : reading %zu bytes.\n", __func__, count);

	mutex_lock(&pn544_dev->read_mutex);
	
	printk("%s irq gpio %d val %d\n", __func__, pn544_dev->irq_gpio, gpio_get_value(pn544_dev->irq_gpio));
	if (/*!gpio_get_value(pn544_dev->irq_gpio)*/gpio_get_value(pn544_dev->irq_gpio)!=pn544_dev->irq_active) {
		printk("%s &&& waitting for interrupt!\n", __func__);
		if (filp->f_flags & O_NONBLOCK) {
			ret = -EAGAIN;
			goto fail;
		}

		while (1) {
			pn544_dev->irq_enabled = true;
			wmt_enable_gpirq();
		//enable_irq(pn544_dev->client->irq);
		
			printk("%s &&&  enter waitting for interrupt!\n", __func__);
			
				ret = wait_event_interruptible(
					pn544_dev->read_wq,
					!pn544_dev->irq_enabled/*false??*/);
					
		/*ret = wait_event_interruptible(pn544_dev->read_wq,
				gpio_get_value(pn544_dev->irq_gpio));
		*/

		pn544_disable_irq(pn544_dev);

		if (ret)
			goto fail;
			if (gpio_get_value(pn544_dev->irq_gpio)==pn544_dev->irq_active)
				break;

			pr_warning("%s: spurious interrupt detected\n", __func__);
		}
	}

	/* Read data */
	ret = i2c_master_recv(pn544_dev->client, tmp, count);
	mutex_unlock(&pn544_dev->read_mutex);
	/* pn544 seems to be slow in handling I2C read requests
	 * so add 1ms delay after recv operation */
	udelay(1000);

	if (ret < 0) {
		pr_err("%s: i2c_master_recv returned %d\n", __func__, ret);
		return ret;
	}
	if (ret > count) {
		pr_err("%s: received too many bytes from i2c (%d)\n",
			__func__, ret);
		return -EIO;
	}
	if (copy_to_user(buf, tmp, ret)) {
		pr_warning("%s : failed to copy to user space\n", __func__);
		return -EFAULT;
	}
	
	printk("IFD->PC:");
	for(i = 0; i < ret; i++){
		printk(" %02X", tmp[i]);
	}
	printk("\n");
	
	return ret;

fail:
	printk("%s fail end! ret :%x\n", __func__, ret);
	mutex_unlock(&pn544_dev->read_mutex);
	return ret;
}

static ssize_t pn544_dev_write(struct file *filp, const char __user *buf,
		size_t count, loff_t *offset)
{
	struct pn544_dev  *pn544_dev;
	char tmp[MAX_BUFFER_SIZE];
	int ret,i;

	pn544_dev = filp->private_data;

	if (count > MAX_BUFFER_SIZE)
		count = MAX_BUFFER_SIZE;

	if (copy_from_user(tmp, buf, count)) {
		pr_err("%s : failed to copy from user space\n", __func__);
		return -EFAULT;
	}

	printk("%s : writing %zu bytes.\n", __func__, count);
	/* Write data */
	ret = i2c_master_send(pn544_dev->client, tmp, count);
	if (ret != count) {
		pr_err("%s : i2c_master_send returned %d\n", __func__, ret);
		ret = -EIO;
	}
	printk("PC->IFD:");
	for(i = 0; i < count; i++){
		printk(" %02X", tmp[i]);
	}
	/* pn544 seems to be slow in handling I2C write requests
	 * so add 1ms delay after I2C send oparation */
	udelay(1000);

	return ret;
}

static int pn544_dev_open(struct inode *inode, struct file *filp)
{
	struct pn544_dev *pn544_dev = container_of(filp->private_data,
						struct pn544_dev,
						pn544_device);

	filp->private_data = pn544_dev;

	pr_debug("%s : %d,%d\n", __func__, imajor(inode), iminor(inode));

	return 0;
}

static /*int*/long pn544_dev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct pn544_dev *pn544_dev = filp->private_data;

	switch (cmd) {
	case PN544_SET_PWR:  
		if (arg == 2) {
			/* power on with firmware download (requires hw reset)
			 */
			printk("%s power on with firmware\n", __func__);
			gpio_set_value(pn544_dev->ven_gpio, /*1*/pn544_dev->ven_active);
			msleep(20);
			if (pn544_dev->firm_gpio >= 0)
				gpio_set_value(pn544_dev->firm_gpio, /*1*/pn544_dev->firm_active);
			msleep(20);
			gpio_set_value(pn544_dev->ven_gpio, /*0*/!pn544_dev->ven_active);
			msleep(100);
			gpio_set_value(pn544_dev->ven_gpio, /*1*/pn544_dev->ven_active);
			msleep(20);
			pn544_dev->ven_on_off  = 1;
		} else if (arg == 1) {
			/* power on */
			printk("%s power on\n", __func__);
			if (pn544_dev->firm_gpio >= 0)
				gpio_set_value(pn544_dev->firm_gpio, /*0*/!pn544_dev->firm_active);
			gpio_set_value(pn544_dev->ven_gpio, /*1*/pn544_dev->ven_active);
			msleep(100);
			pn544_dev->ven_on_off  = 1;
		} else  if (arg == 0) {
			/* power off */
			printk("%s power off ven %d \n", __func__, !pn544_dev->ven_active);
			if (pn544_dev->firm_gpio >= 0)
				gpio_set_value(pn544_dev->firm_gpio, /*0*/!pn544_dev->firm_active);
			gpio_set_value(pn544_dev->ven_gpio, /*0*/!pn544_dev->ven_active);
			
			pn544_dev->ven_on_off  = 0;
			//gpio_set_value(pn544_dev->ven_gpio, 0);
			msleep(100);
		} else {
			printk("%s bad arg %u\n", __func__, arg);
			return -EINVAL;
		}
		break;
	default:
		printk("%s bad ioctl %u\n", __func__, cmd);
		return -EINVAL;
	}

	return 0;
}

static const struct file_operations pn544_dev_fops = {
	.owner	= THIS_MODULE,
	.llseek	= no_llseek,
	.read	= pn544_dev_read,
	.write	= pn544_dev_write,
	.open	= pn544_dev_open,
	.unlocked_ioctl  = pn544_dev_ioctl,
};


static int pn544_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int ret;
	struct pn544_nfc_platform_data *platform_data;
	struct pn544_dev *pn544_dev;

	platform_data = client->dev.platform_data;

	if (platform_data == NULL) {
		pr_err("%s : nfc probe fail\n", __func__);
		return  -ENODEV;
	}

	printk("nfc probe step01 is ok\n");
	
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("%s : need I2C_FUNC_I2C\n", __func__);
		return  -ENODEV;
	}

	printk("nfc probe step02 is ok\n");
	
	ret = gpio_request(platform_data->irq_gpio, "nfc_int");
	if (ret)
		return  -ENODEV;
	
	
	
	ret = gpio_request(platform_data->ven_gpio, "nfc_ven");
	if (ret)
		goto err_ven;
	
	
	if (platform_data->firm_gpio >= 0)
	{
		
		ret = gpio_request(platform_data->firm_gpio, "nfc_firm");
		if (ret)
			goto err_firm;
		
	}	

	pn544_dev = kzalloc(sizeof(*pn544_dev), GFP_KERNEL);
	if (pn544_dev == NULL) {
		dev_err(&client->dev,
				"failed to allocate memory for module data\n");
		ret = -ENOMEM;
		goto err_exit;
	}

	printk("nfc probe step04 is ok\n");
	
	//pn544_dev->irq_enable = platform_data->irq_enable;
	pn544_dev->irq_gpio = platform_data->irq_gpio;
	pn544_dev->irq_active = platform_data->irq_active;
	
	//pn544_dev->ven_enable  = platform_data->ven_enable;
	pn544_dev->ven_gpio  = platform_data->ven_gpio;
	pn544_dev->ven_active  = platform_data->ven_active;
	pn544_dev->ven_on_off  = 0;
	//pn544_dev->firm_enable  = platform_data->firm_enable;
	pn544_dev->firm_gpio  = platform_data->firm_gpio;
	pn544_dev->firm_active  = platform_data->firm_active;
	
	pn544_dev->client   = client;
	
	irq_gpio = pn544_dev->irq_gpio;
	/* init mutex and queues */
	init_waitqueue_head(&pn544_dev->read_wq);
	mutex_init(&pn544_dev->read_mutex);
	spin_lock_init(&pn544_dev->irq_enabled_lock);

	pn544_dev->pn544_device.minor = MISC_DYNAMIC_MINOR;
	switch (g_chip_nr)
	{
		case 7:
			pn544_dev->pn544_device.name = "pn547"; //"pn544" modify 2014-7-21 
			break;
		case 4:
			pn544_dev->pn544_device.name = "pn544"; //"pn544" modify 2014-8-22
			break;
	}	
	pn544_dev->pn544_device.fops = &pn544_dev_fops;

	ret = misc_register(&pn544_dev->pn544_device);
	if (ret) {
		pr_err("%s : misc_register failed\n", __FILE__);
		goto err_misc_register;
	}
	printk("nfc probe step05 is ok\n");

	/* request irq.  the irq is set whenever the chip has data available
	 * for reading.  it is cleared when all data has been read.
	 */
	
	
	gpio_direction_output(platform_data->ven_gpio,/*0*/ !platform_data->ven_active);
	
	if (platform_data->firm_gpio >= 0)
		gpio_direction_output(platform_data->firm_gpio,/*0*/!platform_data->firm_active);
	//s3c_gpio_setpull(platform_data->ven_gpio, S3C_GPIO_PULL_UP);
	//s3c_gpio_setpull(platform_data->firm_gpio, S3C_GPIO_PULL_DOWN);

	//eint16 setting
	
	gpio_direction_input(platform_data->irq_gpio);
	//wmt_gpio_setpull(platform_data->irq_gpio, WMT_GPIO_PULL_UP);
	if (platform_data->irq_active)
	{
		printk("%s irq pull down!\n", __func__);
		wmt_gpio_setpull(platform_data->irq_gpio, WMT_GPIO_PULL_DOWN); //modify 2014-7-16
	}	
	else
	{
		printk("%s irq pull up!\n", __func__);
		wmt_gpio_setpull(platform_data->irq_gpio, WMT_GPIO_PULL_UP);	
	}	
	//s3c_gpio_setpull(platform_data->irq_gpio, S3C_GPIO_PULL_UP);

	
	pr_info("%s : requesting IRQ %d\n", __func__, client->irq);
	pn544_dev->irq_enabled = true;

	ret = request_irq(client->irq, pn544_dev_irq_handler,
			  /*platform_data->irq_active?IRQF_TRIGGER_HIGH:IRQF_TRIGGER_LOW*/IRQF_SHARED, \
			  client->name, pn544_dev);//IRQF_TRIGGER_RISING  IRQF_TRIGGER_HIGH  
	if (ret) {
		printk(/*&client->dev, */"request_irq failed\n");
		goto err_request_irq_failed;
	}
	printk("nfc probe step06 is ok\n");
	//add 2014-7-16 for gpio irq config
	wmt_set_irqinput();
	wmt_set_gpirq(platform_data->irq_active?IRQF_TRIGGER_HIGH:IRQF_TRIGGER_LOW); //IRQF_TRIGGER_HIGH
	//wmt_enable_gpirq();
	//add end
	
	pn544_disable_irq(pn544_dev);
	i2c_set_clientdata(client, pn544_dev);
	
	//add debug 2014-7-10
#if 0		
	printk("%s power on\n", __func__);
	gpio_set_value(pn544_dev->firm_gpio, 0);
	gpio_set_value(pn544_dev->ven_gpio, 1);
	msleep(10);

	int maddr = 1;
	char mbuf[2] = {0};
	for (maddr=1; maddr<0x7f; maddr++)
	{
		mbuf[0] = maddr;
		/* Write data */
		//pn544_dev->client->addr = maddr;
	ret = i2c_master_send(pn544_dev->client, mbuf, 1);
	if (ret != 1) {
		pr_err("%s : reg 0x%x i2c_master_send returned %d\n", __func__, maddr, ret);
		//ret = -EIO;
	}
	else
	{
		pr_err("%s ok!!!: reg 0x%x i2c_master_send returned %d\n", __func__, maddr, ret);
		//break;
	}
	
	struct i2c_msg msg[2] = {
		{.addr = client->addr,
		 .flags = 0|I2C_M_NOSTART,
		 .len = 1,
		 .buf = &mbuf[0],
		},
		{ .addr = client->addr,
		  .flags = I2C_M_RD,
		  .len = 1,
		  .buf = &mbuf[1],
		},
		};
	
	ret = i2c_transfer(client->adapter, msg, 2);
	
	//ret = i2c_master_recv(pn544_dev->client, mbuf, 1);
	if (ret != 2) {
		pr_err("%s : addr 0x%x i2c_master_recv %d returned %d\n", __func__, maddr, mbuf[1], ret);
		//ret = -EIO;
	}
	else
	{
		pr_err("%s ok!!!: addr 0x%x i2c_master_recv %d returned %d\n", __func__, maddr, mbuf[1], ret);
		//break;
	}
	
	}
#endif	
	//add end
	
	printk("nfc probe step07 is ok\n");

	return 0;

err_request_irq_failed:
	misc_deregister(&pn544_dev->pn544_device);
err_misc_register:
	mutex_destroy(&pn544_dev->read_mutex);
	kfree(pn544_dev);
err_exit:
	if (platform_data->firm_gpio >= 0)
		gpio_free(platform_data->firm_gpio);
err_firm:
	
	gpio_free(platform_data->ven_gpio);
err_ven:
	
	gpio_free(platform_data->irq_gpio);
	return ret;
}

static int pn544_remove(struct i2c_client *client)
{
	struct pn544_dev *pn544_dev;

	pn544_dev = i2c_get_clientdata(client);
	free_irq(client->irq, pn544_dev);
	misc_deregister(&pn544_dev->pn544_device);
	mutex_destroy(&pn544_dev->read_mutex);
	gpio_free(pn544_dev->irq_gpio);
	gpio_free(pn544_dev->ven_gpio);
	gpio_free(pn544_dev->firm_gpio);
	kfree(pn544_dev);

	return 0;
}

static const struct i2c_device_id pn544_id[] = {
	{ "pn544", 0 },
	{ }
};

static int pn544_suspend(struct i2c_client *client, pm_message_t mesg)
{
	struct pn544_dev *pn544_dev;

	pn544_dev = i2c_get_clientdata(client);
	printk("\n%s on_off %d\n", __func__, pn544_dev->ven_on_off);
	return 0;
}

static int pn544_resume(struct i2c_client *client)
{
	struct pn544_dev *pn544_dev;

	pn544_dev = i2c_get_clientdata(client);
	
	printk("%s on_off %d\n", __func__, pn544_dev->ven_on_off);
	
	gpio_direction_input(pn544_dev->irq_gpio);
	//wmt_gpio_setpull(platform_data->irq_gpio, WMT_GPIO_PULL_UP);
	if (pn544_dev->irq_active)
		wmt_gpio_setpull(pn544_dev->irq_gpio, WMT_GPIO_PULL_DOWN); //modify 2014-7-16
	else
		wmt_gpio_setpull(pn544_dev->irq_gpio, WMT_GPIO_PULL_UP);	
		
	//add 2014-7-16 for gpio irq config
	wmt_set_irqinput();
	wmt_set_gpirq(pn544_dev->irq_active?IRQF_TRIGGER_HIGH:IRQF_TRIGGER_LOW); //IRQF_TRIGGER_HIGH
	//wmt_enable_gpirq();
	//add end
	
	pn544_disable_irq(pn544_dev);
	
	printk("%s active %d, !active %d\n", __func__, pn544_dev->ven_active, !pn544_dev->ven_active);
	if (pn544_dev->ven_on_off)
		gpio_set_value(pn544_dev->ven_gpio, pn544_dev->ven_active);
	else
		gpio_set_value(pn544_dev->ven_gpio, !pn544_dev->ven_active);
	
	//gpio_set_value(pn544_dev->ven_gpio, 0);	
	
	return 0;
}
static struct i2c_driver pn544_driver = {
	.id_table	= pn544_id,
	.probe		= pn544_probe,
	.remove		= pn544_remove,
	.suspend	= pn544_suspend,
	.resume		= pn544_resume,
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= "pn544",
	},
};

/*
 * module load/unload record keeping
 */

static struct pn544_nfc_platform_data pn544_pdata = {
	//.irq_enable = 1,
	.irq_gpio = WMT_PIN_GP0_GPIO0,
	.irq_active = 1,
	//.ven_enable = 1,
	.ven_gpio= WMT_PIN_GP18_UART0RTS, // WMT_PIN_GP18_UART0RTS    WMT_PIN_GP0_GPIO1
	.ven_active = 1,
	//.firm_enable = 1,
	.firm_gpio=WMT_PIN_GP0_GPIO1,	//WMT_PIN_GP0_GPIO2,
	.firm_active = 1,
};
static int g_i2c_adapter = 0;
static int g_i2c_addr = 0x28;
static int get_board_info(void)
{
	int ret;
	char buf[100] = {0};
	int len = sizeof(buf);
	
	char *pbuf = "wmt.nfc.pn54x"; // wmt.nfc.pn547
	printk("%s\n", __func__);
	//wmt.nfc.pn54x 4:43:0:39:1:1:1:132:1  2b-->43  wmt.nfc.pn54x 4:43:0:39:1:1:0:132:1
	ret = wmt_getsyspara(pbuf, buf, &len);
	if (!ret)
	{
		printk("%s %s:%s\n", __func__, pbuf, buf);//irq ven firm
		
		ret = sscanf(buf, "%d:%d:%d:%d:%d:%d:%d:%d:%d",  \
			&g_chip_nr, &g_i2c_addr, &g_i2c_adapter, \
			&pn544_pdata.irq_gpio, &pn544_pdata.irq_active, \
			&pn544_pdata.ven_gpio, &pn544_pdata.ven_active, \
			&pn544_pdata.firm_gpio, &pn544_pdata.firm_active);
		
		printk("chip nr %d,i2c addr %d adapter %d, irq gpio %d active %d, ven %d %d, firm %d %d\n", \
			g_chip_nr, g_i2c_addr, g_i2c_adapter, \
			pn544_pdata.irq_gpio, pn544_pdata.irq_active, \
			pn544_pdata.ven_gpio, pn544_pdata.ven_active, \
			pn544_pdata.firm_gpio, pn544_pdata.firm_active);
			
		return 0;
	}
	else
	{
		printk("%s not get %s, use default\n", __func__, pbuf);
		return -1;
	}
	
	return 0;
}

static int __init pn544_dev_init(void)
{
	//pr_info("Loading pn544 driver\n");
	
	int r;
	struct i2c_adapter *adapter;
		
		
	
	struct i2c_board_info wmt_pn544_bi = {
		.type		   = PN544_DRIVER_NAME,
		.flags		   = 0x00,
		.addr		   = CLIENT_ADDR,
		.platform_data = &pn544_pdata, //custom 2014-7-25
		.archdata	   = NULL,
		.irq		   = IRQ_GPIO,
	};
	//wmt_pn544_bi.addr = g_i2c_addr;
	
	pr_debug(DRIVER_DESC ": %s\n", __func__);
	r = get_board_info();
	if (r < 0)
	{
		printk("%s no env!!\n", __func__);
		return r;
	}
	wmt_pn544_bi.addr = g_i2c_addr;
	adapter = i2c_get_adapter(/*WMT_PN544_I2C_CHANNEL*/g_i2c_adapter);
	if (adapter == NULL) {
		printk("can not get i2c adapter, client address error");
		return -ENODEV;
	}
	
	pn544_client = i2c_new_device(adapter, &wmt_pn544_bi);
	if ( pn544_client == NULL) {
		printk("allocate i2c client failed");
		return -ENOMEM;
	}
	
	i2c_put_adapter(adapter);
		
	r = i2c_add_driver(&pn544_driver);
	if (r) {
		pr_err(PN544_DRIVER_NAME ": driver registration failed\n");
		return r;
	}
	
	return 0;
	//return i2c_add_driver(&pn544_driver);
}
module_init(pn544_dev_init);

static void __exit pn544_dev_exit(void)
{
	pr_info("Unloading pn544 driver\n");
	i2c_del_driver(&pn544_driver);
	
	i2c_unregister_device(pn544_client);
}
module_exit(pn544_dev_exit);

MODULE_AUTHOR("Sylvain Fonteneau");
MODULE_DESCRIPTION("NFC PN544 driver");
MODULE_LICENSE("GPL");

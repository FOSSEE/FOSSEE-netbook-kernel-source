/*
 * linux/drivers/char/wmt_gpio.c
 * 
 * Copyright (c) 2008  WonderMedia Technologies, Inc.
 * 
 * This program is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software Foundation,
 * either version 2 of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE.  See the GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 * 
 * WonderMedia Technologies, Inc.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/watchdog.h>
#include <linux/init.h>

#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <linux/types.h>
#include <mach/hardware.h>

#define OSCR_FREQ               3000000         /* 3 MHz tick rate */

static unsigned long wmt_wdt_users;
static int expect_close;
static int pre_margin;

static unsigned int wdt_tcr;
static unsigned int wdt_tmr0;
static unsigned int wdt_enable;

#define WDT_NS_RSR (PM_CTRL_BASE_ADDR+0x8050) //NS Reset Status Register
#define WDT_NS_TMR4 (PM_CTRL_BASE_ADDR+0x80f0) //NS OS Timer Match Register 4
#define WDT_NS_TMR5 (PM_CTRL_BASE_ADDR+0x80f4) //NS OS Timer Match Register 5
#define WDT_NS_TMR6 (PM_CTRL_BASE_ADDR+0x80f8) //NS OS Timer Match Register 6
#define WDT_NS_TMR7 (PM_CTRL_BASE_ADDR+0x80fc) //NS OS Timer Match Register 7
#define WDT_NS_TMR0 (PM_CTRL_BASE_ADDR+0x8100) //NS OS Timer Match Register 0
#define WDT_NS_TMR1 (PM_CTRL_BASE_ADDR+0x8104) //NS OS Timer Match Register 1
#define WDT_NS_TMR2 (PM_CTRL_BASE_ADDR+0x8108) //NS OS Timer Match Register 2
#define WDT_NS_TMR3 (PM_CTRL_BASE_ADDR+0x810c) //NS OS Timer Match Register 3
#define WDT_NS_TCR  (PM_CTRL_BASE_ADDR+0x8110) //NS OS Timer Count Register
#define WDT_NS_TSR  (PM_CTRL_BASE_ADDR+0x8114) //NS OS Timer Status register
#define WDT_NS_TWER (PM_CTRL_BASE_ADDR+0x8118) //NS OS Timer Watch Dog Enable Register
#define WDT_NS_IER  (PM_CTRL_BASE_ADDR+0x811c) //NS OS Timer Interrupt Enable Register
#define WDT_NS_CTRL  (PM_CTRL_BASE_ADDR+0x8120) //NS OS Timer Control Register
#define WDT_NS_TASR (PM_CTRL_BASE_ADDR+0x8124) //NS OS Timer Access Status Register
#define WMT_NS_EN (1<<4)
#define WMT_NS_RESET (1<<3)


unsigned int wmt_read_ns_oscr(void)
{
        //
        // Request the reading of OS Timer Count Register.
        //
        REG32_VAL(WDT_NS_CTRL) |= OSTC_RDREQ;

        //
        // Polling until free to reading.
        //        
        while ( REG32_VAL(WDT_NS_TASR) & OSTA_RCA );

        return REG32_VAL(WDT_NS_TCR);
}


void wmt_write_ns_oscr(unsigned int val)
{
        //
        // Polling until free to reading.
        //        
        while ( REG32_VAL(WDT_NS_TASR) & OSTA_CWA );

       REG32_VAL(WDT_NS_TCR) = val;
}

/* wmt_wdt_open()
 *
 * There is a simple mutex to protect only one user could use it.
 */
static int wmt_wdt_open(struct inode *inode, struct file *file)
{
        if (test_and_set_bit(1, &wmt_wdt_users))
                return -EBUSY;

        /*
         * 1. Set timer match value.
         * 2. Activate watchdog timer.
         */
        REG32_VAL(WDT_NS_TMR0) = pre_margin + wmt_read_ns_oscr();
	REG32_VAL(WDT_NS_TWER) = OSTW_WE;

        OSTW_VAL |= WMT_NS_EN;
        REG32_VAL(WDT_NS_CTRL) = OSTC_ENABLE;

        return 0;
}

/* wmt_wdt_release()
 *
 * Shut off the watchdog timer.
 */
static int wmt_wdt_release(struct inode *inode, struct file *file)
{
        /*
         * Release watchdog timer depend on NOWAYOUT option.
         */
#ifndef CONFIG_WATCHDOG_NOWAYOUT
	if (expect_close)
		REG32_VAL(WDT_NS_TWER) = 0;
	else
		printk("CONFIG_WATCHDOG_NOWAYOUT\n");
	
#else
	printk(KERN_CRIT "Watchdog: WDT device closed unexpectedly. " \
                            "WDT will not stop!\n");
#endif
        clear_bit(1, &wmt_wdt_users);
        expect_close = 0;

        return 0;
}

/* wmt_wdt_write()
 *
 */
static ssize_t wmt_wdt_write(struct file *file, const char *data, size_t len, loff_t *ppos)
{
        int ret;

        /*
         * Check if we can seek (pwrite) on this device.
         */        

        if ( len ) {
#ifndef CONFIG_WATCHDOG_NOWAYOUT

	size_t i;

	expect_close = 0;

	for ( i=0; i!=len; i++ ) {
		char c;

		if ( get_user(c, data + i) )
			return -EFAULT;

		/*
		* The magic character 'V' is for shotdown
		* watchdog.
		*/
		if ( c == 'V' )
			expect_close = 1;                                        
	}
#endif                

                /*
                 * Refresh watchdog timer.
                 */
               //printk("Refresh %d to ",REG32_VAL(WDT_NS_TMR0));
               REG32_VAL(WDT_NS_TMR0) = pre_margin + wmt_read_ns_oscr();
               //printk(" to %d\n",REG32_VAL(WDT_NS_TMR0));
                
        }

        ret = len ? 1 : 0;
        return ret;
}

static struct watchdog_info ident = {
        .options        = WDIOF_MAGICCLOSE |
                          WDIOF_SETTIMEOUT | WDIOF_KEEPALIVEPING,
        .identity       = "WMT Watchdog",
        .firmware_version = 0,

};

/* wmt_wdt_ioctl()
 *
 */
static long wmt_wdt_ioctl(struct file *file, unsigned int cmd,
						unsigned long arg)
//static int wmt_wdt_ioctl(struct inode *inode, struct file *file,
//        unsigned int cmd, unsigned long arg)
{
        int ret = 0, time;

        switch ( cmd ) {
        case WDIOC_GETSUPPORT:
                {
                        ret = copy_to_user((struct watchdog_info *)arg, &ident,
                                           sizeof(ident)) ? -EFAULT : 0;
                        break;
                }
        case WDIOC_GETSTATUS:
		return put_user(0, (int __user *)arg);
        case WDIOC_GETBOOTSTATUS:
                {
                        /* TODO */
                        break;
                }

        case WDIOC_SETTIMEOUT:
                {
                        if (get_user(time, (int __user *)arg))
                        	return -EFAULT;
    
                        /* Sanity check */
                        if ( time <= 0 || time > 255 ) {
                                ret = -EINVAL;
                                break;
                        }
                        pre_margin = OSCR_FREQ * time;
                        REG32_VAL(WDT_NS_TMR0) = pre_margin + wmt_read_ns_oscr();
                        /* Falling down */
                }
        case WDIOC_GETTIMEOUT:
                {
                        ret = put_user(pre_margin / OSCR_FREQ, (int *)arg);
                        break;
                }
        case WDIOC_KEEPALIVE:
                {
                        REG32_VAL(WDT_NS_TMR0) = pre_margin + wmt_read_ns_oscr();
                        ret = 0;
                        break;
                }
        default:
                {
                        /* No this ioctl command */
                        ret = -ENOIOCTLCMD;
                }
        }

        return ret;
}

static struct file_operations wmt_dog_fops =
{
        .owner          = THIS_MODULE,
        .write          = wmt_wdt_write,
        .unlocked_ioctl = wmt_wdt_ioctl,
        .open           = wmt_wdt_open,
        .release        = wmt_wdt_release,
};

static struct miscdevice wmt_dog_miscdev =
{
        .minor          = WATCHDOG_MINOR,
        .name           = "watchdog",
        .fops           = &wmt_dog_fops,
};

static int margin __initdata = 60;      /* (secs) Default using one minute. */

static int wmt_watchdog_probe(struct platform_device *pdev)
{
        int ret;
        unsigned int val;

        /*
         * Check if there is an enabled watchdog timer
         * due to IO_RESET or SW_RESET.
         *
         */
	if ( REG32_VAL(WDT_NS_RSR) & WMT_NS_RESET)
		REG32_VAL(WDT_NS_RSR) = WMT_NS_RESET;

	val = OSTW_VAL;
	if (OSTW_VAL&WMT_NS_EN) {
		val &= ~WMT_NS_EN;
		OSTW_VAL = val;
	}

        if ( REG32_VAL(WDT_NS_CTRL) & OSTC_ENABLE)
                REG32_VAL(WDT_NS_CTRL) = 0;

        pre_margin = OSCR_FREQ * margin;

        ret = misc_register(&wmt_dog_miscdev);
	if (ret) {
		printk("Error: Can not register Watchdog driver\n");
		return ret;	
	}
    
        printk(KERN_INFO "Watchdog: timer margin %d sec\n", margin);

        return ret;
}

static int __devexit wmt_wdt_remove(struct platform_device *pdev)
{
	return 0;
}

#ifdef	CONFIG_PM
static int wmt_watchdog_suspend
(
	struct device *pdev
)
{

	wdt_enable = 0;
	if ( REG32_VAL(WDT_NS_CTRL) & OSTC_ENABLE) {
		wdt_tcr = wmt_read_ns_oscr();
        	wdt_tmr0 = REG32_VAL(WDT_NS_TMR0);
        	wdt_enable = 1;
        	printk("wdt_tcr = 0x%x , wdt_tmr0 = 0x%x \n",wdt_tcr,wdt_tmr0);
	}

	return 0;
}

static int wmt_watchdog_resume
(
	struct device *pdev
)
{
	if (wdt_enable) {

        	REG32_VAL(WDT_NS_TMR0) = wdt_tmr0;
        	wmt_write_ns_oscr(wdt_tcr);
		REG32_VAL(WDT_NS_TWER) = OSTW_WE;

	        OSTW_VAL |= WMT_NS_EN;
        	REG32_VAL(WDT_NS_CTRL) = OSTC_ENABLE;
		printk("resume : wdt_tcr = 0x%x , wdt_tmr0 = 0x%x \n",wmt_read_ns_oscr(),REG32_VAL(WDT_NS_TMR0));

	}
	return 0;
}

static int wmt_watchdog_freeze(struct device *dev) 
{
	return 0;
}

static int wmt_watchdog_restore(struct device *dev) 
{
	return 0;
}
#else

#define wmt_watchdog_suspend NULL
#define wmt_watchdog_resume NULL
#define wmt_watchdog_freeze NULL
#define wmt_watchdog_restore NULL

#endif

static struct dev_pm_ops wmt_watchdog_pm_ops = {
        .suspend  = wmt_watchdog_suspend,
        .resume   = wmt_watchdog_resume,
        .freeze    = wmt_watchdog_freeze,
        .thaw       = wmt_watchdog_restore,
        .restore   = wmt_watchdog_restore,
};


static struct platform_device wmt_watchdog_device = {
	.name           = "wmt-watchdog",
	.id             = 0,

};

static struct platform_driver wmt_watchdog_driver = {
	.driver = {
		.name =		"wmt-watchdog",
		.owner =	THIS_MODULE,
	},
	.probe	= wmt_watchdog_probe,
	.remove = __devexit_p(wmt_wdt_remove),
	 .driver.pm = &wmt_watchdog_pm_ops
};

static int __init wmt_watchdog_init(void)
{
	int ret;

	ret = platform_device_register(&wmt_watchdog_device);
	if(ret) {
		printk("Error: Can not register Watchdog platform\n");
		return ret;
	}
	ret = platform_driver_register(&wmt_watchdog_driver);
	if(ret) {
		printk("Error: Can not register Watchdog driver\n");
		platform_device_unregister(&wmt_watchdog_device);
		return ret;
	}
	return 0; 
}
module_init(wmt_watchdog_init);

static void __exit wmt_watchdog_exit(void)
{
	platform_driver_unregister(&wmt_watchdog_driver);
	platform_device_unregister(&wmt_watchdog_device);
}
module_exit(wmt_watchdog_exit);


MODULE_DESCRIPTION("WMT Watchdog Driver");

module_param(margin, int, 0);
MODULE_PARM_DESC(margin, "Watchdog margin in seconds (default 60s)");

MODULE_LICENSE("GPL");

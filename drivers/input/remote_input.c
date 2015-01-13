#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/jiffies.h>
#include <linux/io.h>
#include <linux/kthread.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/wakelock.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <mach/wmt_iomux.h>
#include <linux/input/mt.h>
#include <mach/hardware.h>
#include <linux/fs.h> 
#include <linux/file.h> 
#include <asm/uaccess.h>


#define INPUT_IOC_MAGIC                 'x'
#define INPUT_IOC_CMD_INPUT             _IOR(INPUT_IOC_MAGIC, 1,  int)
#define INPUT_IOC_MAXNR                 1

static struct input_dev *g_input = NULL;
static int lcdX, lcdY;
static int g_Major;
static struct mutex ioc_mutex;
static struct class *dev_class = NULL;

extern int wmt_getsyspara(char *varname, unsigned char *varval, int *varlen);

static struct input_dev *input_dev_alloc(void)
{
        int err;
        struct input_dev *input_dev;
        input_dev = input_allocate_device();
        if (!input_dev) {
                printk("failed to allocate input device\n");
                return NULL;
        }

        input_dev->name = "remote_input";
        input_dev->evbit[0] = BIT_MASK(EV_SYN) | BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS) | BIT_MASK(EV_REL);
        set_bit(INPUT_PROP_DIRECT, input_dev->propbit);
        //set_bit(ABS_MT_TRACKING_ID, input_dev->absbit);

        input_set_abs_params(input_dev,
                        ABS_MT_POSITION_X, 0, lcdX, 0, 0);
        input_set_abs_params(input_dev,
                        ABS_MT_POSITION_Y, 0, lcdY, 0, 0);
        //input_set_abs_params(input_dev,
        //                ABS_MT_TRACKING_ID, 0, 5, 0, 0);

        set_bit(KEY_BACK, input_dev->keybit);
        set_bit(KEY_HOME, input_dev->keybit);
        set_bit(KEY_MENU, input_dev->keybit);
        set_bit(KEY_SEARCH, input_dev->keybit);
		
		set_bit(KEY_ENTER, input_dev->keybit);
		set_bit(KEY_UP, input_dev->keybit);
		set_bit(KEY_PAGEUP, input_dev->keybit);
		set_bit(KEY_LEFT, input_dev->keybit);
		set_bit(KEY_RIGHT, input_dev->keybit);		
		set_bit(KEY_DOWN, input_dev->keybit);
		set_bit(KEY_PAGEDOWN, input_dev->keybit);
		set_bit(KEY_VOLUMEDOWN, input_dev->keybit);
		set_bit(KEY_VOLUMEUP, input_dev->keybit);
				
		set_bit(BTN_LEFT, input_dev->keybit);
		set_bit(BTN_RIGHT, input_dev->keybit);
		set_bit(BTN_MIDDLE, input_dev->keybit);
		set_bit(BTN_SIDE, input_dev->keybit);
		
		set_bit(REL_X, input_dev->relbit);
		set_bit(REL_Y, input_dev->relbit);
		set_bit(REL_WHEEL, input_dev->relbit);		

        err = input_register_device(input_dev);
        if (err) {
                printk("input_dev_alloc: failed to register input device.\n");
                input_free_device(input_dev);
                input_dev = NULL;
        }
		
        return input_dev;
}

static long input_ioctl(struct file *dev, unsigned int cmd, unsigned long arg)
{
        struct input_event event;
        if (_IOC_TYPE(cmd) != INPUT_IOC_MAGIC){ 
                printk("CMD ERROR!");
                return -ENOTTY;
        }

        if (_IOC_NR(cmd) > INPUT_IOC_MAXNR){ 
                printk("NO SUCH IO CMD!\n");
                return -ENOTTY;
        }

        switch (cmd) {
                case INPUT_IOC_CMD_INPUT:
                        copy_from_user(&event, (struct input_event*)arg, sizeof(struct input_event));
                        mutex_lock(&ioc_mutex);
                        input_event(g_input, event.type, event.code, event.value);
                        mutex_unlock(&ioc_mutex);
                        return 0;
        }
        return -EINVAL;
}

static int input_open(struct inode *inode, struct file *filp)
{
        int ret = 0;
        return ret;
}

static int input_close(struct inode *inode, struct file *filp)
{
        return 0;
}


static struct file_operations input_fops = {
        .unlocked_ioctl = input_ioctl,
        .open           = input_open,
        .release        = input_close,
};


static int __init remote_init(void)
{
	struct device *dev = NULL;
	int len = 127;
    char retval[128] = {0},*p=NULL;
	int tmp[6];
	
	mutex_init(&ioc_mutex);
	
	if (wmt_getsyspara("wmt.display.fb0", retval, &len)) {
		printk(KERN_ERR "Can't get display param. \n");
		return -EIO;
	}
	p = retval;
	sscanf(p, "%d:[%d:%d:%d:%d:%d", &tmp[0], &tmp[1], &tmp[2], &tmp[3], &tmp[4], &tmp[5]);
	lcdX = tmp[4];
	lcdY = tmp[5];
   
    g_input = input_dev_alloc();
	if (!g_input){
		printk(KERN_ERR "Alloc input device failed. \n");
		return -ENODEV;		
	}
	
	if ((g_Major = register_chrdev(0, "remote_input", &input_fops)) < 0) {
			printk(KERN_ERR "Can't register char device. \n");
			return -EIO;
	}
	
	dev_class = class_create(THIS_MODULE,"remote_input");
	if (IS_ERR(dev_class)) {
		unregister_chrdev(g_Major, "remote_input");
		printk(KERN_ERR "Class create failed. \n");
		return PTR_ERR(dev_class);
	}
	
	dev = device_create(dev_class, NULL, MKDEV(g_Major, 0), NULL,"remote_input");
	if(!dev){
		printk(KERN_ERR "Create device failed. \n");
		return -ENODEV;
	}
	
	return 0;
}
module_init(remote_init);

static void __exit remote_exit(void)
{  	
	device_destroy(dev_class, MKDEV(g_Major, 0));
	class_destroy(dev_class);
    unregister_chrdev(g_Major, "remote_input");
	
    input_unregister_device(g_input);
    //input_free_device(g_input);
	mutex_destroy(&ioc_mutex);
}
module_exit(remote_exit);

MODULE_DESCRIPTION("Remote Input driver");
MODULE_LICENSE("GPL v2");

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/kdev_t.h>
#include <linux/cdev.h>
#include <asm/uaccess.h>
#include <linux/gpio.h>
#include <linux/fs.h>
#include <mach/wmt_env.h>

#include "nmidrv_kernel.h"
#include "nmidrv_custom_wmt.h"

extern int nmi_cmos_init(void);
extern void nmi_cmos_exit(void);

struct nmi_5625_dev {
	struct i2c_client *i2c_client_atv;

	struct mutex mu;
	struct class *tv_class;
	dev_t devn;
	struct	cdev cdv;
};

static int already_init = 0;
static struct nmi_5625_dev nd;
static u8 i2cBuf[32];

static bool g_bIsAtvStart = false;

int nmi_running(void)
{
	return g_bIsAtvStart;
}
EXPORT_SYMBOL(nmi_running);

/**************************************************************

  file operation:

 **************************************************************/

static int nmi5625_open(struct inode *inode, struct file *file)
{
	int ret = 0;

	func_enter();
	if (!already_init) {
		ret = -ENODEV;
		goto _fail_;
	}

	/***************************************
	  initialize 2.8V 1.2V RESET GPIO mode  for reference
	 ****************************************/

	//PWR Enable
	NMI_SET_GPIO_MODE_ENABLE(NMI_POWER_VDDIO_PIN);
	NMI_SET_GPIO_DIR(NMI_POWER_VDDIO_PIN, 1);
	NMI_SET_GPIO_PULL_DISABLE(NMI_POWER_VDDIO_PIN);
	NMI_SET_GPIO_LEVEL(NMI_POWER_VDDIO_PIN, 0);

	//LDO_Enable
	NMI_SET_GPIO_MODE_ENABLE(NMI_POWER_VCORE_PIN);
	NMI_SET_GPIO_DIR(NMI_POWER_VCORE_PIN, 1);
	NMI_SET_GPIO_PULL_DISABLE(NMI_POWER_VCORE_PIN);
	NMI_SET_GPIO_LEVEL(NMI_POWER_VCORE_PIN, 0);

	//Reset
	NMI_SET_GPIO_MODE_ENABLE(NMI_RESET_PIN);
	NMI_SET_GPIO_DIR(NMI_RESET_PIN, 1);
	NMI_SET_GPIO_PULL_DISABLE(NMI_RESET_PIN);
	NMI_SET_GPIO_LEVEL(NMI_RESET_PIN, 0);

#ifndef NMI_HW_I2C
	nmi_i2c_init();
#endif
	file->private_data = (void *)&nd;

_fail_:

	func_exit();
	return ret;
}

static int nmi5625_release(struct inode * inode, struct file * file)
{
	int ret = 0;
	//struct nmi_5625_dev *d = file->private_data;

	/**
	  nothing to do
	 **/
	func_enter();

#ifndef NMI_HW_I2C
	nmi_i2c_deinit();
#endif
	func_exit();
	return ret;
}

static long nmi5625_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct nmi_5625_dev *d = file->private_data;
	int ret = 0;

	//func_enter();

	switch ((cmd&0xffff0000)) {

	case NMI_POWER_VDDIO_CTL:
		dPrint(N_TRACE,"NM5625_PWR_2P8_CTL, power %s\n", (arg==1)?"on":"off");
		if (arg == 1){	/* on */
			NMI_SET_GPIO_LEVEL(NMI_POWER_VDDIO_PIN, 1);
		}
		else{
			NMI_SET_GPIO_LEVEL(NMI_POWER_VDDIO_PIN, 0);
		}
		break;

	case NMI_POWER_VCORE_CTL:
		dPrint(N_TRACE,"NM5625_PWR_1P2_CTL, power %s\n", (arg==1)?"on":"off");
		if (arg == 1){	/* on */
			NMI_SET_GPIO_LEVEL(NMI_POWER_VCORE_PIN, 1);
			g_bIsAtvStart  = 1;
		}
		else{
			NMI_SET_GPIO_LEVEL(NMI_POWER_VCORE_PIN, 0);
			g_bIsAtvStart = 0;
		}
		break;

	case NMI_FM_POWER_VCORE_CTL:
		dPrint(N_TRACE,"NMI_FM_POWER_VCORE_CTL, power %s\n", (arg==1)?"on":"off");
		if (arg == 1){  /* on */
			NMI_SET_GPIO_LEVEL(NMI_POWER_VCORE_PIN, 1);
			//g_bIsAtvStart  = 1;
		} 
		else{
			NMI_SET_GPIO_LEVEL(NMI_POWER_VCORE_PIN, 0);
			//g_bIsAtvStart = 0;
		}
		break;
	case NMI_RESET_CTL:
		dPrint(N_TRACE,"NM5625_ATV_RESET_CTL, reset %s\n", (arg==1)?"high":"low");
		if (arg == 1) {
			NMI_SET_GPIO_LEVEL(NMI_RESET_PIN, 1);
		}
		else {
			NMI_SET_GPIO_LEVEL(NMI_RESET_PIN, 0);
		}
		break;
	case NMI_I2C_READ:
		{
			u8 *kbuf = &i2cBuf[0];
			int size = cmd&0xffff;	/* Note: I used the lower 16 bits for size */
			int len = size;

			dPrint(N_TRACE,"NM5625_ATV_I2C_READ\n");
			mutex_lock(&d->mu);

#ifdef NMI_HW_I2C

			while(len) {
				int sz;
				if (len > NMI_I2C_RW_LENGTH)
					sz = NMI_I2C_RW_LENGTH;
				else
					sz = len;
				ret = i2c_master_recv(d->i2c_client_atv, kbuf, sz);
				if (ret < 0) {
					dPrint(N_ERR, "nmi: failed i2c read...(%d)\n", ret);
					//kfree(kbuf);
					mutex_unlock(&d->mu);
					goto _fail_;
				}
				kbuf += NMI_I2C_RW_LENGTH;
				len -= sz;
			}

#else
			ret = nmi_i2c_read(0x60,kbuf,size);
#endif
			dPrint(N_TRACE,"kernel:nmi_i2c_read buf is (%p), size is (%d)\n",kbuf,size);

			if (copy_to_user((void *)arg, i2cBuf, size) ) {
				dPrint(N_ERR, "nmi: failed copy to user...\n");
				ret = -EFAULT;
				//kfree(kbuf);
				mutex_unlock(&d->mu);
				goto _fail_;
			}
			//kfree(kbuf);
			mutex_unlock(&d->mu);
		}
		break;
	case NMI_I2C_WRITE:
		{
			u8 *kbuf = &i2cBuf[0];
			int size = cmd&0xffff;	/* Note: I used the lower 16 bits for size */
			int len = size;
			dPrint(N_TRACE,"NM5625_ATV_I2C_WRITE\n");
			if (copy_from_user((void *)kbuf, (void *)arg, size)) {
				dPrint(N_ERR, "nmi: failed copy from user...\n");
				ret = -EFAULT;
				goto _fail_;
			}
			mutex_lock(&d->mu);

#ifdef NMI_HW_I2C

			while (len) {
				int sz;
				if (len > NMI_I2C_RW_LENGTH)
					sz = NMI_I2C_RW_LENGTH;
				else
					sz = len;

				ret = i2c_master_send(d->i2c_client_atv, kbuf, sz);

				if (ret < 0) {
					dPrint(N_ERR, "nmi: failed i2c write...(%d)\n", ret);
					//kfree(kbuf);
					mutex_unlock(&d->mu);
					goto _fail_;
				}
				kbuf += NMI_I2C_RW_LENGTH;
				len -= sz;
			}

#else
			ret = nmi_i2c_write(0x60,kbuf,size);
#endif
			dPrint(N_TRACE,"kernel:nmi_i2c_write buf is (%p), size is (%d)\n",kbuf,size);

			mutex_unlock(&d->mu);
		}
		break;
	default:
		break;
	}

_fail_:
	//func_exit();
	dPrint(N_TRACE, "nmi_ioctl return value...(%d)\n", ret);
	return ret;
}

static const struct file_operations nmi5625_fops = {
	.owner		= THIS_MODULE,
	.unlocked_ioctl	= nmi5625_ioctl,
	.open		= nmi5625_open,
	.release	= nmi5625_release,
};

/**************************************************************

i2c:

 **************************************************************/

static int nmi5625_remove(struct i2c_client *client)
{
	return 0;
}

static int nmi5625_detect(struct i2c_client *client, int kind, struct i2c_board_info *info)
{
	strcpy(info->type, "nmiatv");
	return 0;
}

static int nmi5625_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret = 0;
	struct device *dev;
	func_enter();

	if (!already_init) {
		memset(&nd, 0, sizeof(struct nmi_5625_dev));

		/**
		  initialize mutex
		 **/
		mutex_init(&nd.mu);

		/**
		  register our driver
		 **/
		if ((ret = alloc_chrdev_region (&nd.devn, 0, 1, "nmi")) < 0) {
			dPrint(N_ERR, "nmi: failed unable to get major...%d\n", ret);
			goto _fail_;
		}
		dPrint(N_INFO, "nmi:dynamic major(%d),minor(%d)\n", MAJOR(nd.devn), MINOR(nd.devn));

		cdev_init(&nd.cdv, &nmi5625_fops);
		nd.cdv.owner = THIS_MODULE;
		ret = cdev_add(&nd.cdv, nd.devn, 1);
		if (ret) {
			dPrint(N_ERR, "nmi: failed to add device...%d\n", ret);
			goto _fail_;
		}


		nd.tv_class = class_create(THIS_MODULE, "atv");
		if (IS_ERR(nd.tv_class)) {
			dPrint(N_ERR, "nmi: failed to create the atv class\n");
		}

		dev = device_create(nd.tv_class, NULL, nd.devn, NULL, "nmi");
		if (IS_ERR(dev)) {
			dPrint(N_ERR, "nmi: failed to create device\n");
		}
		/*User interface end */

		already_init = 1;
	}

	nd.i2c_client_atv = client;

_fail_:

	func_exit();
	return ret;
}

static const struct i2c_device_id nmi5625_id[] = {
	{"nmiatv", 0},
	{},
};

//static unsigned short force[] = {2, 0xc0, I2C_CLIENT_END, I2C_CLIENT_END};
//static const unsigned short * const forces[] = { force, NULL };
//static struct i2c_client_address_data addr_data = { .forces = forces,};

static struct i2c_driver nmi5625_i2c_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name  = "nmiatv",
	},
	.probe  = nmi5625_probe,
	.detect = nmi5625_detect,
	.remove = nmi5625_remove,
	.id_table = nmi5625_id,
	//    .address_data =&addr_data,
};

static struct i2c_board_info nmi5625_i2c_dev = {
	I2C_BOARD_INFO("nmiatv", 0x60),
};

/**************************************************************

Module:

 **************************************************************/

static int parse_nmi_param(void)
{
	char env[] = "wmt.nmi.param";
	char buf[64];
	size_t l = sizeof(buf);
	int nr = -EINVAL;

	if (wmt_getsyspara(env, buf, &l) == 0) {
		sscanf(buf, "i2c-%d", &nr);
		pr_info("nmi5625 i2c adapter %d\n", nr);
	}

	return nr;
}

static __init int nmi5625_init(void)
{
	int ret;
	struct i2c_adapter *adapter;
	struct i2c_client *client;
	int nr;
	func_enter();

	nr = parse_nmi_param();
	if (nr < 0)
		return nr;

	adapter = i2c_get_adapter(nr);
	client = i2c_new_device(adapter, &nmi5625_i2c_dev);
	i2c_put_adapter(adapter);
	if (!client) {
		printk("nmi i2c_new_device failed\n");
		return -EINVAL;
	}

	ret = i2c_add_driver(&nmi5625_i2c_driver);
	if (ret < 0) {
		dPrint(N_ERR, "nmi: failed register i2c driver...(%d)\n", ret);
	}

	func_exit();

	return ret;
}

static __exit void nmi5625_clean(void)
{
	func_enter();

	i2c_del_driver(&nmi5625_i2c_driver);

	if (already_init) {
		device_destroy(nd.tv_class, nd.devn);
		class_destroy(nd.tv_class);
		cdev_del(&nd.cdv);
		unregister_chrdev_region(nd.devn, 1);
		already_init = 0;
		i2c_unregister_device(nd.i2c_client_atv);
		nd.i2c_client_atv = NULL;
	}

	func_exit();
}

module_init(nmi5625_init);
module_exit(nmi5625_clean);

MODULE_AUTHOR("nmi");
MODULE_DESCRIPTION("nmi TV 5625 driver");
MODULE_LICENSE("GPL");


/* ----------------------------------------------------------------------- *
 *
 This file created by albert RDA Inc
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/rtc.h>		/* get the user-level API */
#include <linux/bcd.h>
#include <linux/list.h>
#include <linux/delay.h>
#include <linux/nfs_fs.h>
#include <linux/nfs_fs_sb.h>
#include <linux/nfs_mount.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/tty.h>
#include <linux/syscalls.h>
#include <asm/termbits.h>
#include <linux/serial.h>
#include <linux/platform_device.h>
#include <linux/rfkill.h>
//#include <mach/iomap.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
//#include <linux/clk.h>
//#include <mach/rda_clk_name.h>
//#include <mach/board.h>
#include "rda_combo.h"
#include <mach/gpio.h>
//#include <mach/sys_config.h>
#include <mach/hardware.h>//WMT_MMAP_OFFSET
#include <linux/pwm.h> //for pwm
#include <mach/io.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <mach/wmt_iomux.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/spinlock.h>
void set_wifi_name(char * name);
static void enable_pwm1_32KHz(int enable);
static struct mutex i2c_rw_lock;
static struct rfkill *wlan_rfkill = NULL;
static struct rfkill *bt_rfkill = NULL;
static struct platform_device *platform_device;
static unsigned short wlan_version = 0;
static struct wake_lock rda_combo_wake_lock;
static struct delayed_work rda_combo_sleep_worker;
static struct work_struct rda_bt_interrupt; 
struct i2c_client *rda_wifi_core_client = NULL;
struct i2c_client *rda_wifi_rf_client = NULL;
struct i2c_client *rda_bt_core_client = NULL;
struct i2c_client *rda_bt_rf_client = NULL;
struct completion rda_wifi_bt_comp;
struct regulator *combo_reg;

static unsigned int bt_host_wake=9;
static unsigned int bt_host_wake_irq;

static u8 isBigEnded = 0;
static u8 wifi_in_test_mode = 0;
#ifdef RDA_KERNEL_PLATFORM
static u8 clock_mask_32k = 0;
static u8 clock_mask_26m = 0;
static u8 regulator_mask = 0;
static struct clk * clk32k = NULL;
static struct clk * clk26m = NULL;
#endif
void enable_26m_regulator(u8 mask)
{
	#ifdef RDA_KERNEL_PLATFORM
	if (regulator_mask & CLOCK_MASK_ALL) {
	} else {
		regulator_enable(combo_reg);
	}
	regulator_mask |= mask;
	#endif
	
}
void disable_26m_regulator(u8 mask)
{
     #ifdef RDA_KERNEL_PLATFORM
	if (regulator_mask & mask) {
		regulator_mask &= ~mask;
		if (regulator_mask & CLOCK_MASK_ALL) {
		} else {
			regulator_disable(combo_reg);
		}
	}
	 #endif
}
void enable_32k_rtc(u8 mask)
{
	#ifdef RDA_KERNEL_PLATFORM
	if (clock_mask_32k & CLOCK_MASK_ALL) {

	} else {
		clk_prepare_enable(clk32k);
	}
	clock_mask_32k |= mask;
	#endif
}

void disable_32k_rtc(u8 mask)
{
	#ifdef RDA_KERNEL_PLATFORM
	if (clock_mask_32k & mask) {
		clock_mask_32k &= ~mask;
		if (clock_mask_32k & CLOCK_MASK_ALL) {

		} else {
			clk_disable_unprepare(clk32k);
		}
	}
	#endif
}

void enable_26m_rtc(u8 mask)
{
    #ifdef RDA_KERNEL_PLATFORM
	if (clock_mask_26m & CLOCK_MASK_ALL) {

	} else {
		clk_prepare_enable(clk26m);
	}
	clock_mask_26m |= mask;
    #endif
}

void disable_26m_rtc(u8 mask)
{
	#ifdef RDA_KERNEL_PLATFORM
	if (clock_mask_26m & mask) {
		clock_mask_26m &= ~mask;
		if (clock_mask_26m & CLOCK_MASK_ALL) {

		} else {
			clk_disable_unprepare(clk26m);
		}
	}
	#endif
}

int i2c_write_1_addr_2_data(struct i2c_client *client, const u8 addr,
				const u16 data)
{
	unsigned char tmp_data[3];
	int ret = 0;
	int retry = 3;

	if (!isBigEnded) {
		tmp_data[0] = addr;
		tmp_data[1] = data >> 8;
		tmp_data[2] = data >> 0;
	} else {
		tmp_data[0] = addr;
		tmp_data[1] = data >> 0;
		tmp_data[2] = data >> 8;
	}

	while (retry--) {
		ret = i2c_master_send(client, (char *)tmp_data, 3);
		if (ret >= 0) {
			break;
		}
	}

	if (ret < 0) {
		printk(KERN_INFO
			   "***i2c_write_1_addr_2_data send:0x%X err:%d bigendia: %d \n",
			   addr, ret, isBigEnded);
		return -1;
	} else {
		return 0;
	}

}

int i2c_read_1_addr_2_data(struct i2c_client *client, const u8 addr, u16 * data)
{
	unsigned char tmp_data[2];
	int ret = 0;
	int retry = 3;

	while (retry--) {
		ret = i2c_master_send(client, (char *)&addr, 1);
		if (ret >= 0) {
			break;
		}
	}

	if (ret < 0) {
		printk(KERN_INFO "***i2c_read_1_addr_2_data send:0x%X err:%d\n",
			   addr, ret);
		return -1;
	}

	retry = 3;
	while (retry--) {
		ret = i2c_master_recv(client, tmp_data, 2);
		if (ret >= 0) {
			break;
		}
	}

	if (ret < 0) {
		printk(KERN_INFO "***i2c_read_1_addr_2_data send:0x%X err:%d\n",
			   addr, ret);
		return -1;
	}

	if (!isBigEnded) {
		*data = (tmp_data[0] << 8) | tmp_data[1];
	} else {
		*data = (tmp_data[1] << 8) | tmp_data[0];
	}
	return 0;
}

static void wlan_read_version_from_chip(void)
{
	int ret;
	u16 project_id = 0, chip_version = 0;

	if (wlan_version != 0 || !rda_wifi_rf_client)
		return;

	ret = i2c_write_1_addr_2_data(rda_wifi_rf_client, 0x3f, 0x0001);
	if (ret)
		goto err;

	ret = i2c_read_1_addr_2_data(rda_wifi_rf_client, 0x21, &chip_version);
	if (ret)
		goto err;

	ret = i2c_read_1_addr_2_data(rda_wifi_rf_client, 0x20, &project_id);
	if (ret)
		goto err;

	if (project_id == 0x5990) {
		if (chip_version == 0x47)
			wlan_version = WLAN_VERSION_90_D;
		else if (chip_version == 0x44 || chip_version == 0x45)
			wlan_version = WLAN_VERSION_90_E;
	} else if (project_id == 0x5991) {
	   	set_wifi_name("rda5991.ko");
		if (chip_version == 0x44)
			wlan_version = WLAN_VERSION_91;
		else if(chip_version == 0x45)
			wlan_version = WLAN_VERSION_91_E;
		else if(chip_version == 0x46)
			wlan_version = WLAN_VERSION_91_F;
	}

	printk("read project_id:%x version:%x wlan_version:%x \n", project_id,
		   chip_version, wlan_version);
err:
	ret = i2c_write_1_addr_2_data(rda_wifi_rf_client, 0x3f, 0x0000);
	return;

}

int rda_write_data_to_rf(struct i2c_client *client, const u16(*data)[2],
			 u32 count)
{
	int ret = 0;
	u32 i = 0;

	for (i = 0; i < count; i++) {
		if (data[i][0] == I2C_DELAY_FLAG) {
			msleep(data[i][2]);
			continue;
		}
		ret = i2c_write_1_addr_2_data(client, data[i][0], data[i][1]);
		if (ret < 0)
			break;
	}
	return ret;
}

u32 rda_wlan_version(void)
{
    if(wlan_version == 0)
        wlan_read_version_from_chip();
	return wlan_version;
}

static int rda_wifi_rf_probe(struct i2c_client *client,
				 const struct i2c_device_id *id)
{
	int result = 0;

	rda_wifi_rf_client = client;
	printk("rda_wifi_rf_probe \n");
	return result;
}

static int rda_wifi_rf_remove(struct i2c_client *client)
{
	return 0;
}

static int rda_wifi_rf_detect(struct i2c_client *client,
				  struct i2c_board_info *info)
{
	strcpy(info->type, RDA_WIFI_RF_I2C_DEVNAME);
	return 0;
}

static const struct i2c_device_id wifi_rf_i2c_id[] =
	{ {RDA_WIFI_RF_I2C_DEVNAME, RDA_I2C_CHANNEL}, {} };
static struct i2c_driver rda_wifi_rf_driver = {
	//.class = I2C_CLASS_HWMON,
	.probe = rda_wifi_rf_probe,
	.remove = rda_wifi_rf_remove,
	.detect = rda_wifi_rf_detect,
	.driver.name = RDA_WIFI_RF_I2C_DEVNAME,
	.id_table = wifi_rf_i2c_id,
};

static int rda_wifi_core_detect(struct i2c_client *client,
				struct i2c_board_info *info)
{
	strcpy(info->type, RDA_WIFI_CORE_I2C_DEVNAME);
	return 0;
}

static int rda_wifi_core_probe(struct i2c_client *client,
				   const struct i2c_device_id *id)
{
	int result = 0;

	rda_wifi_core_client = client;
	printk("rda_wifi_core_probe \n");
	return result;
}


static int rda_suspend(struct i2c_client *client, pm_message_t state)
{

	return 0;
}

static int rda_resume(struct i2c_client *client)
{
    enable_pwm1_32KHz(1);
	return 0;
}


static int rda_wifi_core_remove(struct i2c_client *client)
{
	return 0;
}

int rda_wifi_power_off(void)
{
	if (wlan_version == WLAN_VERSION_90_D
		|| wlan_version == WLAN_VERSION_90_E)
		return rda_5990_wifi_power_off();
	else if (wlan_version == WLAN_VERSION_91)
		return rda_5991_wifi_power_off();
	else if (wlan_version == WLAN_VERSION_91_E)
		return rda_5991e_wifi_power_off();
	else if (wlan_version == WLAN_VERSION_91_F)
		return rda_5991f_wifi_power_off();
	return 0;
}

int rda_wifi_power_on(void)
{
	wlan_read_version_from_chip();
	if (wlan_version == WLAN_VERSION_90_D
		|| wlan_version == WLAN_VERSION_90_E) {
		return rda_5990_wifi_power_on();
	} else if (wlan_version == WLAN_VERSION_91)
		return rda_5991_wifi_power_on();
	else if (wlan_version == WLAN_VERSION_91_E)
		return rda_5991e_wifi_power_on();
	else if (wlan_version == WLAN_VERSION_91_F)
		return rda_5991f_wifi_power_on();
	return 0;
}

static void rda_wifi_shutdown(struct i2c_client *client)
{
	printk("rda_wifi_shutdown \n");
	rda_wifi_power_off();
}

static const struct i2c_device_id wifi_core_i2c_id[] =
	{ {RDA_WIFI_CORE_I2C_DEVNAME, RDA_I2C_CHANNEL}, {} };
static struct i2c_driver rda_wifi_core_driver = {
	//.class = I2C_CLASS_HWMON,
	.probe = rda_wifi_core_probe,
	.remove = rda_wifi_core_remove,
	.detect = rda_wifi_core_detect,
	.shutdown = rda_wifi_shutdown,
	.suspend  = rda_suspend,
	.resume   = rda_resume,	
	.driver.name = RDA_WIFI_CORE_I2C_DEVNAME,
	.id_table = wifi_core_i2c_id,
};

static int rda_bt_rf_probe(struct i2c_client *client,
			   const struct i2c_device_id *id)
{
	int result = 0;

	rda_bt_rf_client = client;
	printk("rda_bt_rf_probe \n");
	return result;
}

static int rda_bt_rf_remove(struct i2c_client *client)
{
	rda_bt_rf_client = NULL;
	return 0;
}

static int rda_bt_rf_detect(struct i2c_client *client,
				struct i2c_board_info *info)
{
	strcpy(info->type, RDA_BT_RF_I2C_DEVNAME);
	return 0;
}

static const struct i2c_device_id bt_rf_i2c_id[] =
	{ {RDA_BT_RF_I2C_DEVNAME, RDA_I2C_CHANNEL}, {} };
static struct i2c_driver rda_bt_rf_driver = {
	//.class = I2C_CLASS_HWMON,
	.probe = rda_bt_rf_probe,
	.remove = rda_bt_rf_remove,
	.detect = rda_bt_rf_detect,
	.driver.name = RDA_BT_RF_I2C_DEVNAME,
	.id_table = bt_rf_i2c_id,
};

static int rda_bt_core_detect(struct i2c_client *client,
				  struct i2c_board_info *info)
{
	strcpy(info->type, RDA_BT_CORE_I2C_DEVNAME);
	return 0;
}

void rda_combo_set_wake_lock(void);

#ifdef CONFIG_BLUEZ_SUPPORT
extern void hci_bt_wakeup_host(void);
#endif


static irqreturn_t rda_bt_host_wake_eirq_handler(int irq, void *dev_id)
{

	int pin_state;
	
	if(!is_gpio_irqenable(bt_host_wake) || !gpio_irqstatus(bt_host_wake))
			return IRQ_NONE;			
	
	pin_state = gpio_direction_input(bt_host_wake);


	if(pin_state == 0)
	{
		printk("irq_handler\n");			


#ifdef CONFIG_BLUEZ_SUPPORT
    	hci_bt_wakeup_host();

#endif
    	//int *p=NULL,aaa;
    	//aaa=*p+3;
    	printk("rda_bt_host_wake_eirq_handler\n");
    	//rda_combo_set_wake_lock();
    	schedule_work(&rda_bt_interrupt);


        
	}
	else
	{
  		printk("fake interruption:%d\n",pin_state);
	}



	return IRQ_HANDLED;
}

int aaaa;
//tip,for different platform, you must check how to realize a interrupt.
int bt_register_host_wake_irq(void *dev)
{
	int ret = 0;
#ifdef ALLWINNER    
	script_item_u val;
	script_item_value_type_e type;

	type = script_get_item("wifi_para", "rda5990_bt_host_wake", &val);
	if (SCIRPT_ITEM_VALUE_TYPE_PIO!=type)
	{
                printk("get RDA rda5990_bt_host_wake gpio failed\n");
				goto err;

	}
    else
     bt_host_wake = val.gpio.gpio;

	bt_host_wake_irq = gpio_to_irq(bt_host_wake);
	if (IS_ERR_VALUE(bt_host_wake_irq)) {
			pr_warn("map gpio [%d] to virq failed, errno = %d\n", 
		         	bt_host_wake, bt_host_wake_irq);
			ret = -1;
		goto err;
		}
	ret = devm_request_irq(dev, bt_host_wake_irq, rda_bt_host_wake_eirq_handler, 
			       IRQF_TRIGGER_RISING, "rda_combo_bt", NULL);
	if (IS_ERR_VALUE(ret)) {
		pr_warn("request virq %d failed, errno = %d\n", 
		         bt_host_wake_irq, ret);
		goto err;
	}

	enable_irq(bt_host_wake_irq);

    
#endif    
    bt_host_wake_irq = IRQ_GPIO;
	wmt_gpio_mask_irq(bt_host_wake);
	wmt_gpio_set_irq_type(bt_host_wake,IRQ_TYPE_LEVEL_LOW);
	
	/*enable pull down*/
	wmt_gpio_setpull(bt_host_wake,WMT_GPIO_PULL_UP);

    ret = request_irq(bt_host_wake_irq, rda_bt_host_wake_eirq_handler, IRQF_SHARED, "rda_5991", (void*)&aaaa);
            if (ret){
                printk("Request IRQ failed!ERRNO:%d.", ret);
		        goto err;
            }

    

	return 0;
	err:
		return -1;
}

int bt_unregister_host_wake_irq(void *dev)
{
	if(bt_host_wake_irq > 0)
	{
	    //disable_irq_nosync(bt_host_wake_irq);
	  	//devm_free_irq(dev, bt_host_wake_irq, NULL);
	  	free_irq(IRQ_GPIO, (void*)&aaaa);
	  	bt_host_wake_irq = -1;
	}
	return 0;

}

//tip end


static int rda_bt_core_probe(struct i2c_client *client,
				 const struct i2c_device_id *id)
{
	int result = 0;
	rda_bt_core_client = client;
	printk("rda_bt_core_probe\n");
	return result;
}

static int rda_bt_core_remove(struct i2c_client *client)
{
	return 0;
}

static const struct i2c_device_id bt_core_i2c_id[] =
	{ {RDA_BT_CORE_I2C_DEVNAME, RDA_I2C_CHANNEL}, {} };
static struct i2c_driver rda_bt_core_driver = {
	//.class = I2C_CLASS_HWMON,
	.probe = rda_bt_core_probe,
	.remove = rda_bt_core_remove,
	.detect = rda_bt_core_detect,
	.driver.name = RDA_BT_CORE_I2C_DEVNAME,
	.id_table = bt_core_i2c_id,
};

#ifdef CONFIG_BT_RANDADDR
extern void bt_get_random_address(char *buf);
#endif


static int rda_combo_i2c_ops(unsigned long arg)
{
	int ret = 0, argc = 0;
	u8 cmd[256], *argv[5], *pos, rw = 0, addr = 0, pageup = 0;
	struct i2c_client * i2Client = NULL;
	u16  data = 0;
	void __user *argp = (void __user *)arg;

	if(copy_from_user(cmd, argp, 256))
		return -EFAULT;
	else{
		pos = cmd;
		while (*pos != '\0') {
				if (*pos == '\n') {
						*pos = '\0';
						break;
				}
				pos++;
		}
		argc = 0;
		pos = cmd;
		for (;;) {
		while (*pos == ' ')
			pos++;
			if (*pos == '\0')
				break;
			argv[argc] = pos;
			argc++;
			if (argc == 5)
				break;
			if (*pos == '"') {
				char *pos2 = strrchr(pos, '"');
				if (pos2)
					pos = pos2 + 1;
			}
			while (*pos != '\0' && *pos != ' ')
				pos++;
			if (*pos == ' ')
				*pos++ = '\0';
		}	
	}

	if(!memcmp(argv[1], "bt", 2)){
		i2Client = rda_bt_rf_client;
	}
	else
		i2Client = rda_wifi_rf_client;

	if (kstrtou8(argv[3], 0, &addr))
		return -EINVAL;
	
	if(*(argv[2]) == 'r'){
		rw = 0; 
	}
	else{
		rw = 1;
		if (kstrtou16(argv[4], 0, &data))
			return -EINVAL;
	}

	if(addr >= 0x80){
			i2c_write_1_addr_2_data(i2Client, 0x3F, 0x0001);
		addr -= 0x80;
		pageup = 1;
	}

	if(*(argv[2]) == 'r'){
		int read_data = 0;
		i2c_read_1_addr_2_data(i2Client, addr, &data);
		read_data = (int)data;

		if(copy_to_user(argp, &read_data, sizeof(int)))
			ret = -EFAULT;
	}
	else
		i2c_write_1_addr_2_data(i2Client, addr, data);

	if(pageup == 1)
		i2c_write_1_addr_2_data(i2Client, 0x3F, 0x0000);	

	printk("wlan: %s %s %s %s :0x%x \n", argv[0], argv[1], argv[2], argv[3], data);
	return ret;
}


static long rda_combo_pw_ioctl(struct file *file, unsigned int cmd,
				   unsigned long arg)
{
	int ret = 0;
	void __user *argp = (void __user *)arg;

	switch (cmd) {

	case RDA_WLAN_COMBO_VERSION:
		{
			u32 version = rda_wlan_version();
			if (copy_to_user(argp, &version, sizeof(version)))
				ret = -EFAULT;
		}
		break;

	case RDA_WIFI_POWER_SET_TEST_MODE_IOCTL:
		wifi_in_test_mode = 1;
		printk("****set rda wifi in test mode \n");
		break;

	case RDA_WIFI_POWER_CANCEL_TEST_MODE_IOCTL:
		wifi_in_test_mode = 0;
		printk("****set rda wifi in normal mode \n");
		break;

	case RDA_BT_GET_ADDRESS_IOCTL:
		{
			u8 bt_addr[6] = { 0 };

#ifdef CONFIG_BT_RANDADDR
			bt_get_random_address(bt_addr);
#endif
			printk(KERN_INFO
				   "rdabt address[0x%x]:[0x%x]:[0x%x]:[0x%x]:[0x%x]:[0x%x].\n",
				   bt_addr[0], bt_addr[1], bt_addr[2], bt_addr[3],
				   bt_addr[4], bt_addr[5]);
			//tmp for bt adr
			if (copy_to_user(argp, &bt_addr[0], sizeof(bt_addr))) {
				ret = -EFAULT;
			}
		}
		break;

	case RDA_COMBO_I2C_OPS:
		{
			ret = rda_combo_i2c_ops(arg);	
		}
		break;

	case RDA_WIFI_POWER_ON_IOCTL:
	case RDA_BT_POWER_ON_IOCTL:
		wlan_read_version_from_chip();
	default:
		if (wlan_version == WLAN_VERSION_90_D
			|| wlan_version == WLAN_VERSION_90_E) {
			ret = rda_5990_pw_ioctl(file, cmd, arg);
		} else if (wlan_version == WLAN_VERSION_91)
			ret = rda_5991_pw_ioctl(file, cmd, arg);
		else if (wlan_version == WLAN_VERSION_91_E)
			ret = rda_5991e_pw_ioctl(file, cmd, arg);
		else if (wlan_version == WLAN_VERSION_91_F)
			ret = rda_5991f_pw_ioctl(file, cmd, arg);
		break;
	}
	return ret;
}

static int rda_combo_major;
static struct class *rda_combo_class = NULL;
static const struct file_operations rda_combo_operations = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = rda_combo_pw_ioctl,
	.release = NULL
};

void rda_combo_sleep_worker_task(struct work_struct *work)
{
	printk("---rda_combo_sleep_worker_task end \n");
	wake_unlock(&rda_combo_wake_lock);
}
//add by LA
void rda_bt_interrupt_task(struct work_struct *work)
{
	printk("---rda_bt_interrupt_task  \n");
	rda_combo_set_wake_lock();
	printk("---rda_bt_interrupt_task end \n");

	
}
void rda_combo_set_wake_lock(void)
{
	printk("rda_combo_set_wake_lock\n");
	wake_lock(&rda_combo_wake_lock);
	cancel_delayed_work(&rda_combo_sleep_worker);
	schedule_delayed_work(&rda_combo_sleep_worker, 6 * HZ);
	printk("rda_combo_set_wake_lock end \n");
}

static struct platform_driver platform_driver = {
	.driver = {
		   .name = "rda_combo_rfkill_device",
		   .owner = THIS_MODULE,
		   }
};

static int wlan_rfkill_set(void *data, bool blocked)
{
	printk("wlan_rfkill_set %d \n", blocked);
	if (blocked) {
		rda_wifi_power_off();
	} else {
		rda_wifi_power_on();
	}
	return 0;
}

static int rda_bt_power_off(void)
{
	if (wlan_version == WLAN_VERSION_90_D
		|| wlan_version == WLAN_VERSION_90_E) {
		return rda_5990_bt_power_off();
	} else if (wlan_version == WLAN_VERSION_91)
		return rda_5991_bt_power_off();
	else if (wlan_version == WLAN_VERSION_91_E)
		return rda_5991e_bt_power_off();
	else if (wlan_version == WLAN_VERSION_91_F)
		return rda_5991f_bt_power_off();
	return 0;
}

static int rda_bt_power_on(void)
{
	wlan_read_version_from_chip();
	if (wlan_version == WLAN_VERSION_90_D
		|| wlan_version == WLAN_VERSION_90_E) {
		return rda_5990_bt_power_on();
	} else if (wlan_version == WLAN_VERSION_91)
		return rda_5991_bt_power_on();
	else if (wlan_version == WLAN_VERSION_91_E)
		return rda_5991e_bt_power_on();
	else if (wlan_version == WLAN_VERSION_91_F)
		return rda_5991f_bt_power_on();
	return 0;
}

static const struct rfkill_ops wlan_rfkill_ops = {
	.set_block = wlan_rfkill_set,
};

static int bt_rfkill_set(void *data, bool blocked)
{
	printk("bt_rfkill_set %d \n", blocked);
	if (blocked) {
		rda_bt_power_off();
	} else {
		rda_bt_power_on();
	}
	return 0;
}

static const struct rfkill_ops bt_rfkill_ops = {
	.set_block = bt_rfkill_set,
};




//configure pwm1 pin
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

struct pwm_device * g_pwm =NULL;

static void enable_pwm1_32KHz(int enable)
{
	if(enable) {
		//pwm_config(g_pwm, 15625, 31250);// configuration output 32KHZ
		pwm_config(g_pwm, 15258, 30517);// configuration output 32768HZ
		pwm_enable(g_pwm);
		config_pwm1_pin(0x01);
		printk("enable 32khz output\n");
	}else{
		pwm_disable(g_pwm);
		config_pwm1_pin(0x00);
		printk("disable 32khz output\n");
	}
}

static struct i2c_client *rda_wifi_core=NULL;
static struct i2c_client *rda_wifi_rf=NULL;
static struct i2c_client *rda_bt_core=NULL;
static struct i2c_client *rda_bt_rf=NULL;


#define RDA_WIFI_CORE_ADDR (0x13)
#define RDA_WIFI_RF_ADDR (0x14)
#define RDA_BT_CORE_ADDR (0x15)
#define RDA_BT_RF_ADDR (0x16)
#define RDA_WIFI_RF_I2C_DEVNAME "rda_wifi_rf_i2c"
#define RDA_WIFI_CORE_I2C_DEVNAME "rda_wifi_core_i2c"
#define RDA_BT_RF_I2C_DEVNAME "rda_bt_rf_i2c"
#define RDA_BT_CORE_I2C_DEVNAME "rda_bt_core_i2c"



struct i2c_board_info rda_wifi_core_i2c_board_info = {
	.type          = RDA_WIFI_CORE_I2C_DEVNAME,
	.flags         = 0x00,
	.addr          = RDA_WIFI_CORE_ADDR,
	.platform_data = NULL,
	.archdata      = NULL,
	.irq           = -1,
};
struct i2c_board_info rda_wifi_rf_i2c_board_info = {
	.type          = RDA_WIFI_RF_I2C_DEVNAME,
	.flags         = 0x00,
	.addr          = RDA_WIFI_RF_ADDR,
	.platform_data = NULL,
	.archdata      = NULL,
	.irq           = -1,
};
struct i2c_board_info rda_bt_core_i2c_board_info = {
	.type          = RDA_BT_CORE_I2C_DEVNAME,
	.flags         = 0x00,
	.addr          = RDA_BT_CORE_ADDR,
	.platform_data = NULL,
	.archdata      = NULL,
	.irq           = -1,
};
struct i2c_board_info rda_bt_rf_i2c_board_info = {
	.type          = RDA_BT_RF_I2C_DEVNAME,
	.flags         = 0x00,
	.addr          = RDA_BT_RF_ADDR,
	.platform_data = NULL,
	.archdata      = NULL,
	.irq           = -1,
};


static int rda_i2c_register_device (struct i2c_board_info *ts_i2c_bi,struct i2c_client **l_client)
{
	struct i2c_adapter *adapter = NULL;
	adapter = i2c_get_adapter(4);

	if (NULL == adapter) {
		printk("can not get i2c adapter, client address error\n");
		return -1;
	}

	*l_client = i2c_new_device(adapter, ts_i2c_bi);
	if (*l_client == NULL) {
		printk("allocate i2c client failed\n");
		return -1;
	}

	i2c_put_adapter(adapter);

	return 0;
}

static void rda_i2c_unregister_device(struct i2c_client *l_client)
{
	if (l_client != NULL)
	{
		i2c_unregister_device(l_client);
		l_client = NULL;
	}
}



int rda_combo_power_ctrl_init(void)
{
	int ret = 0;
	int n;

	ret = rda_i2c_register_device(&rda_wifi_core_i2c_board_info,&rda_wifi_core);
    if(ret!=0)
        return -1;
	rda_i2c_register_device(&rda_wifi_rf_i2c_board_info,&rda_wifi_rf);
	rda_i2c_register_device(&rda_bt_core_i2c_board_info,&rda_bt_core);
	rda_i2c_register_device(&rda_bt_rf_i2c_board_info,&rda_bt_rf);








    
	printk("begint to pwm request\n");
	g_pwm = pwm_request(0x01,"rda5991 bluetooth 32KHz");
	if(!g_pwm){
		printk("can not request pwm1 for bluetooth mtk6622\n");
		return -1;
	}

    
	if (gpio_request(bt_host_wake, "rda5991_intr") < 0) {
		printk("gpio(%d) rda5991_intr gpio request fail\n", bt_host_wake);
    	return -1;
	}	
    enable_pwm1_32KHz(1);








	printk("rda_combo_power_ctrl_init begin\n");
	if (i2c_add_driver(&rda_wifi_core_driver)) {
		printk("rda_wifi_core_driver failed!\n");
		ret = -ENODEV;
		return ret;
	}

	if (i2c_add_driver(&rda_wifi_rf_driver)) {
		printk("rda_wifi_rf_driver failed!\n");
		ret = -ENODEV;
		return ret;
	}

	if (i2c_add_driver(&rda_bt_core_driver)) {
		printk("rda_bt_core_driver failed!\n");
		ret = -ENODEV;
		return ret;
	}

	if (i2c_add_driver(&rda_bt_rf_driver)) {
		printk("rda_bt_rf_driver failed!\n");
		ret = -ENODEV;
		return ret;
	}

    rda_combo_major = XENVBD_MAJOR;


    n= register_chrdev(rda_combo_major, "rdacombo_power_ctrl", &rda_combo_operations);
	if (n < 0) {
		printk(KERN_INFO "register rdabt_power_ctrl failed!!! \n");
		return rda_combo_major;
	}else
	    printk("rda_combo_major %d\n",rda_combo_major);

	rda_combo_class = class_create(THIS_MODULE, "rda_combo");
	if (IS_ERR(rda_combo_class)) {
		unregister_chrdev(rda_combo_major, "rdacombo_power_ctrl");
		return PTR_ERR(rda_combo_class);
	}

	device_create(rda_combo_class, NULL, MKDEV(rda_combo_major, 0), NULL,
			  "rdacombo");
	#ifdef RDA_KERNEL_PLATFORM
	combo_reg = regulator_get(NULL, LDO_BT);
	if (IS_ERR(combo_reg)) {
		printk(KERN_INFO "could not find regulator devices\n");
		ret = PTR_ERR(combo_reg);
		goto fail_platform_driver;
	}
	#endif
	{
		unsigned char *temp = NULL;
		unsigned short testData = 0xffee;
		temp = (unsigned char *)&testData;
		if (*temp == 0xee)
			isBigEnded = 0;
		else
			isBigEnded = 1;
	}

	INIT_DELAYED_WORK(&rda_combo_sleep_worker, rda_combo_sleep_worker_task);
	wake_lock_init(&rda_combo_wake_lock, WAKE_LOCK_SUSPEND,
			   "RDA_sleep_worker_wake_lock");
   //add by RDA
   INIT_WORK(&rda_bt_interrupt,rda_bt_interrupt_task);

	mutex_init(&i2c_rw_lock);

	ret = platform_driver_register(&platform_driver);
	if (ret)
		goto fail_platform_driver;

	platform_device = platform_device_alloc("rda_combo_rfkill_device", -1);
	if (!platform_device) {
		ret = -ENOMEM;
	} else
		ret = platform_device_add(platform_device);

	if (ret)
		goto fail_platform_device;

	wlan_rfkill =
		rfkill_alloc("rda_wlan_rk", &platform_device->dev, RFKILL_TYPE_WLAN,
			 &wlan_rfkill_ops, NULL);
	if (wlan_rfkill) {
		rfkill_init_sw_state(wlan_rfkill, true);
		ret = rfkill_register(wlan_rfkill);
	}

	bt_rfkill =
		rfkill_alloc("rda_bt_rk", &platform_device->dev,
			 RFKILL_TYPE_BLUETOOTH, &bt_rfkill_ops, NULL);
	if (bt_rfkill) {
		rfkill_init_sw_state(bt_rfkill, true);
		ret = rfkill_register(bt_rfkill);
	} else
		printk("rda_bt_rk failed\n");
#if 0
//tip,for different platform, you must check how to realize a interrupt.
	type = script_get_item("wifi_para", "rda5990_bt_host_wake", &val);
	if (SCIRPT_ITEM_VALUE_TYPE_PIO!=type)
                printk("get RDA rda5990_bt_host_wake gpio failed\n");
        else
                bt_host_wake = val.gpio.gpio;

	bt_host_wake_irq = gpio_to_irq(bt_host_wake);
	if (IS_ERR_VALUE(bt_host_wake_irq)) {
			pr_warn("map gpio [%d] to virq failed, errno = %d\n", 
		         	bt_host_wake, bt_host_wake_irq);
			ret = -1;
		goto fail_platform_driver;
		}
	ret = devm_request_irq(&platform_device->dev, bt_host_wake_irq, rda_bt_host_wake_eirq_handler, 
			       IRQF_TRIGGER_RISING, "rda_combo_bt", NULL);
	if (IS_ERR_VALUE(ret)) {
		pr_warn("request virq %d failed, errno = %d\n", 
		         bt_host_wake_irq, ret);
		goto fail_platform_driver;
	}
//tip end
#else
  //tip,for different platform, you must check how to realize a interrupt.
  if(bt_register_host_wake_irq(&platform_device->dev) == -1)
  goto fail_platform_driver;
  //tip end

#endif
    #ifdef RDA_KERNEL_PLATFORM
	clk32k = clk_get(NULL, RDA_CLK_OUT);
	clk26m = clk_get(NULL, RDA_CLK_AUX);
	#endif
	init_completion(&rda_wifi_bt_comp);
	complete(&rda_wifi_bt_comp);

	printk("rda_combo_power_ctrl_init end\n");
	return 0;

fail_platform_driver:
fail_platform_device:
	unregister_chrdev(rda_combo_major, "rdacombo_power_ctrl");
	printk("rda_combo_power_ctrl_init failed\n");
	
	#ifdef RDA_KERNEL_PLATFORM
	if (!IS_ERR(combo_reg)) {
		regulator_put(combo_reg);
	}
	#endif
	return ret;
}

void rda_combo_power_ctrl_exit(void)
{
	i2c_del_driver(&rda_wifi_core_driver);
	i2c_del_driver(&rda_wifi_rf_driver);
	i2c_del_driver(&rda_bt_core_driver);
	i2c_del_driver(&rda_bt_rf_driver);

	unregister_chrdev(rda_combo_major, "rdacombo_power_ctrl");
	device_destroy(rda_combo_class, MKDEV(rda_combo_major, 0));
	if (rda_combo_class)
		class_destroy(rda_combo_class);

	cancel_delayed_work_sync(&rda_combo_sleep_worker);
	cancel_work_sync(&rda_bt_interrupt);
	wake_lock_destroy(&rda_combo_wake_lock);
	disable_32k_rtc(CLOCK_MASK_ALL);
	disable_26m_rtc(CLOCK_MASK_ALL);
    #ifdef RDA_KERNEL_PLATFORM
	clk_put(clk32k);
	clk_put(clk26m);
    #endif
	if (wlan_rfkill) {
		rfkill_unregister(wlan_rfkill);
		rfkill_destroy(wlan_rfkill);
	}

	if (bt_rfkill) {
		rfkill_unregister(bt_rfkill);
		rfkill_destroy(bt_rfkill);
	}

	#if 0
	devm_free_irq(&platform_device->dev, bt_host_wake_irq, NULL);
	bt_host_wake_irq = -1;
	#else
	bt_unregister_host_wake_irq(&platform_device->dev);
	#endif

	if (platform_device) {
		platform_device_unregister(platform_device);
		platform_driver_unregister(&platform_driver);
	}
	#ifdef RDA_KERNEL_PLATFORM
	if (!IS_ERR(combo_reg)) {
			regulator_disable(combo_reg);
			regulator_put(combo_reg);
	}
	#endif

    rda_i2c_unregister_device(rda_wifi_core);
    rda_i2c_unregister_device(rda_wifi_rf);
    rda_i2c_unregister_device(rda_bt_core);
    rda_i2c_unregister_device(rda_bt_rf);

    enable_pwm1_32KHz(0);
	if(g_pwm)
		pwm_free(g_pwm);
	gpio_free(bt_host_wake);

    
}

unsigned char rda_combo_wifi_in_test_mode(void)
{
	return wifi_in_test_mode;
}

void rda_combo_i2c_lock(void)
{
	mutex_lock(&i2c_rw_lock);
}

void rda_combo_i2c_unlock(void)
{
	mutex_unlock(&i2c_rw_lock);
}

int rda_fm_power_on(void)
{
	int ret = 0;
	wlan_read_version_from_chip();
	if (wlan_version == WLAN_VERSION_90_D
		|| wlan_version == WLAN_VERSION_90_E) {
		ret = rda_5990_fm_power_on();
	} else if (wlan_version == WLAN_VERSION_91)
		ret = rda_5991_fm_power_on();
	else if (wlan_version == WLAN_VERSION_91_E)
		ret = rda_5991e_fm_power_on();
	else if (wlan_version == WLAN_VERSION_91_F)
		ret = rda_5991f_fm_power_on();
	return ret;
}

int rda_fm_power_off(void)
{
	int ret = 0;
	wlan_read_version_from_chip();
	if (wlan_version == WLAN_VERSION_90_D
		|| wlan_version == WLAN_VERSION_90_E) {
		ret = rda_5990_fm_power_off();
	} else if (wlan_version == WLAN_VERSION_91)
		ret = rda_5991_fm_power_off();
	else if (wlan_version == WLAN_VERSION_91_E)
		ret = rda_5991e_fm_power_off();
	else if (wlan_version == WLAN_VERSION_91_F)
		ret = rda_5991f_fm_power_off();
	return ret;

}

EXPORT_SYMBOL(rda_wlan_version);

EXPORT_SYMBOL(rda_combo_wifi_in_test_mode);

EXPORT_SYMBOL(rda_combo_set_wake_lock);
EXPORT_SYMBOL(rda_wifi_power_off);
EXPORT_SYMBOL(rda_wifi_power_on);
EXPORT_SYMBOL(rda_fm_power_on);
EXPORT_SYMBOL(rda_fm_power_off);

module_init(rda_combo_power_ctrl_init);
module_exit(rda_combo_power_ctrl_exit);

MODULE_LICENSE("GPL");

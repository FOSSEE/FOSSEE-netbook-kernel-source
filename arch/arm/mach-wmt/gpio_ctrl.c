/*
 *  procfs1.c -  create a "file" in /proc
 *
 */
#include <linux/module.h>	/* Specifically, a module */
#include <linux/kernel.h>	/* We're doing kernel work */
#include <linux/proc_fs.h>	/* Necessary because we use the proc fs */
#include <linux/gpio.h>
#include <mach/wmt_iomux.h>
#include <asm/uaccess.h>   //for copy_from_user
#include <linux/delay.h> // for mdelay

#define DRIVER_AUTHOR		"rubbitxiao "
#define DRIVER_DESC		"printer power control"


#define procfs_name "gpio_ctrl"

extern int wmt_getsyspara(char *varname, unsigned char *varval, int *varlen);
extern struct proc_dir_entry proc_root;
/**
 * This structure hold information about the /proc file
 *
 */
static struct proc_dir_entry *Our_Proc_File;

int power_on = -1;  //control printer power on/off
int lid_stat = -1;  //inflext printer lid open or close status

enum{
	LOW=0,
	HIGH,
};

/* Put data into the proc fs file.
 * 
 * Arguments
 * =========
 * 1. The buffer where the data is to be inserted, if
 *    you decide to use it.
 * 2. A pointer to a pointer to characters. This is
 *    useful if you don't want to use the buffer
 *    allocated by the kernel.
 * 3. The current position in the file
 * 4. The size of the buffer in the first argument.
 * 5. Write a "1" here to indicate EOF.
 * 6. A pointer to data (useful in case one common 
 *    read for multiple /proc/... entries)
 *
 * Usage and Return Value
 * ======================
 * A return value of zero means you have no further
 * information at this time (end of file). A negative
 * return value is an error condition.
 *
 * For More Information
 * ====================
 * The way I discovered what to do with this function
 * wasn't by reading documentation, but by reading the
 * code which used it. I just looked to see what uses
 * the get_info field of proc_dir_entry struct (I used a
 * combination of find and grep, if you're interested),
 * and I saw that  it is used in <kernel source
 * directory>/fs/proc/array.c.
 *
 * If something is unknown about the kernel, this is
 * usually the way to go. In Linux we have the great
 * advantage of having the kernel source code for
 * free - use it.
 */
static int
procfile_read(char *buffer,
	      char **buffer_location,
	      off_t offset, int buffer_length, int *eof, void *data)
{
	int ret;
	int value = -1;
	
	if (offset > 0) {
		/* we have finished to read, return 0 */
		ret  = 0;
	} else {
		if(lid_stat >=0){
			gpio_direction_input(lid_stat);
			value = __gpio_get_value(lid_stat);
			/* fill the buffer, return the buffer size */
			ret = sprintf(buffer,"%d",value);
		}else{
			printk("err! have not set wmt.gpo.printer uboot variant\n");
			ret = -1;
		}
	}

	return ret;
}


static int procfile_write(struct file *file, const char __user *buffer,
			   unsigned long count, void *data)
{
	int ret = 0;
	char tmp[128];
	int num;

	int on = 0;
	
	if(buffer && !copy_from_user(tmp, buffer, sizeof(tmp))) {

		num = sscanf(tmp, "%d", &on);				
		printk("your input power on/off:%d\n",on);
		if(power_on<0){
			printk("err! have not set wmt.gpo.printer uboot variant\n");
			return -1;
		}
		if(on){	
			gpio_direction_output(power_on, HIGH);
			mdelay(200);
			printk("power on printer\n");
		}else{
			gpio_direction_output(power_on, LOW);
			printk("power off printer\n");
		}	
				
		return strlen(tmp);
	}else{
		printk("copy_from_user failed or buffer is null\n");
		return -1;
	}
}	


static int __init wifi_proc_init(void)
{
	int retval = 0;
	int varlen = 127;                                                                                                       
    char buf[200]={0};      

	Our_Proc_File = create_proc_entry(procfs_name, 0644, NULL);
	
	if (Our_Proc_File == NULL) {
		remove_proc_entry(procfs_name, NULL);
		printk(KERN_ALERT "Error: Could not initialize /proc/%s\n",
		       procfs_name);
		return -ENOMEM;
	}
	/*
	*  wmt.gpo.printer format as follows:
	*   power_on:lid_stat
	*  for example: setenv wmt.gpo.printer 153:3
	*    gpio8 for power on; gpio9 for lid status
	*/
	retval = wmt_getsyspara("wmt.gpo.printer", buf, &varlen);                   
	if(!retval)                                                              
	{                                                                          
		sscanf(buf, "%d:%d",&power_on,&lid_stat);    			                                                			                                                                                                                                               
		printk("power_on:%d,lid_stat:%d\n", power_on,lid_stat);    
		//request gpio for printer power control
		retval = gpio_request(power_on, "printer power pin");
		if(retval < 0) {
			printk("reques gpio:%x failed!!! for printer power pin\n",power_on);
			return -1;
		}else{
			printk("request gpio:%d for printer power pin success!!!\n", power_on);			
		}
		//request gpio for printer lid status
		retval = gpio_request(lid_stat, "printer lid status");
		if(retval < 0) {
			printk("reques gpio:%x failed!!! for printer lid status\n",lid_stat);
			return -1;
		}else{
			printk("request gpio:%d for printer lid status success!!!\n", lid_stat);			
		}
	}else{
		printk("have not set wmt.gpo.printer");
	}
	
	Our_Proc_File->read_proc = procfile_read;
	Our_Proc_File->write_proc = procfile_write;
	Our_Proc_File->mode 	 = S_IFREG | S_IRUGO;
	Our_Proc_File->uid 	 = 0;
	Our_Proc_File->gid 	 = 0;
	Our_Proc_File->size 	 = 37;

	return 0;	/* everything is ok */
}

static void __exit wifi_proc_uninit(void)
{
	remove_proc_entry(procfs_name, NULL);
	if(power_on)
		gpio_free(power_on);
	if(lid_stat)
		gpio_free(lid_stat);
}


module_init(wifi_proc_init);
module_exit(wifi_proc_uninit);
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");

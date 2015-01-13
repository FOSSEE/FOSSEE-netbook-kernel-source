/*
 * Copyright 2012 WonderMedia Technologies, Inc. All Rights Reserved. 
 *  
 * This PROPRIETARY SOFTWARE is the property of WonderMedia Technologies, Inc. 
 * and may contain trade secrets and/or other confidential information of 
 * WonderMedia Technologies, Inc. This file shall not be disclosed to any third party, 
 * in whole or in part, without prior written consent of WonderMedia. 
 *  
 * THIS PROPRIETARY SOFTWARE AND ANY RELATED DOCUMENTATION ARE PROVIDED AS IS, 
 * WITH ALL FAULTS, AND WITHOUT WARRANTY OF ANY KIND EITHER EXPRESS OR IMPLIED, 
 * AND WonderMedia TECHNOLOGIES, INC. DISCLAIMS ALL EXPRESS OR IMPLIED WARRANTIES 
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.  
 */

#include <linux/module.h>	/* Specifically, a module */
#include <linux/kernel.h>	/* We're doing kernel work */
#include <linux/proc_fs.h>	/* Necessary because we use the proc fs */
#include <linux/uaccess.h>  //for copy_from_user
#include <linux/pwm.h>
#include <mach/hardware.h>//WMT_MMAP_OFFSET


#define procfs_name "bt_test"


int skip = 0x00;

extern struct proc_dir_entry proc_root;

//extern void mt_bt_power_off(void);
//extern int mt_bt_power_on(void);
/**
 * This structure hold information about the /proc file
 *
 */
struct proc_dir_entry *Our_Proc_test;

extern void dump_uart_info(void);
static int procfile_read(char *buffer,
	      char **buffer_location,
	      off_t offset, int buffer_length, int *eof, void *data)
{
	int ret = 0;
	
	
	/* 
	 * We give all of our information in one go, so if the
	 * user asks us if we have more information the
	 * answer should always be no.
	 *
	 * This is important because the standard read
	 * function from the library would continue to issue
	 * the read system call until the kernel replies
	 * that it has no more information, or until its
	 * buffer is filled.
	 */
	printk("enter dump\n");
	dump_uart_info();
	return ret;
}
//configure pwm1 pin
void config_pwm1_pin1(int enable)
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

static struct pwm_device * g_pwm1 =NULL;

void enable_pwm1_32KHz1(int enable)
{
	static int first = 0x01;
	if(first) {
		first = 0x00;
		printk("begint ot pwm request\n");
		g_pwm1 = pwm_request(0x01,"mtk6622 bluetooth 32KHz");
		if(!g_pwm1){
			printk("can not request pwm1 for bluetooth mtk6622\n");
			return;
		}
	}
	if(enable) {
		pwm_config(g_pwm1, 15625, 31250);
		pwm_enable(g_pwm1);
		printk("enable 32khz output\n");
	}else{
		pwm_disable(g_pwm1);
		printk("disable 32khz output\n");
	}
}
/*
 *this funciton just for facilitate debugging
 *command formater: echo "digital strings" > /proc/usb_serail
 * setenv wmt.6620.pmu "e3:3:D8110040:D8110080:D81100c0:D81100c4:D81100c8"
 * saveenv
*/
static int procfile_write(struct file *file, const char __user *buffer,
			   unsigned long count, void *data)
{
	char tmp[128];
	char printbuf[128];
	int num;
	int option;

	
	if(buffer && !copy_from_user(tmp, buffer, sizeof(tmp))) {

		num = sscanf(tmp, "%d", &option);				
		printk("your input: option:%d\n",option);

		
		switch(option) {
		case 0:  //power off bluetooth
			//mt_bt_power_off();
		  break;
		case 1:  //power on bluetooth chip
			//mt_bt_power_on();
		  break;
		case 2:  //open bluetooth interruption
			//mt_bt_enable_irq();
			break;
		case 3: //close bluetooth interruption
			//mt_bt_disable_irq();
			break;
		case 4: //enable 32KHz
			//config_pwm1_pin1(1);
			//enable_pwm1_32KHz1(1);
			pwm_32KHZ_control(1);
			break;
		case 5: //disable 32KHz
			//enable_pwm1_32KHz1(0);
			pwm_32KHZ_control(0);
			break;			
		default:
			printk("test command,for example: echo 1  > /proc/bt_test\n");
		}
		return strlen(tmp);
	}else{
		printk("copy_from_user failed or buffer is null\n");
	}
}			   

int test_skeleton_proc_init(void)
{
	printk("enter test_skeleton_proc_init\n");
	Our_Proc_test = create_proc_entry(procfs_name, 0644, NULL);
	
	if (Our_Proc_test == NULL) {
		//remove_proc_entry(procfs_name, &proc_root);
		printk(KERN_ALERT "Error: Could not initialize /proc/%s\n",
		       procfs_name);
		return -ENOMEM;
	}

	Our_Proc_test->read_proc = procfile_read;
	Our_Proc_test->write_proc = procfile_write;
//	//Our_Proc_test->owner 	 = THIS_MODULE;
//	Our_Proc_test->mode 	 = S_IFREG | S_IRUGO;
//	Our_Proc_test->uid 	 = 0;
//	Our_Proc_test->gid 	 = 0;
//	Our_Proc_test->size 	 = 37;   //size what is its mean ?

	return 0;	/* everything is ok */
}
int deinit_proc_init(void)
{
	if(Our_Proc_test != NULL){
        remove_proc_entry(procfs_name,NULL);
        Our_Proc_test = NULL;
    }
		return 0;
}

EXPORT_SYMBOL(test_skeleton_proc_init);

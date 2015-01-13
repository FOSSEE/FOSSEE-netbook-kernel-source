/*++
 
 Copyright (c) 2012-2022 ChipOne Technology (Beijing) Co., Ltd. All Rights Reserved.
 This PROPRIETARY SOFTWARE is the property of ChipOne Technology (Beijing) Co., Ltd. 
 and may contains trade secrets and/or other confidential information of ChipOne 
 Technology (Beijing) Co., Ltd. This file shall not be disclosed to any third party,
 in whole or in part, without prior written consent of ChipOne.  
 THIS PROPRIETARY SOFTWARE & ANY RELATED DOCUMENTATION ARE PROVIDED AS IS, 
 WITH ALL FAULTS, & WITHOUT WARRANTY OF ANY KIND. CHIPONE DISCLAIMS ALL EXPRESS OR 
 IMPLIED WARRANTIES.  
 
 File Name:    icn85xx.c
 Abstract:
               input driver.
 Author:       Zhimin Tian
 Date :        08,14,2013
 Version:      1.0
 History :
     2012,10,30, V0.1 first version  
 --*/

#include "icn85xx.h"
#include "icn85xx_fw.h"
//#include "icn85xx_00_ht_0528.h"
//#include "icn85xx_02_lh_0528.h"

#if COMPILE_FW_WITH_DRIVER
       static char firmware[128] = "icn85xx_firmware";
#else
    #if SUPPORT_SENSOR_ID 
        static char firmware[128] = {0};
	#else
       //static char firmware[128] = {"/misc/modules/ICN8505.BIN"};
       //static char firmware[128] = {"/system/etc/firmware/ICN8505.bin"};
       static char firmware[128] = {"ICN8505.bin"};
    #endif
#endif

#if SUPPORT_SENSOR_ID
   char cursensor_id,tarsensor_id,id_match;
   char invalid_id = 0;

   struct sensor_id {
				char value;
				const char bin_name[128];
			    unsigned char *fw_name;
				int size;
		};

static struct sensor_id sensor_id_table[] = {
	               { 0x00, "/misc/modules/ICN8505_00_name1.BIN",fw_00_ht_0528,sizeof(fw_00_ht_0528)},//default bin or fw
                   { 0x02, "/misc/modules/ICN8505_02_name3.BIN",fw_02_lh_0528,sizeof(fw_02_lh_0528)},
                   
				 // if you want support other sensor id value ,please add here
		         };
#endif
struct i2c_client *this_client;
short log_basedata[COL_NUM][ROW_NUM] = {{0,0}};
short log_rawdata[COL_NUM][ROW_NUM] = {{0,0}};
short log_diffdata[COL_NUM][ROW_NUM] = {{0,0}};
unsigned int log_on_off = 0;
static struct touch_param g_param;
static struct wake_lock downloadWakeLock;
static struct task_struct *resume_download_task;
static int bl_is_delay = 0;

extern int wmt_getsyspara(char *varname, unsigned char *varval, int *varlen);
 //yank added
 void icn85xx_charge_mode(void)
 {
       printk("yank---%s\n",__func__);
       icn85xx_write_reg(ICN85xx_REG_PMODE, 0x55);
 }
 EXPORT_SYMBOL(icn85xx_charge_mode);

 void icn85xx_discharge_mode(void)
 {
 printk("yank---%s\n",__func__);
      icn85xx_write_reg(ICN85xx_REG_PMODE, 0x66);
 }
 EXPORT_SYMBOL(icn85xx_discharge_mode);


static enum hrtimer_restart chipone_timer_func(struct hrtimer *timer);
#if SUPPORT_SYSFS
static ssize_t icn85xx_show_update(struct device* cd,struct device_attribute *attr, char* buf);
static ssize_t icn85xx_store_update(struct device* cd, struct device_attribute *attr, const char* buf, size_t len);
static ssize_t icn85xx_show_process(struct device* cd,struct device_attribute *attr, char* buf);
static ssize_t icn85xx_store_process(struct device* cd, struct device_attribute *attr,const char* buf, size_t len);

static DEVICE_ATTR(update, S_IRUGO | S_IWUSR, icn85xx_show_update, icn85xx_store_update);
static DEVICE_ATTR(process, S_IRUGO | S_IWUSR, icn85xx_show_process, icn85xx_store_process);

static ssize_t icn85xx_show_process(struct device* cd,struct device_attribute *attr, char* buf)
{
    ssize_t ret = 0;
    sprintf(buf, "icn85xx process\n");
    ret = strlen(buf) + 1;
    return ret;
}

static ssize_t icn85xx_store_process(struct device* cd, struct device_attribute *attr,
               const char* buf, size_t len)
{
    struct icn85xx_ts_data *icn85xx_ts = i2c_get_clientdata(this_client);
    unsigned long on_off = simple_strtoul(buf, NULL, 10);     

    log_on_off = on_off;
    memset(&log_basedata[0][0], 0, COL_NUM*ROW_NUM*2);
    if(on_off == 0)
    {
        icn85xx_ts->work_mode = 0;
    }
    else if((on_off == 1) || (on_off == 2) || (on_off == 3))
    {
        if((icn85xx_ts->work_mode == 0) && (icn85xx_ts->use_irq == 1))
        {
            hrtimer_init(&icn85xx_ts->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
            icn85xx_ts->timer.function = chipone_timer_func;
            hrtimer_start(&icn85xx_ts->timer, ktime_set(CTP_START_TIMER/1000, (CTP_START_TIMER%1000)*1000000), HRTIMER_MODE_REL);
        }
        icn85xx_ts->work_mode = on_off;
    }
    else if(on_off == 10)
    {
        icn85xx_ts->work_mode = 4;
        mdelay(10);
        printk("update baseline\n");
        icn85xx_write_reg(4, 0x30); 
        icn85xx_ts->work_mode = 0;
    }
    else
    {
        icn85xx_ts->work_mode = 0;
    }
    
    
    return len;
}

static ssize_t icn85xx_show_update(struct device* cd,
                     struct device_attribute *attr, char* buf)
{
    ssize_t ret = 0;       
    sprintf(buf, firmware);
    ret = strlen(buf) + 1;
    printk("firmware: %s, ret: %d\n", firmware, ret);  

    return ret;
}

static ssize_t icn85xx_store_update(struct device* cd, struct device_attribute *attr, const char* buf, size_t len)
{
    printk("len: %d, update: %s\n", len, buf);  
    memset(firmware, 0, 128);
    memcpy(firmware, buf, len-1);
    if(R_OK == icn85xx_fw_update(firmware))
    {
        printk("update ok\n");
    }
    else
    {
        printk("update error\n");   
    }    
    return len;
}

static int icn85xx_create_sysfs(struct i2c_client *client)
{
    int err;
    struct device *dev = &(client->dev);
    icn85xx_trace("%s: \n",__func__);    
    err = device_create_file(dev, &dev_attr_update);
    err = device_create_file(dev, &dev_attr_process);
    return err;
}

static void icn85xx_remove_sysfs(struct i2c_client *client)
{
    struct device *dev = &(client->dev);
    icn85xx_trace("%s: \n",__func__);    
    device_remove_file(dev, &dev_attr_update);
    device_remove_file(dev, &dev_attr_process);
}
#endif

#if SUPPORT_PROC_FS

pack_head cmd_head;
static struct proc_dir_entry *icn85xx_proc_entry;
int  DATA_LENGTH = 0;

STRUCT_PANEL_PARA_H g_structPanelPara;

static int icn85xx_tool_write(struct file *filp, const char __user *buff, unsigned long len, void *data)
{
    int ret = 0;
    int i;
    unsigned short addr;
    char retvalue;
    struct icn85xx_ts_data *icn85xx_ts = i2c_get_clientdata(this_client);
    proc_info("%s \n",__func__);  
    if(down_interruptible(&icn85xx_ts->sem))  
    {  
        return -1;   
    }     
    ret = copy_from_user(&cmd_head, buff, CMD_HEAD_LENGTH);
    if(ret)
    {
        proc_error("copy_from_user failed.\n");
        goto write_out;
    }  
    else
    {
        ret = CMD_HEAD_LENGTH;
    }
    
    proc_info("wr  :0x%02x.\n", cmd_head.wr);
    proc_info("flag:0x%02x.\n", cmd_head.flag);
    proc_info("circle  :%d.\n", (int)cmd_head.circle);
    proc_info("times   :%d.\n", (int)cmd_head.times);
    proc_info("retry   :%d.\n", (int)cmd_head.retry);
    proc_info("data len:%d.\n", (int)cmd_head.data_len);
    proc_info("addr len:%d.\n", (int)cmd_head.addr_len);
    proc_info("addr:0x%02x%02x.\n", cmd_head.addr[0], cmd_head.addr[1]);
    proc_info("len:%d.\n", (int)len);
    proc_info("data:0x%02x%02x.\n", buff[CMD_HEAD_LENGTH], buff[CMD_HEAD_LENGTH+1]);
    if (1 == cmd_head.wr)  // write para
    {
        ret = copy_from_user(&cmd_head.data[0], &buff[CMD_HEAD_LENGTH], cmd_head.data_len);
        if(ret)
        {
            proc_error("copy_from_user failed.\n");
            goto write_out;
        }
        //need copy to g_structPanelPara

        memcpy(&g_structPanelPara, &cmd_head.data[0], cmd_head.data_len);
        //write para to tp
        for(i=0; i<cmd_head.data_len; )
        {
        	int size = ((i+64) > cmd_head.data_len)?(cmd_head.data_len-i):64;
		    ret = icn85xx_i2c_txdata(0x8000+i, &cmd_head.data[i], size);
		    if (ret < 0) {
		        proc_error("write para failed!\n");
		        goto write_out;
		    }	
		    i = i + 64;	    
		}
		
        ret = cmd_head.data_len + CMD_HEAD_LENGTH;
        icn85xx_ts->work_mode = 5; //reinit
        printk("reinit tp\n");
        icn85xx_write_reg(0, 1); 
        mdelay(100);
        icn85xx_write_reg(0, 0);            
        icn85xx_ts->work_mode = 0;
        goto write_out;

    }
    else if(3 == cmd_head.wr)   //set update file
    {
        proc_info("cmd_head_.wr == 3  \n");
		ret = copy_from_user(&cmd_head.data[0], &buff[CMD_HEAD_LENGTH], cmd_head.data_len);
        if(ret)
        {
            proc_error("copy_from_user failed.\n");
            goto write_out;
        }
        ret = cmd_head.data_len + CMD_HEAD_LENGTH;
        memset(firmware, 0, 128);
        memcpy(firmware, &cmd_head.data[0], cmd_head.data_len);
        proc_info("firmware : %s\n", firmware);
    }
    else if(5 == cmd_head.wr)  //start update 
    {        
        proc_info("cmd_head_.wr == 5 \n");
		icn85xx_update_status(1); 
         ret = kernel_thread(icn85xx_fw_update,firmware,CLONE_KERNEL);
        icn85xx_trace("the kernel_thread result is:%d\n", ret);    
    }
    else if(11 == cmd_head.wr) //write hostcomm
    { 
        ret = copy_from_user(&cmd_head.data[0], &buff[CMD_HEAD_LENGTH], cmd_head.data_len);
        if(ret)
        {
            proc_error("copy_from_user failed.\n");
            goto write_out;
        }   	    	
        addr = (cmd_head.addr[1]<<8) | cmd_head.addr[0];
        icn85xx_write_reg(addr, cmd_head.data[0]);        
    }
    else if(13 == cmd_head.wr) //adc enable
    { 
       proc_info("cmd_head_.wr == 13  \n");
	    icn85xx_ts->work_mode = 4;
        mdelay(10);
        //set col
        icn85xx_write_reg(0x8000+STRUCT_OFFSET(STRUCT_PANEL_PARA_H, u8ColNum), 1); 
        //u8RXOrder[0] = u8RXOrder[cmd_head.addr[0]];
        icn85xx_write_reg(0x8000+STRUCT_OFFSET(STRUCT_PANEL_PARA_H, u8RXOrder[0]), g_structPanelPara.u8RXOrder[cmd_head.addr[0]]); 
        //set row
        icn85xx_write_reg(0x8000+STRUCT_OFFSET(STRUCT_PANEL_PARA_H, u8RowNum), 1);
        //u8TXOrder[0] = u8TXOrder[cmd_head.addr[1]];        
        icn85xx_write_reg(0x8000+STRUCT_OFFSET(STRUCT_PANEL_PARA_H, u8TXOrder[0]), g_structPanelPara.u8TXOrder[cmd_head.addr[1]]); 
        //scan mode
        icn85xx_write_reg(0x8000+STRUCT_OFFSET(STRUCT_PANEL_PARA_H, u8ScanMode), 0);
        //bit
        icn85xx_write_reg(0x8000+STRUCT_OFFSET(STRUCT_PANEL_PARA_H, u16BitFreq), 0xD0);
        icn85xx_write_reg(0x8000+STRUCT_OFFSET(STRUCT_PANEL_PARA_H, u16BitFreq)+1, 0x07);
        //freq
        icn85xx_write_reg(0x8000+STRUCT_OFFSET(STRUCT_PANEL_PARA_H, u16FreqCycleNum[0]), 0x64);
        icn85xx_write_reg(0x8000+STRUCT_OFFSET(STRUCT_PANEL_PARA_H, u16FreqCycleNum[0])+1, 0x00);
        //pga
        icn85xx_write_reg(0x8000+STRUCT_OFFSET(STRUCT_PANEL_PARA_H, u8PgaGain), 0x0);

        //config mode
        icn85xx_write_reg(0, 0x2);
        
        mdelay(1);
        icn85xx_read_reg(2, &retvalue);
        printk("retvalue0: %d\n", retvalue);
        while(retvalue != 1)
        {   
            printk("retvalue: %d\n", retvalue);
            mdelay(1);
            icn85xx_read_reg(2, &retvalue);
        }   

        if(icn85xx_goto_progmode() != 0)
        {
            printk("icn85xx_goto_progmode() != 0 error\n");
            goto write_out;
        }
        
        icn85xx_prog_write_reg(0x040870, 1);   

    }

write_out:
    up(&icn85xx_ts->sem); 
	proc_info("icn85xx_tool_write write_out  \n");
    return len;
    
}

static int icn85xx_tool_read( char *page, char **start, off_t off, int count, int *eof, void *data )
{
    int i, j;
    int ret = 0;

    char row, column, retvalue;
    unsigned short addr;
    struct icn85xx_ts_data *icn85xx_ts = i2c_get_clientdata(this_client);
    if(down_interruptible(&icn85xx_ts->sem))  
    {  
        return -1;   
    }     
    proc_info("%s: count:%d, off:%d, cmd_head.data_len: %d\n",__func__, count,(int)off,(int)cmd_head.data_len); 
    if (cmd_head.wr % 2)
    {
        ret = 0;
		proc_info("cmd_head_.wr == 1111111  \n");
        goto read_out;
    }
    else if (0 == cmd_head.wr)   //read para
    {
        //read para
		proc_info("cmd_head_.wr == 0  \n");
        ret = icn85xx_i2c_rxdata(0x8000, &page[0], cmd_head.data_len);
        if (ret < 0) {
            icn85xx_error("%s read_data i2c_rxdata failed: %d\n", __func__, ret);
        }
        memcpy(&g_structPanelPara, &page[0], sizeof(g_structPanelPara));
        goto read_out;

    }
    else if(2 == cmd_head.wr)  //get update status
    {
        proc_info("cmd_head_.wr == 2  \n");
		page[0] = icn85xx_get_status();
        proc_info("status: %d\n", page[0]); 
    }
    else if(4 == cmd_head.wr)  //read rawdata
    {
        //icn85xx_read_reg(0x8004, &row);
        //icn85xx_read_reg(0x8005, &column);   
		proc_info("cmd_head_.wr == 4  \n");     
        row = cmd_head.addr[1];
        column = cmd_head.addr[0];
        //scan tp rawdata
        icn85xx_write_reg(4, 0x20); 
        mdelay(1);  
        for(i=0; i<1000; i++)
        {
            mdelay(1);
            icn85xx_read_reg(2, &retvalue);
            if(retvalue == 1)
                break;
        }
        
        for(i=0; i<row; i++)
        { 
            ret = icn85xx_i2c_rxdata(0x2000 + i*(COL_NUM)*2,(char *) &log_rawdata[i][0], column*2);
            if (ret < 0) {
                icn85xx_error("%s read_data i2c_rxdata failed: %d\n", __func__, ret);
            } 
            //icn85xx_rawdatadump(&log_rawdata[i][0], column, COL_NUM); 
            memcpy(&page[column*2*i], &log_rawdata[i][0], column*2);
            
        } 
    
        //finish scan tp rawdata
        icn85xx_write_reg(2, 0x0);    
        icn85xx_write_reg(4, 0x21); 
        goto read_out;        
    }
    else if(6 == cmd_head.wr)  //read diffdata
    {   
        proc_info("cmd_head_.wr == 6   \n");
        row = cmd_head.addr[1];
        column = cmd_head.addr[0];
                
        //scan tp rawdata
        icn85xx_write_reg(4, 0x20); 
        mdelay(1);

        for(i=0; i<1000; i++)
        {
            mdelay(1);
            icn85xx_read_reg(2, &retvalue);
            if(retvalue == 1)
                break;
        }      

        for(i=0; i<row; i++)
        { 
            ret = icn85xx_i2c_rxdata(0x3000 + (i+1)*(COL_NUM+2)*2 + 2,(char *) &log_diffdata[i][0], column*2);
            if (ret < 0) {
                icn85xx_error("%s read_data i2c_rxdata failed: %d\n", __func__, ret);
            } 
            //icn85xx_rawdatadump(&log_diffdata[i][0], column, COL_NUM); 
            memcpy(&page[column*2*i], &log_diffdata[i][0], column*2);
        }      
        //finish scan tp rawdata
        icn85xx_write_reg(2, 0x0);         
        icn85xx_write_reg(4, 0x21); 
        
        goto read_out;          
    }
    else if(8 == cmd_head.wr)  //change TxVol, read diff
    {       
        proc_info("cmd_head_.wr == 8  \n");
		row = cmd_head.addr[1];
        column = cmd_head.addr[0];
        //scan tp rawdata
        icn85xx_write_reg(4, 0x20); 
        mdelay(1);
        for(i=0; i<1000; i++)
        {
            mdelay(1);
            icn85xx_read_reg(2, &retvalue);
            if(retvalue == 1)
                break;
        }     
        
        for(i=0; i<row; i++)
        { 
            ret = icn85xx_i2c_rxdata(0x2000 + i*(COL_NUM)*2,(char *) &log_rawdata[i][0], column*2);
            if (ret < 0) {
                icn85xx_error("%s read_data i2c_rxdata failed: %d\n", __func__, ret);
            } 
           
        } 
    
        //finish scan tp rawdata
        icn85xx_write_reg(2, 0x0);    
        icn85xx_write_reg(4, 0x21); 

        icn85xx_write_reg(4, 0x12);

        //scan tp rawdata
        icn85xx_write_reg(4, 0x20); 
        mdelay(1);
        for(i=0; i<1000; i++)
        {
            mdelay(1);
            icn85xx_read_reg(2, &retvalue);
            if(retvalue == 1)
                break;
        }    
        
        for(i=0; i<row; i++)
        { 
            ret = icn85xx_i2c_rxdata(0x2000 + i*(COL_NUM)*2,(char *) &log_diffdata[i][0], column*2);
            if (ret < 0) {
                icn85xx_error("%s read_data i2c_rxdata failed: %d\n", __func__, ret);
            } 
            for(j=0; j<column; j++)
            {
                *(short *)&page[2*(column*i +j)] = log_rawdata[i][j] - log_diffdata[i][j];
            }
            
        } 
    
        //finish scan tp rawdata
        icn85xx_write_reg(2, 0x0);    
        icn85xx_write_reg(4, 0x21); 

        icn85xx_write_reg(4, 0x10);

        goto read_out;          
    }
    else if(10 == cmd_head.wr)  //read adc data
    {
        if(cmd_head.flag == 0)
        {
            icn85xx_prog_write_reg(0x040874, 0);  
        }        
        icn85xx_prog_i2c_rxdata(2500*cmd_head.flag ,&page[0], cmd_head.data_len);  
        //icn85xx_rawdatadump(&page[0], 1024, 16);
        if(cmd_head.flag == 9)
        {
            //reload code
            if(icn85xx_ts->ictype == ICN85XX_WITHOUT_FLASH)
            {
                if(R_OK == icn85xx_fw_update(firmware))
                {
                    icn85xx_ts->code_loaded_flag = 1;
                    icn85xx_trace("ICN85XX_WITHOUT_FLASH, reload code ok\n"); 
                }
                else
                {
                    icn85xx_ts->code_loaded_flag = 0;
                    icn85xx_trace("ICN85XX_WITHOUT_FLASH, reload code error\n"); 
                }
            }
            else
            {
                icn85xx_bootfrom_flash();                
                msleep(50);
            }
            icn85xx_ts->work_mode = 0;
        }
    }
    else if(12 == cmd_head.wr) //read hostcomm
    {
        proc_info("cmd_head_.wr == 12  \n");
		addr = (cmd_head.addr[1]<<8) | cmd_head.addr[0];
        icn85xx_read_reg(addr, &retvalue);
        page[0] = retvalue;
    }
    else if(14 == cmd_head.wr) //read adc status
    {
        proc_info("cmd_head_.wr == 14  \n");
	    icn85xx_prog_read_reg(0x4085E, &retvalue);
        page[0] = retvalue;
        printk("0x4085E: 0x%x\n", retvalue);
    }    
read_out:
    up(&icn85xx_ts->sem);   
    proc_info("%s out: %d, cmd_head.data_len: %d\n\n",__func__, count, cmd_head.data_len); 
    return cmd_head.data_len;
}

void init_proc_node(void)
{
    int i;
    memset(&cmd_head, 0, sizeof(cmd_head));
    cmd_head.data = NULL;

    i = 5;
    while ((!cmd_head.data) && i)
    {
        cmd_head.data = kzalloc(i * DATA_LENGTH_UINT, GFP_KERNEL);
        if (NULL != cmd_head.data)
        {
            break;
        }
        i--;
    }
    if (i)
    {
        //DATA_LENGTH = i * DATA_LENGTH_UINT + GTP_ADDR_LENGTH;
        DATA_LENGTH = i * DATA_LENGTH_UINT;
        icn85xx_trace("alloc memory size:%d.\n", DATA_LENGTH);
    }
    else
    {
        proc_error("alloc for memory failed.\n");
        return ;
    }

    icn85xx_proc_entry = create_proc_entry(ICN85xx_ENTRY_NAME, 0666, NULL);
    if (icn85xx_proc_entry == NULL)
    {
        proc_error("Couldn't create proc entry!\n");
        return ;
    }
    else
    {
        icn85xx_trace("Create proc entry success!\n");
        icn85xx_proc_entry->write_proc = icn85xx_tool_write;
        icn85xx_proc_entry->read_proc = icn85xx_tool_read;
    }

    return ;
}

void uninit_proc_node(void)
{
    kfree(cmd_head.data);
    cmd_head.data = NULL;
    remove_proc_entry(ICN85xx_ENTRY_NAME, NULL);
}
    
#endif


#if TOUCH_VIRTUAL_KEYS
static ssize_t virtual_keys_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    return sprintf(buf,
     __stringify(EV_KEY) ":" __stringify(KEY_MENU) ":100:1030:50:60"
     ":" __stringify(EV_KEY) ":" __stringify(KEY_HOME) ":280:1030:50:60"
     ":" __stringify(EV_KEY) ":" __stringify(KEY_BACK) ":470:1030:50:60"
     ":" __stringify(EV_KEY) ":" __stringify(KEY_SEARCH) ":900:1030:50:60"
     "\n");
}

static struct kobj_attribute virtual_keys_attr = {
    .attr = {
        .name = "virtualkeys.chipone-ts",
        .mode = S_IRUGO,
    },
    .show = &virtual_keys_show,
};

static struct attribute *properties_attrs[] = {
    &virtual_keys_attr.attr,
    NULL
};

static struct attribute_group properties_attr_group = {
    .attrs = properties_attrs,
};

static void icn85xx_ts_virtual_keys_init(void)
{
    int ret;
    struct kobject *properties_kobj;      
    properties_kobj = kobject_create_and_add("board_properties", NULL);
    if (properties_kobj)
        ret = sysfs_create_group(properties_kobj,
                     &properties_attr_group);
    if (!properties_kobj || ret)
        pr_err("failed to create board_properties\n");    
}
#endif




/* ---------------------------------------------------------------------
*
*   Chipone panel related driver
*
*
----------------------------------------------------------------------*/
/***********************************************************************************************
Name    :   icn85xx_ts_wakeup 
Input   :   void
Output  :   ret
function    : this function is used to wakeup tp
***********************************************************************************************/
void icn85xx_ts_wakeup(void)
{
  


}

/***********************************************************************************************
Name    :   icn85xx_ts_reset 
Input   :   void
Output  :   ret
function    : this function is used to reset tp, you should not delete it
***********************************************************************************************/
void icn85xx_ts_reset(void)
{
    //set reset func

	int rst = g_param.rstgpio;
	gpio_direction_output(rst,0);
	msleep(50);
	icn85xx_info("[%s]:>>>>>>>>>>>>>>>>>CTP_RST_PORT = %d;msleep(50);\n",__func__,gpio_get_value(rst));
	gpio_direction_output(rst,1);
	msleep(70);
	icn85xx_info("[%s]:>>>>>>>>>>>>>>>>>CTP_RST_PORT = %d;msleep(70);\n",__func__,gpio_get_value(rst));

}

/***********************************************************************************************
Name    :   icn85xx_irq_disable 
Input   :   void
Output  :   ret
function    : this function is used to disable irq
***********************************************************************************************/
void icn85xx_irq_disable(void)
{
    unsigned long irqflags;
    struct icn85xx_ts_data *icn85xx_ts = i2c_get_clientdata(this_client);

    spin_lock_irqsave(&icn85xx_ts->irq_lock, irqflags);
    if (!icn85xx_ts->irq_is_disable)
    {
        icn85xx_ts->irq_is_disable = 1; 
		wmt_gpio_mask_irq(g_param.irqgpio);
        //disable_irq_nosync(icn85xx_ts->irq);
        //disable_irq(icn85xx_ts->irq);
    }
    spin_unlock_irqrestore(&icn85xx_ts->irq_lock, irqflags);

}

/***********************************************************************************************
Name    :   icn85xx_irq_enable 
Input   :   void
Output  :   ret
function    : this function is used to enable irq
***********************************************************************************************/
void icn85xx_irq_enable(void)
{
    unsigned long irqflags = 0;
    struct icn85xx_ts_data *icn85xx_ts = i2c_get_clientdata(this_client);

    spin_lock_irqsave(&icn85xx_ts->irq_lock, irqflags);
    if (icn85xx_ts->irq_is_disable) 
    {
		wmt_gpio_unmask_irq(g_param.irqgpio);
        //enable_irq(icn85xx_ts->irq);
        icn85xx_ts->irq_is_disable = 0; 
    }
    spin_unlock_irqrestore(&icn85xx_ts->irq_lock, irqflags);

}

/***********************************************************************************************
Name    :   icn85xx_prog_i2c_rxdata 
Input   :   addr
            *rxdata
            length
Output  :   ret
function    : read data from icn85xx, prog mode 
***********************************************************************************************/
int icn85xx_prog_i2c_rxdata(unsigned int addr, char *rxdata, int length)
{
    int ret = -1;
    int retries = 0;
#if 0 
    struct i2c_msg msgs[] = {   
        {
            .addr   = ICN85XX_PROG_IIC_ADDR,//this_client->addr,
            .flags  = I2C_M_RD,
            .len    = length,
            .buf    = rxdata,
#if SUPPORT_ROCKCHIP            
            .scl_rate = ICN85XX_I2C_SCL,
#endif            
        },
    };
        
    icn85xx_prog_i2c_txdata(addr, NULL, 0);
    while(retries < IIC_RETRY_NUM)
    {    
        ret = i2c_transfer(this_client->adapter, msgs, 1);
        if(ret == 1)break;
        retries++;
    }
    if (retries >= IIC_RETRY_NUM)
    {
        icn85xx_error("%s i2c read error: %d\n", __func__, ret); 
//        icn85xx_ts_reset();
    }    
#else
    unsigned char tmp_buf[3];
    struct i2c_msg msgs[] = {
        {
            .addr   = ICN85XX_PROG_IIC_ADDR,//this_client->addr,
            .flags  = 0,
            .len    = 3,
            .buf    = tmp_buf,
#if SUPPORT_ROCKCHIP            
            .scl_rate = ICN85XX_I2C_SCL,
#endif            
        },
        {
            .addr   = ICN85XX_PROG_IIC_ADDR,//this_client->addr,
            .flags  = I2C_M_RD,
            .len    = length,
            .buf    = rxdata,
#if SUPPORT_ROCKCHIP            
            .scl_rate = ICN85XX_I2C_SCL,
#endif            
        },
    };

    tmp_buf[0] = (unsigned char)(addr>>16);
    tmp_buf[1] = (unsigned char)(addr>>8);
    tmp_buf[2] = (unsigned char)(addr);
    

    while(retries < IIC_RETRY_NUM)
    {
        ret = i2c_transfer(this_client->adapter, msgs, 2);
        if(ret == 2)break;
        retries++;
    }

    if (retries >= IIC_RETRY_NUM)
    {
        icn85xx_error("%s i2c read error: %d\n", __func__, ret); 
//        icn85xx_ts_reset();
    }
#endif      
    return ret;
}
/***********************************************************************************************
Name    :   icn85xx_prog_i2c_txdata 
Input   :   addr
            *rxdata
            length
Output  :   ret
function    : send data to icn85xx , prog mode
***********************************************************************************************/
int icn85xx_prog_i2c_txdata(unsigned int addr, char *txdata, int length)
{
    int ret = -1;
    char tmp_buf[128];
    int retries = 0; 
    struct i2c_msg msg[] = {
        {
            .addr   = ICN85XX_PROG_IIC_ADDR,//this_client->addr,
            .flags  = 0,
            .len    = length + 3,
            .buf    = tmp_buf,
#if SUPPORT_ROCKCHIP            
            .scl_rate = ICN85XX_I2C_SCL,
#endif            
        },
    };
    
    if (length > 125)
    {
        icn85xx_error("%s too big datalen = %d!\n", __func__, length);
        return -1;
    }
    
    tmp_buf[0] = (unsigned char)(addr>>16);
    tmp_buf[1] = (unsigned char)(addr>>8);
    tmp_buf[2] = (unsigned char)(addr);


    if (length != 0 && txdata != NULL)
    {
        memcpy(&tmp_buf[3], txdata, length);
    }   
    
    while(retries < IIC_RETRY_NUM)
    {
        ret = i2c_transfer(this_client->adapter, msg, 1);
        if(ret == 1)break;
        retries++;
    }

    if (retries >= IIC_RETRY_NUM)
    {
        icn85xx_error("%s i2c write error: %d\n", __func__, ret); 
//        icn85xx_ts_reset();
    }
    return ret;
}
/***********************************************************************************************
Name    :   icn85xx_prog_write_reg
Input   :   addr -- address
            para -- parameter
Output  :   
function    :   write register of icn85xx, prog mode
***********************************************************************************************/
int icn85xx_prog_write_reg(unsigned int addr, char para)
{
    char buf[3];
    int ret = -1;

    buf[0] = para;
    ret = icn85xx_prog_i2c_txdata(addr, buf, 1);
    if (ret < 0) {
        icn85xx_error("%s write reg failed! %#x ret: %d\n", __func__, buf[0], ret);
        return -1;
    }
    
    return ret;
}


/***********************************************************************************************
Name    :   icn85xx_prog_read_reg 
Input   :   addr
            pdata
Output  :   
function    :   read register of icn85xx, prog mode
***********************************************************************************************/
int icn85xx_prog_read_reg(unsigned int addr, char *pdata)
{
    int ret = -1;
    ret = icn85xx_prog_i2c_rxdata(addr, pdata, 1);  
    return ret;    
}

/***********************************************************************************************
Name    :   icn85xx_i2c_rxdata 
Input   :   addr
            *rxdata
            length
Output  :   ret
function    : read data from icn85xx, normal mode   
***********************************************************************************************/
int icn85xx_i2c_rxdata(unsigned short addr, char *rxdata, int length)
{
    int ret = -1;
    int retries = 0;
     unsigned char tmp_buf[2];
#if 0
   
    struct i2c_msg msgs[] = {
        {
            .addr   = this_client->addr,
            .flags  = 0,
            .len    = 2,
            .buf    = tmp_buf,
#if SUPPORT_ROCKCHIP            
            .scl_rate = ICN85XX_I2C_SCL,
#endif            
        },
        {
            .addr   = this_client->addr,
            .flags  = I2C_M_RD,
            .len    = length,
            .buf    = rxdata,
#if SUPPORT_ROCKCHIP            
            .scl_rate = ICN85XX_I2C_SCL,
#endif
        },
    };
    
    tmp_buf[0] = (unsigned char)(addr>>8);
    tmp_buf[1] = (unsigned char)(addr);
   

    while(retries < IIC_RETRY_NUM)
    {
        ret = i2c_transfer(this_client->adapter, msgs, 2);
        if(ret == 2)break;
        retries++;
    }

    if (retries >= IIC_RETRY_NUM)
    {
        icn85xx_error("%s i2c read error: %d\n", __func__, ret); 
//        icn85xx_ts_reset();
    }    
#else 

    tmp_buf[0] = (unsigned char)(addr>>8);
    tmp_buf[1] = (unsigned char)(addr);
   
    while(retries < IIC_RETRY_NUM)
    {
      //  ret = i2c_transfer(this_client->adapter, msgs, 2);
		ret = i2c_master_send(this_client, tmp_buf, 2);
	    if (ret < 0)
			return ret;
		ret = i2c_master_recv(this_client, rxdata, length);
		if (ret < 0)
			return ret;
		if(ret == length)break;
        retries++;
    }

    if (retries >= IIC_RETRY_NUM)
    {
        icn85xx_error("%s i2c read error: %d\n", __func__, ret); 
//        icn85xx_ts_reset();
    }    
#endif

    return ret;
}
/***********************************************************************************************
Name    :   icn85xx_i2c_txdata 
Input   :   addr
            *rxdata
            length
Output  :   ret
function    : send data to icn85xx , normal mode
***********************************************************************************************/
int icn85xx_i2c_txdata(unsigned short addr, char *txdata, int length)
{
    int ret = -1;
    unsigned char tmp_buf[128];
    int retries = 0;

    struct i2c_msg msg[] = {
        {
            .addr   = this_client->addr,
            .flags  = 0,
            .len    = length + 2,
            .buf    = tmp_buf,
#if SUPPORT_ROCKCHIP             
            .scl_rate = ICN85XX_I2C_SCL,
#endif            
        },
    };
    
    if (length > 125)
    {
        icn85xx_error("%s too big datalen = %d!\n", __func__, length);
        return -1;
    }
    
    tmp_buf[0] = (unsigned char)(addr>>8);
    tmp_buf[1] = (unsigned char)(addr);

    if (length != 0 && txdata != NULL)
    {
        memcpy(&tmp_buf[2], txdata, length);
    }   
    
    while(retries < IIC_RETRY_NUM)
    {
        ret = i2c_transfer(this_client->adapter, msg, 1);
        if(ret == 1)break;
        retries++;
    }

    if (retries >= IIC_RETRY_NUM)
    {
        icn85xx_error("%s i2c write error: %d\n", __func__, ret); 
//        icn85xx_ts_reset();
    }

    return ret;
}

/***********************************************************************************************
Name    :   icn85xx_write_reg
Input   :   addr -- address
            para -- parameter
Output  :   
function    :   write register of icn85xx, normal mode
***********************************************************************************************/
int icn85xx_write_reg(unsigned short addr, char para)
{
    char buf[3];
    int ret = -1;

    buf[0] = para;
    ret = icn85xx_i2c_txdata(addr, buf, 1);
    if (ret < 0) {
        icn85xx_error("write reg failed! %#x ret: %d\n", buf[0], ret);
        return -1;
    }
    
    return ret;
}


/***********************************************************************************************
Name    :   icn85xx_read_reg 
Input   :   addr
            pdata
Output  :   
function    :   read register of icn85xx, normal mode
***********************************************************************************************/
int icn85xx_read_reg(unsigned short addr, char *pdata)
{
    int ret = -1;
    ret = icn85xx_i2c_rxdata(addr, pdata, 1);   
    if(ret < 0)
    {
        icn85xx_error("addr: 0x%x: 0x%x\n", addr, *pdata); 
    }
    return ret;    
}


/***********************************************************************************************
Name    :   icn85xx_log
Input   :   0: rawdata, 1: diff data
Output  :   err type
function    :   calibrate param
***********************************************************************************************/
static void icn85xx_log(char diff)
{
    char row = 0;
    char column = 0;
    int i, j, ret;
    char retvalue = 0;
	
    icn85xx_read_reg(0x8004, &row);
    icn85xx_read_reg(0x8005, &column);

    //scan tp rawdata
    icn85xx_write_reg(4, 0x20); 
    mdelay(1);
    for(i=0; i<1000; i++)
    {
        mdelay(1);
        icn85xx_read_reg(2, &retvalue);
        if(retvalue == 1)
            break;
    }     
    if(diff == 0)
    {
        for(i=0; i<row; i++)
        {       
            ret = icn85xx_i2c_rxdata(0x2000 + i*COL_NUM*2, (char *)&log_rawdata[i][0], column*2);
            if (ret < 0) {
                icn85xx_error("%s read_data i2c_rxdata failed: %d\n", __func__, ret);
            } 
            icn85xx_rawdatadump(&log_rawdata[i][0], column, COL_NUM);  

        } 
    }
    if(diff == 1)
    {
        for(i=0; i<row; i++)
        { 
            ret = icn85xx_i2c_rxdata(0x3000 + (i+1)*(COL_NUM+2)*2 + 2, (char *)&log_diffdata[i][0], column*2);
            if (ret < 0) {
                icn85xx_error("%s read_data i2c_rxdata failed: %d\n", __func__, ret);
            } 
            icn85xx_rawdatadump(&log_diffdata[i][0], column, COL_NUM);   
            
        }           
    }
    else if(diff == 2)
    {        
        for(i=0; i<row; i++)
        {       
            ret = icn85xx_i2c_rxdata(0x2000 + i*COL_NUM*2, (char *)&log_rawdata[i][0], column*2);
            if (ret < 0) {
                icn85xx_error("%s read_data i2c_rxdata failed: %d\n", __func__, ret);
            } 
            if((log_basedata[0][0] != 0) || (log_basedata[0][1] != 0))
            {
                for(j=0; j<column; j++)
                {
                    log_rawdata[i][j] = log_basedata[i][j] - log_rawdata[i][j];
                }
            }
            icn85xx_rawdatadump(&log_rawdata[i][0], column, COL_NUM);  

        }    
        if((log_basedata[0][0] == 0) && (log_basedata[0][1] == 0))
        {
            memcpy(&log_basedata[0][0], &log_rawdata[0][0], COL_NUM*ROW_NUM*2);           
        }
     
        
    }
    
    //finish scan tp rawdata
    icn85xx_write_reg(2, 0x0);      
    icn85xx_write_reg(4, 0x21); 
}


/***********************************************************************************************
Name    :   icn85xx_iic_test 
Input   :   void
Output  :   
function    : 0 success,
***********************************************************************************************/
static int icn85xx_iic_test(void)
{
    struct icn85xx_ts_data *icn85xx_ts = i2c_get_clientdata(this_client);
    int  ret = -1;
    char value = 0;
    int  retry = 0;
    int  flashid;
    icn85xx_ts->ictype = 0;
    icn85xx_trace("====%s begin=====.  \n", __func__);
    
    while(retry++ < 3)
    {        
        ret = icn85xx_read_reg(0xa, &value);
        if(ret > 0)
        {
            if(value == 0x85)
            {
                icn85xx_ts->ictype = ICN85XX_WITH_FLASH;
                return ret;
            }
        }
        
        icn85xx_info("iic test error! retry = %d\n", retry);
        msleep(3);
    }
    icn85xx_goto_progmode();
    msleep(10);
    retry = 0;
    while(retry++ < 3)
    {       
        ret = icn85xx_prog_i2c_rxdata(0x040002, &value, 1);
        icn85xx_info("icn85xx_check_progmod: 0x%x\n", value);
        if(ret > 0)
        {
            if(value == 0x85)
            {
                flashid = icn85xx_read_flashid();
                if((MD25D40_ID1 == flashid) || (MD25D40_ID2 == flashid)
                    ||(MD25D20_ID1 == flashid) || (MD25D20_ID1 == flashid)
                    ||(GD25Q10_ID == flashid) || (MX25L512E_ID == flashid))
                {
                    icn85xx_ts->ictype = ICN85XX_WITH_FLASH;
                }
                else
                {
                    icn85xx_ts->ictype = ICN85XX_WITHOUT_FLASH;
                }
                return ret;
            }
        }          
        icn85xx_error("iic2 test error! %d\n", retry);
        msleep(3);
    }    
    
    return ret;    
}

#if !CTP_REPORT_PROTOCOL
/***********************************************************************************************
Name    :   icn85xx_ts_release 
Input   :   void
Output  :   
function    : touch release
***********************************************************************************************/
static void icn85xx_ts_release(void)
{
    struct icn85xx_ts_data *icn85xx_ts = i2c_get_clientdata(this_client);
    icn85xx_info("==icn85xx_ts_release ==\n");
    input_report_abs(icn85xx_ts->input_dev, ABS_MT_TOUCH_MAJOR, 0);
    input_sync(icn85xx_ts->input_dev);
}

/***********************************************************************************************
Name    :   icn85xx_report_value_A
Input   :   void
Output  :   
function    : reprot touch ponit
***********************************************************************************************/
static void icn85xx_report_value_A(void)
{
    struct icn85xx_ts_data *icn85xx_ts = i2c_get_clientdata(this_client);
    char buf[POINT_NUM*POINT_SIZE+3]={0};
    int ret = -1;
    int i;
#if TOUCH_VIRTUAL_KEYS
    unsigned char button;
    static unsigned char button_last;
#endif

    icn85xx_info("==icn85xx_report_value_A ==\n");

    ret = icn85xx_i2c_rxdata(0x1000, buf, POINT_NUM*POINT_SIZE+2);
    if (ret < 0) {
        icn85xx_error("%s read_data i2c_rxdata failed: %d\n", __func__, ret);
        return ;
    }
#if TOUCH_VIRTUAL_KEYS    
    button = buf[0];    
    icn85xx_info("%s: button=%d\n",__func__, button);

    if((button_last != 0) && (button == 0))
    {
        icn85xx_ts_release();
        button_last = button;
        return ;       
    }
    if(button != 0)
    {
        switch(button)
        {
            case ICN_VIRTUAL_BUTTON_HOME:
                icn85xx_info("ICN_VIRTUAL_BUTTON_HOME down\n");
                input_report_abs(icn85xx_ts->input_dev, ABS_MT_TOUCH_MAJOR, 200);
                input_report_abs(icn85xx_ts->input_dev, ABS_MT_POSITION_X, 280);
                input_report_abs(icn85xx_ts->input_dev, ABS_MT_POSITION_Y, 1030);
                input_report_abs(icn85xx_ts->input_dev, ABS_MT_WIDTH_MAJOR, 1);
                input_mt_sync(icn85xx_ts->input_dev);
                input_sync(icn85xx_ts->input_dev);            
                break;
            case ICN_VIRTUAL_BUTTON_BACK:
                icn85xx_info("ICN_VIRTUAL_BUTTON_BACK down\n");
                input_report_abs(icn85xx_ts->input_dev, ABS_MT_TOUCH_MAJOR, 200);
                input_report_abs(icn85xx_ts->input_dev, ABS_MT_POSITION_X, 470);
                input_report_abs(icn85xx_ts->input_dev, ABS_MT_POSITION_Y, 1030);
                input_report_abs(icn85xx_ts->input_dev, ABS_MT_WIDTH_MAJOR, 1);
                input_mt_sync(icn85xx_ts->input_dev);
                input_sync(icn85xx_ts->input_dev);                
                break;
            case ICN_VIRTUAL_BUTTON_MENU:
                icn85xx_info("ICN_VIRTUAL_BUTTON_MENU down\n");
                input_report_abs(icn85xx_ts->input_dev, ABS_MT_TOUCH_MAJOR, 200);
                input_report_abs(icn85xx_ts->input_dev, ABS_MT_POSITION_X, 100);
                input_report_abs(icn85xx_ts->input_dev, ABS_MT_POSITION_Y, 1030);
                input_report_abs(icn85xx_ts->input_dev, ABS_MT_WIDTH_MAJOR, 1);
                input_mt_sync(icn85xx_ts->input_dev);
                input_sync(icn85xx_ts->input_dev);            
                break;                      
            default:
                icn85xx_info("other gesture\n");
                break;          
        }
        button_last = button;
        return ;
    }        
#endif
 
    icn85xx_ts->point_num = buf[1];    
    if (icn85xx_ts->point_num == 0) {
        icn85xx_ts_release();
        return ; 
    }   
    for(i=0;i<icn85xx_ts->point_num;i++){
        if(buf[8 + POINT_SIZE*i]  != 4)
        {
          break ;
        }
        else
        {
        
        }
    }
    
    if(i == icn85xx_ts->point_num) {
        icn85xx_ts_release();
        return ; 
    }   

    for(i=0; i<icn85xx_ts->point_num; i++)
    {
        icn85xx_ts->point_info[i].u8ID = buf[2 + POINT_SIZE*i];
        icn85xx_ts->point_info[i].u16PosX = (buf[4 + POINT_SIZE*i]<<8) + buf[3 + POINT_SIZE*i];
        icn85xx_ts->point_info[i].u16PosY = (buf[6 + POINT_SIZE*i]<<8) + buf[5 + POINT_SIZE*i];
        icn85xx_ts->point_info[i].u8Pressure = 20;//buf[7 + POINT_SIZE*i];
        icn85xx_ts->point_info[i].u8EventId = buf[8 + POINT_SIZE*i];    

        if(1 == icn85xx_ts->revert_x_flag)
        {
            icn85xx_ts->point_info[i].u16PosX = icn85xx_ts->screen_max_x- icn85xx_ts->point_info[i].u16PosX;
        }
        if(1 == icn85xx_ts->revert_y_flag)
        {
            icn85xx_ts->point_info[i].u16PosY = icn85xx_ts->screen_max_y- icn85xx_ts->point_info[i].u16PosY;
        }
        
        icn85xx_info("u8ID %d\n", icn85xx_ts->point_info[i].u8ID);
        icn85xx_info("u16PosX %d\n", icn85xx_ts->point_info[i].u16PosX);
        icn85xx_info("u16PosY %d\n", icn85xx_ts->point_info[i].u16PosY);
        icn85xx_info("u8Pressure %d\n", icn85xx_ts->point_info[i].u8Pressure);
        icn85xx_info("u8EventId %d\n", icn85xx_ts->point_info[i].u8EventId);  


        input_report_abs(icn85xx_ts->input_dev, ABS_MT_TRACKING_ID, icn85xx_ts->point_info[i].u8ID);    
        input_report_abs(icn85xx_ts->input_dev, ABS_MT_TOUCH_MAJOR, icn85xx_ts->point_info[i].u8Pressure);
        input_report_abs(icn85xx_ts->input_dev, ABS_MT_POSITION_X, icn85xx_ts->point_info[i].u16PosX);
        input_report_abs(icn85xx_ts->input_dev, ABS_MT_POSITION_Y, icn85xx_ts->point_info[i].u16PosY);
        input_report_abs(icn85xx_ts->input_dev, ABS_MT_WIDTH_MAJOR, 1);
        input_mt_sync(icn85xx_ts->input_dev);
        icn85xx_point_info("point: %d ===x = %d,y = %d, press = %d ====\n",i, icn85xx_ts->point_info[i].u16PosX,icn85xx_ts->point_info[i].u16PosY, icn85xx_ts->point_info[i].u8Pressure);
    }

    input_sync(icn85xx_ts->input_dev);
  
}
#endif
/***********************************************************************************************
Name    :   icn85xx_report_value_B
Input   :   void
Output  :   
function    : reprot touch ponit
***********************************************************************************************/
#if CTP_REPORT_PROTOCOL
static void icn85xx_report_value_B(void)
{
    struct icn85xx_ts_data *icn85xx_ts = i2c_get_clientdata(this_client);
    char buf[POINT_NUM*POINT_SIZE+3]={0};
    static unsigned char finger_last[POINT_NUM + 1]={0};
    unsigned char  finger_current[POINT_NUM + 1] = {0};
    unsigned int position = 0;
    int temp = 0;
    int ret = -1;
	int x,y;
    icn85xx_info("==icn85xx_report_value_B ==\n");



    ret = icn85xx_i2c_rxdata(0x1000, buf, POINT_NUM*POINT_SIZE+2);
    if (ret < 0) {
         icn85xx_error("%s read_data i2c_rxdata failed: %d\n", __func__, ret);
          return ;
      }		 
    icn85xx_ts->point_num = buf[1];
    if (icn85xx_ts->point_num > POINT_NUM)
     {
       return ;
     }

    if(icn85xx_ts->point_num > 0)
    {
        for(position = 0; position<icn85xx_ts->point_num; position++)
        {       
            temp = buf[2 + POINT_SIZE*position] + 1;
            finger_current[temp] = 1;
            icn85xx_ts->point_info[temp].u8ID = buf[2 + POINT_SIZE*position];
            icn85xx_ts->point_info[temp].u16PosX = (buf[4 + POINT_SIZE*position]<<8) + buf[3 + POINT_SIZE*position];
            icn85xx_ts->point_info[temp].u16PosY = (buf[6 + POINT_SIZE*position]<<8) + buf[5 + POINT_SIZE*position];
            icn85xx_ts->point_info[temp].u8Pressure = buf[7 + POINT_SIZE*position];
            icn85xx_ts->point_info[temp].u8EventId = buf[8 + POINT_SIZE*position];
            
            if(icn85xx_ts->point_info[temp].u8EventId == 4)
                finger_current[temp] = 0;
                            
            if(1 == icn85xx_ts->revert_x_flag)
            {
                icn85xx_ts->point_info[temp].u16PosX = icn85xx_ts->screen_max_x- icn85xx_ts->point_info[temp].u16PosX;
            }
            if(1 == icn85xx_ts->revert_y_flag)
            {
                icn85xx_ts->point_info[temp].u16PosY = icn85xx_ts->screen_max_y- icn85xx_ts->point_info[temp].u16PosY;
            }
            icn85xx_info("temp %d\n", temp);
            icn85xx_info("u8ID %d\n", icn85xx_ts->point_info[temp].u8ID);
            icn85xx_info("u16PosX %d\n", icn85xx_ts->point_info[temp].u16PosX);
            icn85xx_info("u16PosY %d\n", icn85xx_ts->point_info[temp].u16PosY);
            icn85xx_info("u8Pressure %d\n", icn85xx_ts->point_info[temp].u8Pressure);
            icn85xx_info("u8EventId %d\n", icn85xx_ts->point_info[temp].u8EventId);             
            //icn85xx_info("u8Pressure %d\n", icn85xx_ts->point_info[temp].u8Pressure*16);
        }
    }   
    else
    {
        for(position = 1; position < POINT_NUM+1; position++)
        {
            finger_current[position] = 0;
        }
        icn85xx_info("no touch\n");
    }

    for(position = 1; position < POINT_NUM + 1; position++)
    {
        if((finger_current[position] == 0) && (finger_last[position] != 0))
        {
            input_mt_slot(icn85xx_ts->input_dev, position-1);
            input_mt_report_slot_state(icn85xx_ts->input_dev, MT_TOOL_FINGER, false);
            icn85xx_point_info("one touch up: %d\n", position);
        }
        else if(finger_current[position])
        {
			if (g_param.xyswap == 0)
			{
				x = icn85xx_ts->point_info[position].u16PosX;
				y = icn85xx_ts->point_info[position].u16PosY;
			} else {
				y = icn85xx_ts->point_info[position].u16PosX;
				x = icn85xx_ts->point_info[position].u16PosY;
			}
			if (g_param.xdir == -1)
			{
				x = g_param.panelres_x - x;
			}
			if (g_param.ydir == -1)
			{
				y = g_param.panelres_y - y;
			}

			if (g_param.lcd_exchg) {
				int tmp;
				tmp = x;
				x = y;
				y = g_param.panelres_x - tmp;
			}

            input_mt_slot(icn85xx_ts->input_dev, position-1);
            input_mt_report_slot_state(icn85xx_ts->input_dev, MT_TOOL_FINGER, true);
            input_report_abs(icn85xx_ts->input_dev, ABS_MT_TOUCH_MAJOR, 1);
            //input_report_abs(icn85xx_ts->input_dev, ABS_MT_PRESSURE, icn85xx_ts->point_info[position].u8Pressure);
            input_report_abs(icn85xx_ts->input_dev, ABS_MT_PRESSURE, 200);
            input_report_abs(icn85xx_ts->input_dev, ABS_MT_POSITION_X, x);
            input_report_abs(icn85xx_ts->input_dev, ABS_MT_POSITION_Y, y);
            //icn85xx_point_info("===position: %d, x = %d,y = %d, press = %d ====\n", position, icn85xx_ts->point_info[position].u16PosX,icn85xx_ts->point_info[position].u16PosY, icn85xx_ts->point_info[position].u8Pressure);
			icn85xx_point_info("raw%d(%d,%d), rpt%d(%d,%d)\n", position, icn85xx_ts->point_info[position].u16PosX, icn85xx_ts->point_info[position].u16PosY, position, x, y);
        }

    }
    input_sync(icn85xx_ts->input_dev);

    for(position = 1; position < POINT_NUM + 1; position++)
    {
        finger_last[position] = finger_current[position];
    }
    
}
#endif

/***********************************************************************************************
Name    :   icn85xx_ts_pen_irq_work
Input   :   void
Output  :   
function    : work_struct
***********************************************************************************************/
static void icn85xx_ts_pen_irq_work(struct work_struct *work)
{
    struct icn85xx_ts_data *icn85xx_ts = i2c_get_clientdata(this_client);  
#if SUPPORT_PROC_FS
    if(down_interruptible(&icn85xx_ts->sem))  
    {  
        return ;   
    }  
#endif
      
    if(icn85xx_ts->work_mode == 0)
    {
#if CTP_REPORT_PROTOCOL
        icn85xx_report_value_B();
#else
        icn85xx_report_value_A();
#endif 


        if(icn85xx_ts->use_irq)
        {
            icn85xx_irq_enable();
        }
        if(log_on_off == 4)
        {
            printk("normal raw data\n");
            icn85xx_log(0);   //raw data
        }
        else if(log_on_off == 5)
        {
            printk("normal diff data\n");
            icn85xx_log(1);   //diff data         
        }
        else if(log_on_off == 6)
        {
            printk("normal raw2diff\n");
            icn85xx_log(2);   //diff data         
        }

    }
    else if(icn85xx_ts->work_mode == 1)
    {
        printk("raw data\n");
        icn85xx_log(0);   //raw data
    }
    else if(icn85xx_ts->work_mode == 2)
    {
        printk("diff data\n");
        icn85xx_log(1);   //diff data
    }
    else if(icn85xx_ts->work_mode == 3)
    {
        printk("raw2diff data\n");
        icn85xx_log(2);   //diff data
    }
    else if(icn85xx_ts->work_mode == 4)  //idle
    {
        ;
    }
    else if(icn85xx_ts->work_mode == 5)//write para, reinit
    {
        printk("reinit tp\n");
        icn85xx_write_reg(0, 1); 
        mdelay(100);
        icn85xx_write_reg(0, 0);            
        icn85xx_ts->work_mode = 0;
    }

#if SUPPORT_PROC_FS
    up(&icn85xx_ts->sem);
#endif


}
/***********************************************************************************************
Name    :   chipone_timer_func
Input   :   void
Output  :   
function    : Timer interrupt service routine.
***********************************************************************************************/
static enum hrtimer_restart chipone_timer_func(struct hrtimer *timer)
{
    struct icn85xx_ts_data *icn85xx_ts = container_of(timer, struct icn85xx_ts_data, timer);
    queue_work(icn85xx_ts->ts_workqueue, &icn85xx_ts->pen_event_work);
    //icn85xx_info("chipone_timer_func\n"); 
    if(icn85xx_ts->use_irq == 1)
    {
        if((icn85xx_ts->work_mode == 1) || (icn85xx_ts->work_mode == 2) || (icn85xx_ts->work_mode == 3))
        {
            hrtimer_start(&icn85xx_ts->timer, ktime_set(CTP_POLL_TIMER/1000, (CTP_POLL_TIMER%1000)*1000000), HRTIMER_MODE_REL);
        }
    }
    else
    {
        hrtimer_start(&icn85xx_ts->timer, ktime_set(CTP_POLL_TIMER/1000, (CTP_POLL_TIMER%1000)*1000000), HRTIMER_MODE_REL);
    }
    return HRTIMER_NORESTART;
}
/***********************************************************************************************
Name    :   icn85xx_ts_interrupt
Input   :   void
Output  :   
function    : interrupt service routine
***********************************************************************************************/
static irqreturn_t icn85xx_ts_interrupt(int irq, void *dev_id)
{
    struct icn85xx_ts_data *icn85xx_ts = dev_id;
	int irqindex = g_param.irqgpio;
       
    icn85xx_info("==========------icn85xx_ts TS Interrupt-----============\n"); 

	if (gpio_irqstatus(irqindex)) {
		wmt_gpio_ack_irq(irqindex);
		if (is_gpio_irqenable(irqindex)) {
			icn85xx_irq_disable();
			if(icn85xx_ts->work_mode != 0) {
				icn85xx_irq_enable();
				return IRQ_HANDLED;
			}
			if (!work_pending(&icn85xx_ts->pen_event_work)) {
				icn85xx_info("Enter work\n");
				queue_work(icn85xx_ts->ts_workqueue, &icn85xx_ts->pen_event_work);
			}
		}
		return IRQ_HANDLED;
	}
	return IRQ_NONE;

    /*if(icn85xx_ts->use_irq)
        icn85xx_irq_disable();
    if (!work_pending(&icn85xx_ts->pen_event_work)) 
    {
        queue_work(icn85xx_ts->ts_workqueue, &icn85xx_ts->pen_event_work);

    }

    return IRQ_HANDLED;*/
}

/***********************************************************************************************
Name    :   icn85xx_ts_suspend
Input   :   void
Output  :   
function    : tp enter sleep mode
***********************************************************************************************/
static int icn85xx_ts_suspend(struct i2c_client *client, pm_message_t mesg)
{
    struct icn85xx_ts_data *icn85xx_ts = i2c_get_clientdata(client);
    icn85xx_trace("icn85xx_ts_suspend\n");
    if (icn85xx_ts->use_irq) {
        icn85xx_irq_disable();
    } else {
        hrtimer_cancel(&icn85xx_ts->timer);
    }  
	cancel_work_sync(&icn85xx_ts->pen_event_work);
    flush_workqueue(icn85xx_ts->ts_workqueue);
    //}  

    //reset flag if ic is flashless when power off
    if(icn85xx_ts->ictype == ICN85XX_WITHOUT_FLASH)
    {
        //icn85xx_ts->code_loaded_flag = 0;
    }      
#if SUSPEND_POWER_OFF           

#else
    icn85xx_write_reg(ICN85xx_REG_PMODE, PMODE_HIBERNATE); 
#endif

	return 0;
    
}

int resume_download_thread(void *arg)
{
    int retry = 1;
    int need_update_fw = false;
    int ret = -1;
    unsigned char value;
    struct icn85xx_ts_data *icn85xx_ts = (struct icn85xx_ts_data*)arg;
	wake_lock(&downloadWakeLock);
	while (retry-- && !need_update_fw) {
		icn85xx_ts_reset();
		icn85xx_bootfrom_sram();
		msleep(50);
		ret = icn85xx_read_reg(0xa, &value);
		if (ret > 0) { 
			need_update_fw = false;
			break; 
		}
	}
	if (retry < 0) need_update_fw = true;

	if (need_update_fw) {
		if(R_OK == icn85xx_fw_update(firmware)) {
			icn85xx_ts->code_loaded_flag = 1;
			icn85xx_trace("ICN85XX_WITHOUT_FLASH, reload code ok\n"); 
		}
		else {
			icn85xx_ts->code_loaded_flag = 0;
			icn85xx_trace("ICN85XX_WITHOUT_FLASH, reload code error\n"); 
		}
	}

	if (icn85xx_ts->use_irq) {
		icn85xx_irq_enable();
	} else {
		hrtimer_start(&icn85xx_ts->timer, ktime_set(CTP_START_TIMER/1000, \
					(CTP_START_TIMER%1000)*1000000), HRTIMER_MODE_REL);
	}
	wake_unlock(&downloadWakeLock);
	return 0;
}

/***********************************************************************************************
Name    :   icn85xx_ts_resume
Input   :   void
Output  :   
function    : wakeup tp or reset tp
***********************************************************************************************/
static int icn85xx_ts_resume(struct i2c_client *client)
{
    struct icn85xx_ts_data *icn85xx_ts = i2c_get_clientdata(client);
    int i;
    icn85xx_trace("==icn85xx_ts_resume== \n");

        //report touch release    
#if CTP_REPORT_PROTOCOL
    for(i = 0; i < POINT_NUM; i++)
    {
        input_mt_slot(icn85xx_ts->input_dev, i);
        input_mt_report_slot_state(icn85xx_ts->input_dev, MT_TOOL_FINGER, false);
    }
#else
    icn85xx_ts_release();
#endif 


	if(icn85xx_ts->ictype == ICN85XX_WITHOUT_FLASH) {
		if (bl_is_delay) {
			resume_download_task = kthread_create(resume_download_thread, icn85xx_ts, "resume_download");
			if(IS_ERR(resume_download_task)) {
				icn85xx_error("cread thread failed\n");	
			}
			wake_up_process(resume_download_task);
		} else
			resume_download_thread(icn85xx_ts);
	} else {
		icn85xx_write_reg(ICN85xx_REG_PMODE, 0xff);
		icn85xx_ts_reset();
		if (icn85xx_ts->use_irq) {
			icn85xx_irq_enable();
		} else {
			hrtimer_start(&icn85xx_ts->timer, ktime_set(CTP_START_TIMER/1000, \
						(CTP_START_TIMER%1000)*1000000), HRTIMER_MODE_REL);
		}
	}
    
   // icn85xx_write_reg(ICN85xx_REG_PMODE, 0x00); 
	return 0;
}

/***********************************************************************************************
Name    :   icn85xx_request_io_port
Input   :   void
Output  :   
function    : 0 success,
***********************************************************************************************/
static int icn85xx_request_io_port(struct icn85xx_ts_data *icn85xx_ts)
{

#if SUPPORT_ROCKCHIP
    icn85xx_ts->screen_max_x = SCREEN_MAX_X;
    icn85xx_ts->screen_max_y = SCREEN_MAX_Y;
    icn85xx_ts->irq = CTP_IRQ_PORT; //maybe need changed
#endif
	icn85xx_ts->irq = IRQ_GPIO;

	if (gpio_request(g_param.rstgpio, "ts_rst") < 0) {
		printk("gpio(%d) touchscreen reset request fail\n", g_param.rstgpio);
		return -EIO;
	}
	gpio_direction_output(g_param.rstgpio, 1);

	if (gpio_request(g_param.irqgpio, "ts_irq") < 0) {
		printk("gpio(%d) touchscreen interrupt request fail\n", g_param.irqgpio);
		gpio_free(g_param.rstgpio);
		return -EIO;
	}
	wmt_gpio_setpull(g_param.irqgpio, WMT_GPIO_PULL_UP);
	gpio_direction_input(g_param.irqgpio);


    return 0;

}

/***********************************************************************************************
Name    :   icn85xx_free_io_port
Input   :   void
Output  :   
function    : 0 success,
***********************************************************************************************/
static void icn85xx_free_io_port(void)
{    
	gpio_free(g_param.rstgpio);
	gpio_free(g_param.irqgpio);
	return;
}

/***********************************************************************************************
Name    :   icn85xx_request_irq
Input   :   void
Output  :   
function    : 0 success,
***********************************************************************************************/
static int icn85xx_request_irq(struct icn85xx_ts_data *icn85xx_ts)
{
    int err = -1;


	wmt_gpio_set_irq_type(g_param.irqgpio, IRQ_TYPE_EDGE_FALLING);
	err = request_irq(icn85xx_ts->irq, icn85xx_ts_interrupt, IRQF_SHARED, "icn85xx_ts", icn85xx_ts);
	if (err < 0) 
	{
		icn85xx_error("icn85xx_ts_probe: request irq failed\n");
		return err;
	} 
	else
	{
		icn85xx_irq_disable();
		icn85xx_ts->use_irq = 1;        
	} 

#if SUPPORT_ROCKCHIP
    err = gpio_request(icn85xx_ts->irq, "TS_INT"); //Request IO
    if (err < 0)
    {
        icn85xx_error("Failed to request GPIO:%d, ERRNO:%d\n", (int)icn85xx_ts->irq, err);
        return err;
    }
    gpio_direction_input(icn85xx_ts->irq);
    err = request_irq(icn85xx_ts->irq, icn85xx_ts_interrupt, IRQ_TYPE_EDGE_FALLING, "icn85xx_ts", icn85xx_ts);
    if (err < 0) 
    {
        icn85xx_ts->use_irq = 0;
		icn85xx_error("icn85xx_ts_probe: request irq failed\n");
        return err;
    } 
    else
    {
        icn85xx_irq_disable();
        icn85xx_ts->use_irq = 1;        
    } 
#endif

    return 0;
}


/***********************************************************************************************
Name    :   icn85xx_free_irq
Input   :   void
Output  :   
function    : 0 success,
***********************************************************************************************/
static void icn85xx_free_irq(struct icn85xx_ts_data *icn85xx_ts)
{
    if (icn85xx_ts) 
    {
        if (icn85xx_ts->use_irq)
        {
            free_irq(icn85xx_ts->irq, icn85xx_ts);
        }
        else
        {
            hrtimer_cancel(&icn85xx_ts->timer);
        }
    }
     
}

/***********************************************************************************************
Name    :   icn85xx_request_input_dev
Input   :   void
Output  :   
function    : 0 success,
***********************************************************************************************/
static int icn85xx_request_input_dev(struct icn85xx_ts_data *icn85xx_ts)
{
    int ret = -1;    
    struct input_dev *input_dev;

    input_dev = input_allocate_device();
    if (!input_dev) {
        icn85xx_error("failed to allocate input device\n");
        return -ENOMEM;
    }
    icn85xx_ts->input_dev = input_dev;

    icn85xx_ts->input_dev->evbit[0] = BIT_MASK(EV_SYN) | BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS) ;
#if CTP_REPORT_PROTOCOL
    __set_bit(INPUT_PROP_DIRECT, icn85xx_ts->input_dev->propbit);
    input_mt_init_slots(icn85xx_ts->input_dev, POINT_NUM*2);
#else
    set_bit(ABS_MT_TOUCH_MAJOR, icn85xx_ts->input_dev->absbit);
    set_bit(ABS_MT_POSITION_X, icn85xx_ts->input_dev->absbit);
    set_bit(ABS_MT_POSITION_Y, icn85xx_ts->input_dev->absbit);
    set_bit(ABS_MT_WIDTH_MAJOR, icn85xx_ts->input_dev->absbit); 
#endif
	if (g_param.lcd_exchg) {
		input_set_abs_params(icn85xx_ts->input_dev, ABS_MT_POSITION_X, 0, g_param.panelres_y, 0, 0);
		input_set_abs_params(icn85xx_ts->input_dev, ABS_MT_POSITION_Y, 0, g_param.panelres_x, 0, 0);
	} else {
		input_set_abs_params(icn85xx_ts->input_dev, ABS_MT_POSITION_X, 0, g_param.panelres_x, 0, 0);
		input_set_abs_params(icn85xx_ts->input_dev, ABS_MT_POSITION_Y, 0, g_param.panelres_y, 0, 0);
	}
    input_set_abs_params(icn85xx_ts->input_dev, ABS_MT_WIDTH_MAJOR, 0, 255, 0, 0);
    input_set_abs_params(icn85xx_ts->input_dev, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);  
    input_set_abs_params(icn85xx_ts->input_dev, ABS_MT_TRACKING_ID, 0, POINT_NUM*2, 0, 0);

    __set_bit(KEY_MENU,  input_dev->keybit);
    __set_bit(KEY_BACK,  input_dev->keybit);
    __set_bit(KEY_HOME,  input_dev->keybit);
    __set_bit(KEY_SEARCH,  input_dev->keybit);

    input_dev->name = CTP_NAME;
    ret = input_register_device(input_dev);
    if (ret) {
        icn85xx_error("Register %s input device failed\n", input_dev->name);
        input_free_device(input_dev);
        return -ENODEV;        
    }
    
#ifdef CONFIG_HAS_EARLYSUSPEND
    icn85xx_trace("==register_early_suspend =\n");
    icn85xx_ts->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
    icn85xx_ts->early_suspend.suspend = icn85xx_ts_suspend;
    icn85xx_ts->early_suspend.resume  = icn85xx_ts_resume;
    register_early_suspend(&icn85xx_ts->early_suspend);
#endif

    return 0;
}
#if SUPPORT_SENSOR_ID
static void read_sensor_id(void)
{
        int i,ret;
		//icn85xx_trace("scan sensor id value begin sensor_id_num = %d\n",(sizeof(sensor_id_table)/sizeof(sensor_id_table[0])));
        ret = icn85xx_read_reg(0x10, &cursensor_id);
        if(ret > 0)
	        {	
	          icn85xx_trace("cursensor_id= 0x%x\n", cursensor_id); 
	        }
		else
			{
			  icn85xx_error("icn85xx read cursensor_id failed.\n");
			  cursensor_id = -1;
			}

		 ret = icn85xx_read_reg(0x1e, &tarsensor_id);
        if(ret > 0)
	        {	
	          icn85xx_trace("tarsensor_id= 0x%x\n", tarsensor_id);
			  tarsensor_id = -1;
	        }
		else
			{
			  icn85xx_error("icn85xx read tarsensor_id failed.\n");
			}
		 ret = icn85xx_read_reg(0x1f, &id_match);
        if(ret > 0)
	        {	
	          icn85xx_trace("match_flag= 0x%x\n", id_match); // 1: match; 0:not match
		    }
		else
			{
			  icn85xx_error("icn85xx read id_match failed.\n");
			  id_match = -1;
			}
		// scan sensor id value
	icn85xx_trace("begin to scan id table,find correct fw or bin. sensor_id_num = %d\n",(sizeof(sensor_id_table)/sizeof(sensor_id_table[0])));
   
   for(i = 0;i < (sizeof(sensor_id_table)/sizeof(sensor_id_table[0])); i++)  // not change tp
	{
        if (cursensor_id == sensor_id_table[i].value)
        	{
                #if COMPILE_FW_WITH_DRIVER
				    icn85xx_set_fw(sensor_id_table[i].size, sensor_id_table[i].fw_name);	
				#else
				    strcpy(firmware,sensor_id_table[i].bin_name);
					icn85xx_trace("icn85xx matched firmware = %s\n", firmware);
				#endif
				    icn85xx_trace("icn85xx matched id = 0x%x\n", sensor_id_table[i].value);
				    invalid_id = 1;
				    break;
        	}
		else
			{ 
			  invalid_id = 0;
			  icn85xx_trace("icn85xx not matched id%d= 0x%x\n", i,sensor_id_table[i].value);
			  //icn85xx_trace("not match sensor_id_table[%d].value= 0x%x,bin_name = %s\n",i,sensor_id_table[i].value,sensor_id_table[i].bin_name);
			}
     }
		
}
static void compare_sensor_id(void)
{
   int retry = 5;
   
   read_sensor_id(); // select sensor id 
   
   if(0 == invalid_id)   //not compare sensor id,update default fw or bin
   	{
   	  icn85xx_trace("not compare sensor id table,update default: invalid_id= %d, cursensor_id= %d\n", invalid_id,cursensor_id);
	  #if COMPILE_FW_WITH_DRIVER
	           icn85xx_set_fw(sensor_id_table[0].size, sensor_id_table[0].fw_name);
	  #else
   	            strcpy(firmware,sensor_id_table[0].bin_name);
				icn85xx_trace("match default firmware = %s\n", firmware);
	  #endif
   	
      while(retry > 0)
            {
				if(R_OK == icn85xx_fw_update(firmware))
                {
                    icn85xx_trace("icn85xx upgrade default firmware ok\n");
					break;
                }
                retry--;
                icn85xx_error("icn85xx_fw_update default firmware failed.\n");   
            }
   	}
   
   if ((1 == invalid_id)&&(0 == id_match))  // tp is changed,update current fw or bin
   	{
        icn85xx_trace("icn85xx detect tp is changed!!! invalid_id= %d,id_match= %d,\n", invalid_id,id_match);
		 while(retry > 0)
            {
				if(R_OK == icn85xx_fw_update(firmware))
                {
                    icn85xx_trace("icn85xx upgrade cursensor id firmware ok\n");
					break;
                }
                retry--;
                icn85xx_error("icn85xx_fw_update current id firmware failed.\n");   
            }
    }
}
#endif

static void icn85xx_update(struct icn85xx_ts_data *icn85xx_ts)
{
    short fwVersion = 0;
    short curVersion = 0;
	int retry = 0;
	
	if(icn85xx_ts->ictype == ICN85XX_WITHOUT_FLASH)
    {
        #if (COMPILE_FW_WITH_DRIVER && !SUPPORT_SENSOR_ID)
            icn85xx_set_fw(sizeof(icn85xx_fw), &icn85xx_fw[0]);
        #endif
		
		#if SUPPORT_SENSOR_ID
		while(0 == invalid_id ) //reselect sensor id  
      	{
     	  compare_sensor_id();   // select sensor id
		  icn85xx_trace("invalid_id= %d\n", invalid_id);
     	}
		#else
        if(R_OK == icn85xx_fw_update(firmware))
        {
			icn85xx_ts->code_loaded_flag = 1;
            icn85xx_trace("ICN85XX_WITHOUT_FLASH, update default fw ok\n");
        }
        else
        {
            icn85xx_ts->code_loaded_flag = 0;
            icn85xx_trace("ICN85XX_WITHOUT_FLASH, update error\n"); 
        }
		#endif

    }
    else if(icn85xx_ts->ictype == ICN85XX_WITH_FLASH)
    {
        #if (COMPILE_FW_WITH_DRIVER && !SUPPORT_SENSOR_ID)
            icn85xx_set_fw(sizeof(icn85xx_fw), &icn85xx_fw[0]);
        #endif 
		
		#if SUPPORT_SENSOR_ID
		while(0 == invalid_id ) //reselect sensor id  
      	{
     	  compare_sensor_id();   // select sensor id
     	  if( 1 == invalid_id)
     	  	{
		      icn85xx_trace("select sensor id ok. begin compare fwVersion with curversion\n");
     	  	}
     	}
		#endif
        
        fwVersion = icn85xx_read_fw_Ver(firmware);
        curVersion = icn85xx_readVersion();
        icn85xx_trace("fwVersion : 0x%x\n", fwVersion); 
        icn85xx_trace("current version: 0x%x\n", curVersion);  

        #if FORCE_UPDATA_FW
	        retry = 5;
	        while(retry > 0)
	        {
			    if(icn85xx_goto_progmode() != 0)
			    {
					printk("icn85xx_goto_progmode() != 0 error\n");
			        return -1; 
			    } 
	        	icn85xx_read_flashid();
	            if(R_OK == icn85xx_fw_update(firmware))
	            {
					break;
	            }
	            retry--;
	            icn85xx_error("icn85xx_fw_update failed.\n");        
	        }

        #else
           if(fwVersion > curVersion)
           {
	            retry = 5;
	            while(retry > 0)
		            {
						if(R_OK == icn85xx_fw_update(firmware))
		                {
							break;
		                }
		                retry--;
		                icn85xx_error("icn85xx_fw_update failed.\n");   
		            }
           }
        #endif
    }
}
static int icn85xx_ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    struct icn85xx_ts_data *icn85xx_ts;
    int err = 0;

    icn85xx_trace("====%s begin=====.  \n", __func__);
    if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) 
    {
        icn85xx_error("I2C check functionality failed.\n");
        return -ENODEV;
    }

    icn85xx_ts = kzalloc(sizeof(*icn85xx_ts), GFP_KERNEL);
    if (!icn85xx_ts)
    {
        icn85xx_error("Alloc icn85xx_ts memory failed.\n");
        return -ENOMEM;
    }
    memset(icn85xx_ts, 0, sizeof(*icn85xx_ts));

    this_client = client;
    this_client->addr = client->addr;
    i2c_set_clientdata(client, icn85xx_ts);

    icn85xx_ts->work_mode = 0;
    spin_lock_init(&icn85xx_ts->irq_lock);
//    icn85xx_ts->irq_lock = SPIN_LOCK_UNLOCKED;

    icn85xx_request_io_port(icn85xx_ts);
	if (err != 0) {
		icn85xx_error("icn85xx_request_io_port failed.\n");
		goto fail1;
	}
	memset(firmware, 0, 128);
	sprintf(firmware,"%s.bin",g_param.fw_name);

    icn85xx_ts_reset();

    err = icn85xx_iic_test();
    if (err <= 0)
    {
        icn85xx_error("icn85xx_iic_test  failed.\n");
		goto fail2;
     
    }
    else
    {
        icn85xx_trace("iic communication ok: 0x%x\n", icn85xx_ts->ictype); 
    }
	
    icn85xx_update(icn85xx_ts);

    err= icn85xx_request_input_dev(icn85xx_ts);
    if (err < 0)
    {
        icn85xx_error("request input dev failed\n");
		goto fail3;
    }

	
  
#if TOUCH_VIRTUAL_KEYS
    icn85xx_ts_virtual_keys_init();
#endif
    err = icn85xx_request_irq(icn85xx_ts);
    if (err != 0)
    {
        icn85xx_error("request irq error, use timer\n");
        icn85xx_ts->use_irq = 0;
        hrtimer_init(&icn85xx_ts->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
        icn85xx_ts->timer.function = chipone_timer_func;
        hrtimer_start(&icn85xx_ts->timer, ktime_set(CTP_START_TIMER/1000, (CTP_START_TIMER%1000)*1000000), HRTIMER_MODE_REL);
    }
#if SUPPORT_SYSFS
    icn85xx_create_sysfs(client);
#endif

#if SUPPORT_PROC_FS
    sema_init(&icn85xx_ts->sem, 1);
    init_proc_node();
#endif

    INIT_WORK(&icn85xx_ts->pen_event_work, icn85xx_ts_pen_irq_work);
    icn85xx_ts->ts_workqueue = create_singlethread_workqueue(dev_name(&client->dev));
    if (!icn85xx_ts->ts_workqueue) {
        icn85xx_error("create_singlethread_workqueue failed.\n");
        err = -ESRCH;
		goto fail4;
    }

    if(icn85xx_ts->use_irq)
        icn85xx_irq_enable();
    icn85xx_trace("==%s over =\n", __func__);    
    return 0;
fail4:
	cancel_work_sync(&icn85xx_ts->pen_event_work);
fail3:
	input_unregister_device(icn85xx_ts->input_dev);
	input_free_device(icn85xx_ts->input_dev);
fail2:
    icn85xx_free_io_port();
fail1:
	kfree(icn85xx_ts);
	icn85xx_free_fw();
	return err;
}

static int __devexit icn85xx_ts_remove(struct i2c_client *client)
{
    struct icn85xx_ts_data *icn85xx_ts = i2c_get_clientdata(client);  
    icn85xx_trace("==icn85xx_ts_remove=\n");
    if(icn85xx_ts->use_irq)
        icn85xx_irq_disable();
#ifdef CONFIG_HAS_EARLYSUSPEND
    unregister_early_suspend(&icn85xx_ts->early_suspend);
#endif

#if SUPPORT_PROC_FS
    uninit_proc_node();
#endif

#if SUPPORT_SYSFS
    icn85xx_remove_sysfs(client);
#endif
    input_unregister_device(icn85xx_ts->input_dev);
    input_free_device(icn85xx_ts->input_dev);
    cancel_work_sync(&icn85xx_ts->pen_event_work);
    destroy_workqueue(icn85xx_ts->ts_workqueue);
    icn85xx_free_irq(icn85xx_ts);
    icn85xx_free_io_port();
	icn85xx_free_fw();
    kfree(icn85xx_ts);    
    i2c_set_clientdata(client, NULL);
    return 0;
}

static int wmt_check_touch_env(void)
{
	int ret = 0;
	int len = 96;
	char retval[200] = {0},*p=NULL,*s=NULL;
	int Enable=0;

	ret = wmt_getsyspara("wmt.io.touch", retval, &len);
	if(ret){
		printk("Read wmt.io.touch Failed.\n");
		return -EIO;
	}

//paste:
	p = retval;
	Enable = (p[0] - '0' == 1) ? 1 : 0;
	if(Enable == 0){
		printk("Touch Screen Is Disabled.\n");
		return -ENODEV;
	}

	p = strchr(p,':');p++;
	s = strchr(p,':');
	strncpy(g_param.fw_name,p, (s-p));
	printk("ts_name=%s\n", g_param.fw_name);
	if (strncmp(g_param.fw_name, "ICN85", 5)) {
		printk("Wrong firmware name.\n");
		return -ENODEV;
	}

	p = s+1;
	ret = sscanf(p,"%d:%d:%d:%d:%d:%d:%d",
			&(g_param.irqgpio),&(g_param.panelres_x),&(g_param.panelres_y),&(g_param.rstgpio),
			&(g_param.xyswap),&(g_param.xdir),&(g_param.ydir));

	if (ret < 7) {
		printk("Wrong format ts u-boot param(%d)!\nwmt.io.touch=%s\n",ret,retval);
		return -ENODEV;
	}

	printk("p.x = %d, p.y = %d, irqgpio=%d, rstgpio=%d,xyswap=%d,xdir=%d,ydir=%d\n",
			g_param.panelres_x,g_param.panelres_y,g_param.irqgpio,g_param.rstgpio,
			g_param.xyswap,g_param.xdir,g_param.ydir);

	/*memset(retval,0,sizeof(retval));
	ret = wmt_getsyspara("wmt.touch.earlysus", retval, &len);
	if(!ret) 
		g_param.earlysus_en = (retval[0] - '0' == 1) ? 1 : 0;*/

	memset(retval,0,sizeof(retval));
	ret = wmt_getsyspara("wmt.display.fb0", retval, &len);
	if (!ret) {
		int tmp[6];
		p = retval;
		sscanf(p, "%d:[%d:%d:%d:%d:%d", &tmp[0], &tmp[1], &tmp[2], &tmp[3], &tmp[4], &tmp[5]);
		if (tmp[4] > tmp[5])
			g_param.lcd_exchg = 1;
	}

	ret = wmt_getsyspara("wmt.backlight.delay", retval, &len);
	if(ret) {
		bl_is_delay = 0;
	} else
		bl_is_delay = 1;

	return 0;
}

static const struct i2c_device_id icn85xx_ts_id[] = {
    { CTP_NAME, 0 },
    {}
};
MODULE_DEVICE_TABLE(i2c, icn85xx_ts_id);

static struct i2c_driver icn85xx_ts_driver = {
    .probe      = icn85xx_ts_probe,
    .remove     = __devexit_p(icn85xx_ts_remove),
#ifndef CONFIG_HAS_EARLYSUSPEND
    .suspend    = icn85xx_ts_suspend,
    .resume     = icn85xx_ts_resume,
#endif
    .id_table   = icn85xx_ts_id,
    .driver = {
        .name   = CTP_NAME,
        .owner  = THIS_MODULE,
    },
   
};


static struct i2c_board_info i2c_board_info = {
	I2C_BOARD_INFO(CTP_NAME, ICN85XX_IIC_ADDR),
};


static int __init icn85xx_ts_init(void)
{ 
	struct i2c_client *client;
	struct i2c_adapter *adap;
    icn85xx_trace("===========================%s=====================\n", __func__);

	if(wmt_check_touch_env())
		return -ENODEV;
   
	{//register i2c device
		adap = i2c_get_adapter(1); //i2c Bus 1
		if (!adap)
			return -ENODEV;
		client = i2c_new_device(adap, &i2c_board_info);
		i2c_put_adapter(adap);
		if (!client) {
			printk("i2c_new_device error\n");
			return -ENODEV;
		}
	}

    return i2c_add_driver(&icn85xx_ts_driver);
}

static void __exit icn85xx_ts_exit(void)
{
    icn85xx_trace("==icn85xx_ts_exit==\n");
	i2c_unregister_device(this_client);
    i2c_del_driver(&icn85xx_ts_driver);
}
late_initcall(icn85xx_ts_init);
//module_init(icn85xx_ts_init);
module_exit(icn85xx_ts_exit);

MODULE_AUTHOR("<zmtian@chiponeic.com>");
MODULE_DESCRIPTION("Chipone icn85xx TouchScreen driver");
MODULE_LICENSE("GPL");

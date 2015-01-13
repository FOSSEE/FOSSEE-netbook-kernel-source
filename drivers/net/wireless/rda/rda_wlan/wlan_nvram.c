#include <linux/fs.h>
#include <asm/uaccess.h>
#include "wlan_includes.h"

#ifdef USE_MAC_FROM_RDA_NVRAM
#include <plat/md_sys.h>
#endif

#define WIFI_NVRAM_FILE_NAME "/data/misc/wifi/WLANMAC"

static int nvram_read(char *filename, char *buf, ssize_t len, int offset)
{   
    struct file *fd;
    int retLen = -1;
    
    mm_segment_t old_fs = get_fs();
    set_fs(KERNEL_DS);  
    fd = filp_open(filename, O_WRONLY|O_CREAT, 0644);
    
    if(IS_ERR(fd)){
        printk("[wlan][nvram_read] : failed to open!!\n");
        return -1;
    }
    
    do{
        if ((fd->f_op == NULL) || (fd->f_op->read == NULL)){
            printk("[wlan][nvram_read] : file can not be read!!\n");
            break;
        } 
            
        if (fd->f_pos != offset){
            if (fd->f_op->llseek){
                if(fd->f_op->llseek(fd, offset, 0) != offset){
                    printk("[wlan][nvram_read] : failed to seek!!\n");
                    break;
                }
            }else{
                fd->f_pos = offset;
            }
        }           

        retLen = fd->f_op->read(fd, buf, len, &fd->f_pos);                  
    }while(false);
    
    filp_close(fd, NULL);  
    set_fs(old_fs);
    
    return retLen;
}

static int nvram_write(char *filename, char *buf, ssize_t len, int offset)
{   
    struct file *fd;
    int retLen = -1;
        
    mm_segment_t old_fs = get_fs();
    set_fs(KERNEL_DS);
    
    fd = filp_open(filename, O_WRONLY|O_CREAT, 0644);
    
    if(IS_ERR(fd)){
        printk("[wlan][nvram_write] : failed to open!!\n");
        return -1;
    }
    
    do{
        if ((fd->f_op == NULL) || (fd->f_op->write == NULL)){
            printk("[wlan][nvram_write] : file can not be write!!\n");
            break;
        } 
            
        if (fd->f_pos != offset){
            if (fd->f_op->llseek){
                if(fd->f_op->llseek(fd, offset, 0) != offset){
                    printk("[wlan][nvram_write] : failed to seek!!\n");
                    break;
                }
            }else{
                fd->f_pos = offset;
            }
        }               
        
        retLen = fd->f_op->write(fd, buf, len, &fd->f_pos);         
            
    }while(false);
    
    filp_close(fd, NULL);
    set_fs(old_fs);
    
    return retLen;
}

int wlan_read_mac_from_file(char* buf)
{
    return nvram_read(WIFI_NVRAM_FILE_NAME, buf, 6, 0);
}

int wlan_write_mac_to_file(char * buf)
{
    return nvram_write(WIFI_NVRAM_FILE_NAME, buf, 6, 0);
}

#ifdef USE_MAC_FROM_RDA_NVRAM
int wlan_read_mac_from_nvram(char *buf)
{
	int ret;
	struct msys_device *wlan_msys = NULL;
	struct wlan_mac_info wlan_info;
	struct client_cmd cmd_set;

	wlan_msys = rda_msys_alloc_device();
	if (!wlan_msys) {
		WLAN_ERRP("nvram: can not allocate wlan_msys device\n");
		ret = -ENOMEM;
		goto err_handle_sys;
	}

	wlan_msys->module = SYS_GEN_MOD;
	wlan_msys->name = "rda-wlan";
	rda_msys_register_device(wlan_msys);

	memset(&wlan_info, sizeof(wlan_info), 0);
	cmd_set.pmsys_dev = wlan_msys;
	cmd_set.mod_id = SYS_GEN_MOD;
	cmd_set.mesg_id = SYS_GEN_CMD_GET_WIFI_INFO;
	cmd_set.pdata = NULL;
	cmd_set.data_size = 0;
	cmd_set.pout_data = &wlan_info;
	cmd_set.out_size = sizeof(wlan_info);

	ret = rda_msys_send_cmd(&cmd_set);
	if (ret) {
		WLAN_ERRP("nvram:can not get wifi mac from nvram \n");
		ret = -EBUSY;
		goto err_handle_cmd;
	}

	if (wlan_info.activated != WIFI_MAC_ACTIVATED_FLAG) {
		WLAN_ERRP("nvram:get invalid wifi mac address from nvram\n");
		ret = -EINVAL;
		goto err_invalid_mac;
	}

	memcpy(buf, wlan_info.mac_addr, ETH_ALEN);
	WLAN_DBGLAP(WLAN_DA_MAIN, WLAN_DL_CRIT,
		    "nvram:get wifi mac address [%02x:%02x:%02x:%02x:%02x:%02x] from nvram success.\n",
		    buf[0], buf[1], buf[2],
		    buf[3], buf[4], buf[5]);
	ret = 0; /* success*/

err_invalid_mac:
err_handle_cmd:
	rda_msys_unregister_device(wlan_msys);
	rda_msys_free_device(wlan_msys);
err_handle_sys:
	return ret;
}

int wlan_write_mac_to_nvram(const char *buf)
{
	int ret;
	struct msys_device *wlan_msys = NULL;
	struct wlan_mac_info wlan_info;
	struct client_cmd cmd_set;

	wlan_msys = rda_msys_alloc_device();
	if (!wlan_msys) {
		WLAN_ERRP("nvram: can not allocate wlan_msys device\n");
		ret = -ENOMEM;
		goto err_handle_sys;
	}

	wlan_msys->module = SYS_GEN_MOD;
	wlan_msys->name = "rda-wlan";
	rda_msys_register_device(wlan_msys);

	memset(&wlan_info, sizeof(wlan_info), 0);
	wlan_info.activated = WIFI_MAC_ACTIVATED_FLAG;
	memcpy(wlan_info.mac_addr, buf, ETH_ALEN);

	cmd_set.pmsys_dev = wlan_msys;
	cmd_set.mod_id = SYS_GEN_MOD;
	cmd_set.mesg_id = SYS_GEN_CMD_SET_WIFI_INFO;
	cmd_set.pdata = &wlan_info;
	cmd_set.data_size = sizeof(wlan_info);
	cmd_set.pout_data = NULL;
	cmd_set.out_size = 0;

	ret = rda_msys_send_cmd(&cmd_set);
	if (ret) {
		WLAN_ERRP("nvram:can not set wifi mac to nvram \n");
		ret = -EBUSY;
		goto err_handle_cmd;
	}

	WLAN_DBGLAP(WLAN_DA_MAIN, WLAN_DL_CRIT,
		    "nvram:set wifi mac address [%02x:%02x:%02x:%02x:%02x:%02x] to nvram success.\n",
		    buf[0], buf[1], buf[2],
		    buf[3], buf[4], buf[5]);
	ret = 0; /* success*/

err_handle_cmd:
	rda_msys_unregister_device(wlan_msys);
	rda_msys_free_device(wlan_msys);
err_handle_sys:
	return ret;
}
#endif /*USE_MAC_FROM_RDA_NVRAM*/


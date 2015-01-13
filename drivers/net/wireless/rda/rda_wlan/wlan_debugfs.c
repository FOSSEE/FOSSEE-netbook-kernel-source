#include "wlan_includes.h"

static int open_file_generic(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

#define FOPS(fread, fwrite) { \
	.owner = THIS_MODULE, \
	.open = open_file_generic, \
	.read = (fread), \
	.write = (fwrite), \
}

struct wlan_debugfs_files {
	char *name;
	int perm;
	struct file_operations fops;
};

extern int wlan_dbg_level;
extern int wlan_dbg_area;
static struct dentry *wlan_dbg_dir = NULL;

static ssize_t wlan_debugarea_read(struct file *file, char __user *userbuf,
				  size_t count, loff_t *ppos)
{
	size_t pos = 0;
	unsigned long addr = get_zeroed_page(GFP_KERNEL);
	char *buf = (char *)addr;
	ssize_t res;

	WLAN_DBGLAP(WLAN_DA_SDIO, WLAN_DL_TRACE,
		"%s\n", __func__);

	WLAN_DBGLAP(WLAN_DA_SDIO, WLAN_DL_TRACE,
		"get debug_area = 0x%x\n",wlan_dbg_area);

	pos += snprintf(buf+pos, PAGE_SIZE - pos, "%x\n",
				wlan_dbg_area);

	res = simple_read_from_buffer(userbuf, count, ppos, buf, pos);

	free_page(addr);
	return res;
}

static ssize_t wlan_debugarea_write(struct file *file,
				const char __user *user_buf, size_t count,
				loff_t *ppos)
{
	ssize_t ret;
	int debug_area;
	unsigned long addr = get_zeroed_page(GFP_KERNEL);
	char *buf = (char *)addr;

	WLAN_DBGLAP(WLAN_DA_SDIO, WLAN_DL_TRACE,
		"%s count:%d \n", __func__, count);

	if (copy_from_user(buf, user_buf, count)) {
		ret = -EFAULT;
		goto out_unlock;
	}
	ret = sscanf(buf, "%x", &debug_area);
	if (ret != 1) {
		ret = -EINVAL;
		goto out_unlock;
	}

    wlan_dbg_area = debug_area;
	WLAN_DBGLAP(WLAN_DA_SDIO, WLAN_DL_TRACE,
		"set debug_area = 0x%x\n",wlan_dbg_area);

	ret = count;
out_unlock:
	free_page(addr);
	return ret;
}

static ssize_t wlan_debuglevel_read(struct file *file, char __user *userbuf,
				  size_t count, loff_t *ppos)
{
	size_t pos = 0;
	unsigned long addr = get_zeroed_page(GFP_KERNEL);
	char *buf = (char *)addr;
	ssize_t res;

	WLAN_DBGLAP(WLAN_DA_SDIO, WLAN_DL_TRACE,
		"%s\n", __func__);

	WLAN_DBGLAP(WLAN_DA_SDIO, WLAN_DL_TRACE,
		"get debug_level = 0x%x\n",wlan_dbg_level);

	pos += snprintf(buf+pos, PAGE_SIZE - pos, "%x\n",
				wlan_dbg_level);

	res = simple_read_from_buffer(userbuf, count, ppos, buf, pos);

	free_page(addr);
	return res;
}

static ssize_t wlan_debuglevel_write(struct file *file,
				const char __user *user_buf, size_t count,
				loff_t *ppos)
{
	ssize_t ret;
	int debug_level;
	unsigned long addr = get_zeroed_page(GFP_KERNEL);
	char *buf = (char *)addr;

	WLAN_DBGLAP(WLAN_DA_SDIO, WLAN_DL_TRACE,
		"%s\n", __func__);

	if (copy_from_user(buf, user_buf, count)) {
		ret = -EFAULT;
		goto out_unlock;
	}
	ret = sscanf(buf, "%x", &debug_level);
	if (ret != 1) {
		ret = -EINVAL;
		goto out_unlock;
	}

    wlan_dbg_level = debug_level;
    
    WLAN_DBGLAP(WLAN_DA_SDIO, WLAN_DL_TRACE,
		"set debug_level = 0x%x\n",wlan_dbg_level);

	ret = count;
out_unlock:
	free_page(addr);
	return ret;
}

#if 0
static ssize_t wlan_loopback_read(struct file *file, char __user *userbuf,
				  size_t count, loff_t *ppos)
{
	ssize_t ret;
	ret = 0;
	return ret;
}

static ssize_t wlan_loopback_write(struct file *file, char __user *userbuf,
				  size_t count, loff_t *ppos)
{
	ssize_t ret;
	ret = 0;
	return ret;
}
#endif

static struct wlan_debugfs_files debugfs_files[] = {
	{ "debugarea", 0444, FOPS(wlan_debugarea_read, wlan_debugarea_write), },
    { "debuglevel", 0444, FOPS(wlan_debuglevel_read, wlan_debuglevel_write), },
    //{ "loopback", 0444, FOPS(wlan_loopback_read, wlan_loopback_write), },
};


void wlan_debugfs_init(void)
{
	WLAN_DBGLAP(WLAN_DA_SDIO, WLAN_DL_DEBUG,
		"%s\n", __func__);

	if (!wlan_dbg_dir)
		wlan_dbg_dir = debugfs_create_dir("rdawlan", NULL);

	return;
}

void wlan_debugfs_remove(void)
{
	WLAN_DBGLAP(WLAN_DA_SDIO, WLAN_DL_DEBUG,
		"%s\n", __func__);

	if (wlan_dbg_dir)
		 debugfs_remove(wlan_dbg_dir);

    wlan_dbg_dir = NULL;
       
	return;
}

void wlan_debugfs_init_all(wlan_private *priv)
{
	int i;
	struct wlan_debugfs_files *files;
	if (!wlan_dbg_dir)
		goto exit;

	WLAN_DBGLAP(WLAN_DA_SDIO, WLAN_DL_DEBUG,
		"%s\n", __func__);

	priv->debugfs_dir = debugfs_create_dir("rdawlan_dev", wlan_dbg_dir);
	if (!priv->debugfs_dir)
		goto exit;

	for (i=0; i<ARRAY_SIZE(debugfs_files); i++) {
		files = &debugfs_files[i];
		priv->debugfs_files[i] = debugfs_create_file(files->name,
							     files->perm,
							     priv->debugfs_dir,
							     priv,
							     &files->fops);
	}

exit:
	return;
}

void wlan_debugfs_remove_all(wlan_private *priv)
{
	int i;

	WLAN_DBGLAP(WLAN_DA_SDIO, WLAN_DL_DEBUG,
		"%s\n", __func__);

	for(i=0; i<ARRAY_SIZE(debugfs_files); i++)
		debugfs_remove(priv->debugfs_files[i]);
	debugfs_remove(priv->debugfs_dir);
}



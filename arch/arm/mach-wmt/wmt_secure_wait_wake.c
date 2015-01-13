#include <linux/init.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/wait.h>
#include <linux/workqueue.h>
#include <linux/sched.h>

#include <mach/wmt_secure.h>
#include <mach/wmt_env.h>

#define WMT_SMC_CMD_WW_BASE    60
#define WMT_SMC_CMD_WAIT_STUB  (WMT_SMC_CMD_WW_BASE + 1)
#define WMT_SMC_CMD_WAKE_STUB  (WMT_SMC_CMD_WW_BASE + 2)
#define WMT_SMC_CMD_WAKE_BEGIN (WMT_SMC_CMD_WW_BASE + 3)
#define WMT_SMC_CMD_WAKE_DONE  (WMT_SMC_CMD_WW_BASE + 4)
#define WMT_SMC_CMD_WW_TEST    (WMT_SMC_CMD_WW_BASE + 5)

DECLARE_WAIT_QUEUE_HEAD(wmt_smc_wait);
static int smc_done = 0;

static void wmt_smc_wait_stub(void)
{
	wait_event_interruptible(wmt_smc_wait, smc_done);
	smc_done = 0;
	wmt_smc(WMT_SMC_CMD_WAKE_DONE, 0);
}

static void wmt_smc_wake_stub(void)
{
	/* irq already been disabled in secure world */
	smc_done = 1;
	wake_up_interruptible(&wmt_smc_wait);
	wmt_smc(WMT_SMC_CMD_WAKE_BEGIN, 0);
}

static void wmt_setup_wait_stub(u32 wait_stub)
{
	wmt_smc(WMT_SMC_CMD_WAIT_STUB, wait_stub);
}

static void wmt_setup_wake_stub(u32 wake_stub)
{
	wmt_smc(WMT_SMC_CMD_WAKE_STUB, wake_stub);
}

static int __init wmt_check_secure_env(void)
{
    int ret = 0;
    int varlen = 128;
    unsigned int sec_en = 0;
    unsigned char buf[128] = {0};

    /* uboot env name is: wmt.secure.param */
    ret = wmt_getsyspara("wmt.secure.param", buf, &varlen);                                                                    
    if (ret) {
        ret = -ENODATA;
        goto out;
    }

    sscanf(buf, "%d", &sec_en);
    if (sec_en != 1) {
        printk(KERN_INFO "wmt security extension disaled\n");
        ret = -ENODEV;
        goto out;
    }

out:
    return ret;
}
static int __init wmt_secure_wait_wake_init(void)
{
	/* if secure os disabled, we do not setup stubs */
	if (wmt_check_secure_env())
		return -EINVAL;

	wmt_setup_wait_stub((u32)wmt_smc_wait_stub);
	wmt_setup_wake_stub((u32)wmt_smc_wake_stub);

	return 0;
}
module_init(wmt_secure_wait_wake_init);

static void __exit wmt_secure_wait_wake_exit(void)
{
	return ;
}
module_exit(wmt_secure_wait_wake_exit);

MODULE_AUTHOR("WonderMedia Technologies, Inc");
MODULE_DESCRIPTION("WMT Secure Wait & Wake Implementation");
MODULE_LICENSE("Dual BSD/GPL");

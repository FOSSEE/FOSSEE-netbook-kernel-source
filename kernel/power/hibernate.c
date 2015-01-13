/*
 * kernel/power/hibernate.c - Hibernation (a.k.a suspend-to-disk) support.
 *
 * Copyright (c) 2003 Patrick Mochel
 * Copyright (c) 2003 Open Source Development Lab
 * Copyright (c) 2004 Pavel Machek <pavel@ucw.cz>
 * Copyright (c) 2009 Rafael J. Wysocki, Novell Inc.
 *
 * This file is released under the GPLv2.
 */

#include <linux/export.h>
#include <linux/suspend.h>
#include <linux/syscalls.h>
#include <linux/reboot.h>
#include <linux/string.h>
#include <linux/device.h>
#include <linux/async.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/mount.h>
#include <linux/pm.h>
#include <linux/console.h>
#include <linux/cpu.h>
#include <linux/freezer.h>
#include <linux/gfp.h>
#include <linux/syscore_ops.h>
#include <linux/ctype.h>
#include <linux/genhd.h>
#include <scsi/scsi_scan.h>
#include <mach/hardware.h>
#include "power.h"
#include "../../drivers/char/wmt-pwm.h"

#define CONFIG_WMT_KILLER 1
//#undef CONFIG_WMT_KILLER
#define MAY_SWAP 0x2

static int nocompress;
static int noresume;
static int resume_wait;
static int resume_delay;
char resume_file[64] = CONFIG_PM_STD_PARTITION;
dev_t swsusp_resume_device;
sector_t swsusp_resume_block;
int in_suspend __nosavedata;
extern struct mutex wmt_lock;
extern int wmt_swap;
extern u32 __nosave_backup_phys;
extern u32 __nosave_begin_phys;
extern u32 __nosave_end_phys;
static int stress_resume_times;//record stress test times.
extern int wmt_getsyspara(char *varname, unsigned char *varval, int *varlen);

enum {
	HIBERNATION_INVALID,
	HIBERNATION_PLATFORM,
	HIBERNATION_SHUTDOWN,
	HIBERNATION_REBOOT,
	/* keep last */
	__HIBERNATION_AFTER_LAST
};
#define HIBERNATION_MAX (__HIBERNATION_AFTER_LAST-1)
#define HIBERNATION_FIRST (HIBERNATION_INVALID + 1)

static int hibernation_mode = HIBERNATION_SHUTDOWN;

bool freezer_test_done;

static const struct platform_hibernation_ops *hibernation_ops;
extern void wmt_resume_notify(void);
extern void lcd_enable_signal(int enable);
static int wmt_setswap(void)
{
        int varlen = 5;
        char std_env_val[20] = "0";
        unsigned int std;
        if (wmt_getsyspara("wmt.std.param", std_env_val, &varlen) == 0) {
                sscanf(std_env_val, "%X", &std);
                if (std & MAY_SWAP)
                        return 1;
                return 0;
        } else {
                printk(KERN_ALERT "##Warning: \"wmt.std.param\" not find\n");
                printk(KERN_ALERT "Close may_swap function");
        }
        return 0;
}

/**
 * hibernation_set_ops - Set the global hibernate operations.
 * @ops: Hibernation operations to use in subsequent hibernation transitions.
 */
void hibernation_set_ops(const struct platform_hibernation_ops *ops)
{
	if (ops && !(ops->begin && ops->end &&  ops->pre_snapshot
	    && ops->prepare && ops->finish && ops->enter && ops->pre_restore
	    && ops->restore_cleanup && ops->leave)) {
		WARN_ON(1);
		return;
	}
	lock_system_sleep();
	hibernation_ops = ops;
	if (ops)
		hibernation_mode = HIBERNATION_PLATFORM;
	else if (hibernation_mode == HIBERNATION_PLATFORM)
		hibernation_mode = HIBERNATION_SHUTDOWN;

	unlock_system_sleep();
}

static bool entering_platform_hibernation;

bool system_entering_hibernation(void)
{
	return entering_platform_hibernation;
}
EXPORT_SYMBOL(system_entering_hibernation);

#ifdef CONFIG_PM_DEBUG
static void hibernation_debug_sleep(void)
{
	printk(KERN_INFO "hibernation debug: Waiting for 5 seconds.\n");
	mdelay(5000);
}

static int hibernation_test(int level)
{
	if (pm_test_level == level) {
		hibernation_debug_sleep();
		return 1;
	}
	return 0;
}
#else /* !CONFIG_PM_DEBUG */
static int hibernation_test(int level) { return 0; }
#endif /* !CONFIG_PM_DEBUG */

/**
 * platform_begin - Call platform to start hibernation.
 * @platform_mode: Whether or not to use the platform driver.
 */
static int platform_begin(int platform_mode)
{
	return (platform_mode && hibernation_ops) ?
		hibernation_ops->begin() : 0;
}

/**
 * platform_end - Call platform to finish transition to the working state.
 * @platform_mode: Whether or not to use the platform driver.
 */
static void platform_end(int platform_mode)
{
	if (platform_mode && hibernation_ops)
		hibernation_ops->end();
}

/**
 * platform_pre_snapshot - Call platform to prepare the machine for hibernation.
 * @platform_mode: Whether or not to use the platform driver.
 *
 * Use the platform driver to prepare the system for creating a hibernate image,
 * if so configured, and return an error code if that fails.
 */

static int platform_pre_snapshot(int platform_mode)
{
	return (platform_mode && hibernation_ops) ?
		hibernation_ops->pre_snapshot() : 0;
}

/**
 * platform_leave - Call platform to prepare a transition to the working state.
 * @platform_mode: Whether or not to use the platform driver.
 *
 * Use the platform driver prepare to prepare the machine for switching to the
 * normal mode of operation.
 *
 * This routine is called on one CPU with interrupts disabled.
 */
static void platform_leave(int platform_mode)
{
	if (platform_mode && hibernation_ops)
		hibernation_ops->leave();
}

/**
 * platform_finish - Call platform to switch the system to the working state.
 * @platform_mode: Whether or not to use the platform driver.
 *
 * Use the platform driver to switch the machine to the normal mode of
 * operation.
 *
 * This routine must be called after platform_prepare().
 */
static void platform_finish(int platform_mode)
{
	if (platform_mode && hibernation_ops)
		hibernation_ops->finish();
}

/**
 * platform_pre_restore - Prepare for hibernate image restoration.
 * @platform_mode: Whether or not to use the platform driver.
 *
 * Use the platform driver to prepare the system for resume from a hibernation
 * image.
 *
 * If the restore fails after this function has been called,
 * platform_restore_cleanup() must be called.
 */
static int platform_pre_restore(int platform_mode)
{
	return (platform_mode && hibernation_ops) ?
		hibernation_ops->pre_restore() : 0;
}

/**
 * platform_restore_cleanup - Switch to the working state after failing restore.
 * @platform_mode: Whether or not to use the platform driver.
 *
 * Use the platform driver to switch the system to the normal mode of operation
 * after a failing restore.
 *
 * If platform_pre_restore() has been called before the failing restore, this
 * function must be called too, regardless of the result of
 * platform_pre_restore().
 */
static void platform_restore_cleanup(int platform_mode)
{
	if (platform_mode && hibernation_ops)
		hibernation_ops->restore_cleanup();
}

/**
 * platform_recover - Recover from a failure to suspend devices.
 * @platform_mode: Whether or not to use the platform driver.
 */
static void platform_recover(int platform_mode)
{
	if (platform_mode && hibernation_ops && hibernation_ops->recover)
		hibernation_ops->recover();
}

/**
 * swsusp_show_speed - Print time elapsed between two events during hibernation.
 * @start: Starting event.
 * @stop: Final event.
 * @nr_pages: Number of memory pages processed between @start and @stop.
 * @msg: Additional diagnostic message to print.
 */
void swsusp_show_speed(struct timeval *start, struct timeval *stop,
			unsigned nr_pages, char *msg)
{
	s64 elapsed_centisecs64;
	int centisecs;
	int k;
	int kps;

	elapsed_centisecs64 = timeval_to_ns(stop) - timeval_to_ns(start);
	do_div(elapsed_centisecs64, NSEC_PER_SEC / 100);
	centisecs = elapsed_centisecs64;
	if (centisecs == 0)
		centisecs = 1;	/* avoid div-by-zero */
	k = nr_pages * (PAGE_SIZE / 1024);
	kps = (k * 100) / centisecs;
	printk(KERN_CRIT "PM: %s %d kbytes in %d.%02d seconds (%d.%02d MB/s)\n",
			msg, k,
			centisecs / 100, centisecs % 100,
			kps / 1000, (kps % 1000) / 10);
}
#ifdef CONFIG_WMT_KILLER
#include <linux/oom.h>

#define OOM_SCORE_ADJ_MAX	1000

static unsigned long  wmt_deathpending_timeout;

/*Keywords of Excluded Process Name*/
static const char keywords[][64] = {
    ".launcher",
	".mkpro",
    ""
};
/*
Name: wmt_in_list()
Desc: list contains some keywords. check if a specific name contains keyword that on the list.
Param:
        keyword_list: the list contains keywords
        name: the name to be checked
Return:
        if the specific name contains keywords in the list, 
        return true, otherwise return false.
*/
static bool wmt_in_list(const char keyword_list[][64], const char *name)
{
    int i = 0;
    char *sub_str = NULL;
    //printk("checking name: %s\n", name);
    while (strcmp(keyword_list[i], "") != 0) {
        //printk("matching keyword:%s\n", keyword_list[i]);
        sub_str = strstr(name, keyword_list[i]);

        if (sub_str) {
            //printk("matched:%s\n", keyword_list[i]);
            return true;
        }
        i++;
    }
    return false;
}

static void wmt_killer(void)
{
	struct task_struct *tsk;
	struct task_struct *selected = NULL;
	int rem = 0;
	int tasksize;
	int min_score_adj = 1;//OOM_SCORE_ADJ_MAX>>6;
	int selected_tasksize = 0;
	int selected_oom_score_adj;

	rem = global_page_state(NR_ACTIVE_ANON) +
		global_page_state(NR_ACTIVE_FILE) +
		global_page_state(NR_INACTIVE_ANON) +
		global_page_state(NR_INACTIVE_FILE);
	printk("wmt_killer: min_score_adj %d, rem %d\n",
		min_score_adj,rem);

	selected_oom_score_adj = min_score_adj;

	rcu_read_lock();
	for_each_process(tsk) {
		struct task_struct *p;
		int oom_score_adj;

		if (tsk->flags & PF_KTHREAD)
			continue;

		p = find_lock_task_mm(tsk);
		if (!p)
			continue;

		if (test_tsk_thread_flag(p, TIF_MEMDIE) &&
			time_before_eq(jiffies, wmt_deathpending_timeout)) {
				task_unlock(p);
				rcu_read_unlock();
				return;
		}
		oom_score_adj = p->signal->oom_score_adj;
		if (oom_score_adj < min_score_adj) {
			task_unlock(p);
			continue;
		}
		tasksize = get_mm_rss(p->mm);
		task_unlock(p);
		if (tasksize <= 0)
			continue;
#if 0
		if (selected) {
			if (oom_score_adj < selected_oom_score_adj)
				continue;
			if (oom_score_adj == selected_oom_score_adj &&
				tasksize <= selected_tasksize)
				continue;
		}
#endif
		selected = p;
		selected_tasksize = tasksize;
		selected_oom_score_adj = oom_score_adj;
		printk("wmt_killer: select %d (%s), adj %d, size %d, to kill\n",
			p->pid, p->comm, oom_score_adj, tasksize);

#if 1	/*check process name first.*/
		if(wmt_in_list(keywords, p->comm)){
			printk("[wmt_killer]: Exclude Process :%s\n", p->comm);
			selected = NULL;
		}
#endif
		if (selected) {
			printk("wmt_killer: send sigkill to %d (%s), adj %d, size %d\n",
				selected->pid, selected->comm,
				selected_oom_score_adj, selected_tasksize);
			wmt_deathpending_timeout = jiffies + HZ;
			send_sig(SIGKILL, selected, 0);
			set_tsk_thread_flag(selected, TIF_MEMDIE);
			rem -= selected_tasksize;
		}
	}
	rcu_read_unlock();
	printk("wmt_killer: rem = %d\n", rem);
}

#else
static void wmt_killer(void){
}

#endif

/**
 * create_image - Create a hibernation image.
 * @platform_mode: Whether or not to use the platform driver.
 *
 * Execute device drivers' "late" and "noirq" freeze callbacks, create a
 * hibernation image and run the drivers' "noirq" and "early" thaw callbacks.
 *
 * Control reappears in this routine after the subsequent restore.
 */
static int create_image(int platform_mode)
{
	int error;
	unsigned int pmc_temp;

	error = dpm_suspend_end(PMSG_FREEZE);
	if (error) {
		printk(KERN_ERR "PM: Some devices failed to power down, "
			"aborting hibernation\n");
		return error;
	}

	error = platform_pre_snapshot(platform_mode);
	if (error || hibernation_test(TEST_PLATFORM))
		goto Platform_finish;

	error = disable_nonboot_cpus();
	if (error || hibernation_test(TEST_CPUS))
		goto Enable_cpus;

	local_irq_disable();

	error = syscore_suspend();
	if (error) {
		printk(KERN_ERR "PM: Some system devices failed to power down, "
			"aborting hibernation\n");
		goto Enable_irqs;
	}

	pmc_temp = PMWS_VAL;
	pmc_temp &= (WK_TRG_EN_VAL | 0x4000);
	PMWS_VAL = PMWS_VAL;
	if (hibernation_test(TEST_CORE) || pm_wakeup_pending())
		goto Power_up;
	in_suspend = 1;
	save_processor_state();
	error = swsusp_arch_suspend();
	if (error)
		printk(KERN_ERR "PM: Error %d creating hibernation image\n",
			error);
	/* Restore control flow magically appears here */
	restore_processor_state();
#if 1
	/*check if nosave addresses are correct.*/
	if(!in_suspend){
		printk(KERN_CRIT "PM: Image Restored, Resuming...\n");
		printk("__nosave_backup_phys=0x%x\n",__nosave_backup_phys);
		printk("__nosave_begin_phys=0x%x\n",__nosave_begin_phys);
		printk("__nosave_end_phys=0x%x\n",__nosave_end_phys);
	}
#endif
	if (!in_suspend) {
		events_check_enabled = false;
		//platform_leave(platform_mode);
	}
	/* 
	  do platform_leave() for both in_suspend = 0 or 1.
	  When in_suspend=1, 
	  If we need to abort the procedure,  
	  platform_leave() will not be called unless we do it here.
	  Without calling this function,  some devices are not be resumed
	  and can not functional properly.
	*/
	platform_leave(platform_mode);

 Power_up:
	syscore_resume();

 Enable_irqs:
	local_irq_enable();

 Enable_cpus:
	enable_nonboot_cpus();

 Platform_finish:
	platform_finish(platform_mode);

	dpm_resume_start(in_suspend ?
		(error ? PMSG_RECOVER : PMSG_THAW) : PMSG_RESTORE);

	return error;
}

/**
 * hibernation_snapshot - Quiesce devices and create a hibernation image.
 * @platform_mode: If set, use platform driver to prepare for the transition.
 *
 * This routine must be called with pm_mutex held.
 */
int hibernation_snapshot(int platform_mode)
{
	pm_message_t msg;
	int error;
	
	error = platform_begin(platform_mode);
	if (error)
		goto Close;

	/* Preallocate image memory before shutting down devices. */
	error = hibernate_preallocate_memory();
	if (error) {
		mutex_lock(&wmt_lock);
		mutex_unlock(&wmt_lock);
		goto Close;
	}
	mutex_lock(&wmt_lock);
	error = freeze_kernel_threads();
	if (error)
		goto Cleanup;

	if (hibernation_test(TEST_FREEZER)) {

		/*
		 * Indicate to the caller that we are returning due to a
		 * successful freezer test.
		 */
		freezer_test_done = true;
		goto Thaw;
	}

	error = dpm_prepare(PMSG_FREEZE);
	if (error) {
		dpm_complete(PMSG_RECOVER);
		goto Thaw;
	}

	suspend_console();
	pm_restrict_gfp_mask();

	error = dpm_suspend(PMSG_FREEZE);

	if (error || hibernation_test(TEST_DEVICES))
		platform_recover(platform_mode);
	else
		error = create_image(platform_mode);
	mutex_unlock(&wmt_lock);
	/*
	 * In the case that we call create_image() above, the control
	 * returns here (1) after the image has been created or the
	 * image creation has failed and (2) after a successful restore.
	 */

	/* We may need to release the preallocated image pages here. */
	if (error || !in_suspend)
		swsusp_free();

	msg = in_suspend ? (error ? PMSG_RECOVER : PMSG_THAW) : PMSG_RESTORE;
	dpm_resume(msg);

	if (error || !in_suspend)
		pm_restore_gfp_mask();

	resume_console();
	dpm_complete(msg);

 Close:
	platform_end(platform_mode);
	return error;

 Thaw:
	thaw_kernel_threads();
 Cleanup:
	swsusp_free();
	goto Close;
}

/**
 * resume_target_kernel - Restore system state from a hibernation image.
 * @platform_mode: Whether or not to use the platform driver.
 *
 * Execute device drivers' "noirq" and "late" freeze callbacks, restore the
 * contents of highmem that have not been restored yet from the image and run
 * the low-level code that will restore the remaining contents of memory and
 * switch to the just restored target kernel.
 */
static int resume_target_kernel(bool platform_mode)
{
	int error;

	error = dpm_suspend_end(PMSG_QUIESCE);
	if (error) {
		printk(KERN_ERR "PM: Some devices failed to power down, "
			"aborting resume\n");
		return error;
	}

	error = platform_pre_restore(platform_mode);
	if (error)
		goto Cleanup;

	error = disable_nonboot_cpus();
	if (error)
		goto Enable_cpus;

	local_irq_disable();

	error = syscore_suspend();
	if (error)
		goto Enable_irqs;

	save_processor_state();
	error = restore_highmem();
	if (!error) {
		error = swsusp_arch_resume();
		/*
		 * The code below is only ever reached in case of a failure.
		 * Otherwise, execution continues at the place where
		 * swsusp_arch_suspend() was called.
		 */
		BUG_ON(!error);
		/*
		 * This call to restore_highmem() reverts the changes made by
		 * the previous one.
		 */
		restore_highmem();
	}
	/*
	 * The only reason why swsusp_arch_resume() can fail is memory being
	 * very tight, so we have to free it as soon as we can to avoid
	 * subsequent failures.
	 */
	swsusp_free();
	restore_processor_state();
	touch_softlockup_watchdog();

	syscore_resume();

 Enable_irqs:
	local_irq_enable();

 Enable_cpus:
	enable_nonboot_cpus();

 Cleanup:
	platform_restore_cleanup(platform_mode);

	dpm_resume_start(PMSG_RECOVER);

	return error;
}

/**
 * hibernation_restore - Quiesce devices and restore from a hibernation image.
 * @platform_mode: If set, use platform driver to prepare for the transition.
 *
 * This routine must be called with pm_mutex held.  If it is successful, control
 * reappears in the restored target kernel in hibernation_snapshot().
 */
int hibernation_restore(int platform_mode)
{
	int error;

	pm_prepare_console();
	suspend_console();
	pm_restrict_gfp_mask();
	error = dpm_suspend_start(PMSG_QUIESCE);
	if (!error) {
		error = resume_target_kernel(platform_mode);
		dpm_resume_end(PMSG_RECOVER);
	}
	pm_restore_gfp_mask();
	resume_console();
	pm_restore_console();
	return error;
}

/**
 * hibernation_platform_enter - Power off the system using the platform driver.
 */
int hibernation_platform_enter(void)
{
	int error;

	if (!hibernation_ops)
		return -ENOSYS;

	/*
	 * We have cancelled the power transition by running
	 * hibernation_ops->finish() before saving the image, so we should let
	 * the firmware know that we're going to enter the sleep state after all
	 */
	error = hibernation_ops->begin();
	if (error)
		goto Close;

	entering_platform_hibernation = true;
	suspend_console();
	error = dpm_suspend_start(PMSG_HIBERNATE);
	if (error) {
		if (hibernation_ops->recover)
			hibernation_ops->recover();
		goto Resume_devices;
	}

	error = dpm_suspend_end(PMSG_HIBERNATE);
	if (error)
		goto Resume_devices;

	error = hibernation_ops->prepare();
	if (error)
		goto Platform_finish;

	error = disable_nonboot_cpus();
	if (error)
		goto Platform_finish;

	local_irq_disable();
	syscore_suspend();
	if (pm_wakeup_pending()) {
		error = -EAGAIN;
		goto Power_up;
	}

	hibernation_ops->enter();
	/* We should never get here */
	while (1);

 Power_up:
	syscore_resume();
	local_irq_enable();
	enable_nonboot_cpus();

 Platform_finish:
	hibernation_ops->finish();

	dpm_resume_start(PMSG_RESTORE);

 Resume_devices:
	entering_platform_hibernation = false;
	dpm_resume_end(PMSG_RESTORE);
	resume_console();

 Close:
	hibernation_ops->end();

	return error;
}

/**
 * power_down - Shut the machine down for hibernation.
 *
 * Use the platform driver, if configured, to put the system into the sleep
 * state corresponding to hibernation, or try to power it off or reboot,
 * depending on the value of hibernation_mode.
 */
static void power_down(void)
{
	switch (hibernation_mode) {
	case HIBERNATION_REBOOT:
		kernel_restart(NULL);
		break;
	case HIBERNATION_PLATFORM:
		hibernation_platform_enter();
	case HIBERNATION_SHUTDOWN:
		kernel_power_off();
		break;
	}
	kernel_halt();
	/*
	 * Valid image is on the disk, if we continue we risk serious data
	 * corruption after resume.
	 */
	printk(KERN_CRIT "PM: Please power down manually\n");
	while(1);
}

/**
 * hibernate - Carry out system hibernation, including saving the image.
 */
int hibernate(void)
{
	int error;
	char std_env_val[20] = "0";
	int varlen = 5;
	unsigned int std;

	lock_system_sleep();
	
	error = swsusp_swap_check();
	if (error) {
		if (error != -ENOSPC)
			printk(KERN_ERR "PM: Cannot find swap device, try "
			"swapon -a.\n");
		goto Unlock;
	}

	/* The snapshot device should not be opened while we're running */
	if (!atomic_add_unless(&snapshot_device_available, -1, 0)) {
		error = -EBUSY;
		goto Unlock;
	}

	pm_prepare_console();
	error = pm_notifier_call_chain(PM_HIBERNATION_PREPARE);
	if (error)
		goto Exit;
	/*Try to open swap function*/
	wmt_swap = wmt_setswap();

	/* Allocate memory management structures */
	error = create_basic_memory_bitmaps();
	if (error)
		goto Exit;

	printk(KERN_CRIT "PM: Syncing filesystems ... ");
	sys_sync();
	printk("done.\n");

	wmt_killer();//kill process to save memory before freeze_process.

	error = freeze_processes();
	if (error)
		goto Free_bitmaps;

	error = hibernation_snapshot(hibernation_mode == HIBERNATION_PLATFORM);
	if (error || freezer_test_done) {
		pm_notifier_call_chain(PM_HIBERNATION_FINISH);
		goto Thaw;
	}
	if (in_suspend) {
		unsigned int flags = 0;

		if (hibernation_mode == HIBERNATION_PLATFORM)
			flags |= SF_PLATFORM_MODE;
		if (nocompress)
			flags |= SF_NOCOMPRESS_MODE;
		else
		        flags |= SF_CRC32_MODE;

		printk(KERN_CRIT"PM: writing image.\n");
		error = swsusp_write(flags);
		pm_notifier_call_chain(PM_HIBERNATION_FINISH);
		swsusp_free();
		if (!error)
			power_down();
		in_suspend = 0;
		pm_restore_gfp_mask();
	} else {
		mutex_unlock(&wmt_lock);
		printk(KERN_CRIT "PM: Image restored successfully.\n");
		printk(KERN_CRIT "PM: STD Stress Test: %d times\n",++stress_resume_times);
		printk(KERN_CRIT "Resume to Android\n");
		if (wmt_getsyspara("wmt.std.param",std_env_val,&varlen) == 0) {
			sscanf(std_env_val,"%X",&std);
			if (std & 0x1) 
				wmt_resume_notify();
		}
	}

 Thaw:
	thaw_processes();

	/* Don't bother checking whether freezer_test_done is true */
	freezer_test_done = false;

 Free_bitmaps:
	free_basic_memory_bitmaps();
 Exit:
	pm_notifier_call_chain(PM_POST_HIBERNATION);
	pm_restore_console();
	atomic_inc(&snapshot_device_available);
 Unlock:
	unlock_system_sleep();

	wmt_swap = 1;
	return error;
}


/**
 * software_resume - Resume from a saved hibernation image.
 *
 * This routine is called as a late initcall, when all devices have been
 * discovered and initialized already.
 *
 * The image reading code is called to see if there is a hibernation image
 * available for reading.  If that is the case, devices are quiesced and the
 * contents of memory is restored from the saved image.
 *
 * If this is successful, control reappears in the restored target kernel in
 * hibernation_snaphot() which returns to hibernate().  Otherwise, the routine
 * attempts to recover gracefully and make the kernel return to the normal mode
 * of operation.
 */
static int software_resume(void)
{
	int error;
	unsigned int flags;
	int retry_wait;

	/*
	  Set resume_wait to 1 is essential when using mmc device as std partition.
	  Note that kernel will wait for it until it is present.
	*/
	resume_wait = 1;
	/*
	 * If the user said "noresume".. bail out early.
	 */
	if (noresume)
		return 0;

	/*
	 * name_to_dev_t() below takes a sysfs buffer mutex when sysfs
	 * is configured into the kernel. Since the regular hibernate
	 * trigger path is via sysfs which takes a buffer mutex before
	 * calling hibernate functions (which take pm_mutex) this can
	 * cause lockdep to complain about a possible ABBA deadlock
	 * which cannot happen since we're in the boot code here and
	 * sysfs can't be invoked yet. Therefore, we use a subclass
	 * here to avoid lockdep complaining.
	 */
	mutex_lock_nested(&pm_mutex, SINGLE_DEPTH_NESTING);

	if (swsusp_resume_device)
		goto Check_image;

	if (!strlen(resume_file)) {
		error = -ENOENT;
		goto Unlock;
	}

	printk(KERN_CRIT"PM: Checking hibernation image partition %s\n", resume_file);

	if (resume_delay) {
		printk(KERN_INFO "Waiting %dsec before reading resume device...\n",
			resume_delay);
		ssleep(resume_delay);
	}

	/* Check if the device is there */
	swsusp_resume_device = name_to_dev_t(resume_file);

	/*
	 * name_to_dev_t is ineffective to verify parition if resume_file is in
	 * integer format. (e.g. major:minor)
	 */
	if (isdigit(resume_file[0]) && resume_wait) {
		int partno;
		while (!get_gendisk(swsusp_resume_device, &partno))
			msleep(10);
	}

	if (!swsusp_resume_device) {
		/*
		 * Some device discovery might still be in progress; we need
		 * to wait for this to finish.
		 */
		printk(KERN_CRIT"\n resume device not present.\n");
		wait_for_device_probe();

		if (resume_wait) {
#if 0
			while ((swsusp_resume_device = name_to_dev_t(resume_file)) == 0)
				msleep(10);
#else
			for(retry_wait=100; retry_wait>0; retry_wait--){
				if((swsusp_resume_device = name_to_dev_t(resume_file)) == 0){
					msleep(10);
				}
			}	
#endif
			async_synchronize_full();
		}

		/*
		 * We can't depend on SCSI devices being available after loading
		 * one of their modules until scsi_complete_async_scans() is
		 * called and the resume device usually is a SCSI one.
		 */
		scsi_complete_async_scans();

		swsusp_resume_device = name_to_dev_t(resume_file);
		if (!swsusp_resume_device) {
			error = -ENODEV;
			goto Unlock;
		}
	}

 Check_image:
	printk(KERN_CRIT"PM: Hibernation image partition %d:%d present\n",
		MAJOR(swsusp_resume_device), MINOR(swsusp_resume_device));

	printk(KERN_CRIT"PM: Looking for hibernation image.\n");
	error = swsusp_check();
	if (error)
		goto Unlock;

	/* The snapshot device should not be opened while we're running */
	if (!atomic_add_unless(&snapshot_device_available, -1, 0)) {
		error = -EBUSY;
		swsusp_close(FMODE_READ);
		goto Unlock;
	}

	pm_prepare_console();
	error = pm_notifier_call_chain(PM_RESTORE_PREPARE);
	if (error)
		goto close_finish;

	error = create_basic_memory_bitmaps();
	if (error)
		goto close_finish;

	printk(KERN_CRIT"PM: Preparing processes for restore.\n");
	error = freeze_processes();
	if (error) {
		swsusp_close(FMODE_READ);
		goto Done;
	}

	printk(KERN_CRIT"PM: Loading hibernation image.\n");

	error = swsusp_read(&flags);
	swsusp_close(FMODE_READ);
	if (!error)
		hibernation_restore(flags & SF_PLATFORM_MODE);

	printk(KERN_ERR "PM: Failed to load hibernation image, recovering.\n");
	swsusp_free();
	thaw_processes();
 Done:
	free_basic_memory_bitmaps();
 Finish:
	pm_notifier_call_chain(PM_POST_RESTORE);
	pm_restore_console();
	atomic_inc(&snapshot_device_available);
	/* For success case, the suspend path will release the lock */
 Unlock:
	mutex_unlock(&pm_mutex);
	printk(KERN_CRIT"PM: Hibernation image not present or could not be loaded.\n");
	return error;
close_finish:
	swsusp_close(FMODE_READ);
	goto Finish;
}

late_initcall(software_resume);


static const char * const hibernation_modes[] = {
	[HIBERNATION_PLATFORM]	= "platform",
	[HIBERNATION_SHUTDOWN]	= "shutdown",
	[HIBERNATION_REBOOT]	= "reboot",
};

/*
 * /sys/power/disk - Control hibernation mode.
 *
 * Hibernation can be handled in several ways.  There are a few different ways
 * to put the system into the sleep state: using the platform driver (e.g. ACPI
 * or other hibernation_ops), powering it off or rebooting it (for testing
 * mostly).
 *
 * The sysfs file /sys/power/disk provides an interface for selecting the
 * hibernation mode to use.  Reading from this file causes the available modes
 * to be printed.  There are 3 modes that can be supported:
 *
 *	'platform'
 *	'shutdown'
 *	'reboot'
 *
 * If a platform hibernation driver is in use, 'platform' will be supported
 * and will be used by default.  Otherwise, 'shutdown' will be used by default.
 * The selected option (i.e. the one corresponding to the current value of
 * hibernation_mode) is enclosed by a square bracket.
 *
 * To select a given hibernation mode it is necessary to write the mode's
 * string representation (as returned by reading from /sys/power/disk) back
 * into /sys/power/disk.
 */

static ssize_t disk_show(struct kobject *kobj, struct kobj_attribute *attr,
			 char *buf)
{
	int i;
	char *start = buf;

	for (i = HIBERNATION_FIRST; i <= HIBERNATION_MAX; i++) {
		if (!hibernation_modes[i])
			continue;
		switch (i) {
		case HIBERNATION_SHUTDOWN:
		case HIBERNATION_REBOOT:
			break;
		case HIBERNATION_PLATFORM:
			if (hibernation_ops)
				break;
			/* not a valid mode, continue with loop */
			continue;
		}
		if (i == hibernation_mode)
			buf += sprintf(buf, "[%s] ", hibernation_modes[i]);
		else
			buf += sprintf(buf, "%s ", hibernation_modes[i]);
	}
	buf += sprintf(buf, "\n");
	return buf-start;
}

static ssize_t disk_store(struct kobject *kobj, struct kobj_attribute *attr,
			  const char *buf, size_t n)
{
	int error = 0;
	int i;
	int len;
	char *p;
	int mode = HIBERNATION_INVALID;

	p = memchr(buf, '\n', n);
	len = p ? p - buf : n;

	lock_system_sleep();
	for (i = HIBERNATION_FIRST; i <= HIBERNATION_MAX; i++) {
		if (len == strlen(hibernation_modes[i])
		    && !strncmp(buf, hibernation_modes[i], len)) {
			mode = i;
			break;
		}
	}
	if (mode != HIBERNATION_INVALID) {
		switch (mode) {
		case HIBERNATION_SHUTDOWN:
		case HIBERNATION_REBOOT:
			hibernation_mode = mode;
			break;
		case HIBERNATION_PLATFORM:
			if (hibernation_ops)
				hibernation_mode = mode;
			else
				error = -EINVAL;
		}
	} else
		error = -EINVAL;

	if (!error)
		printk(KERN_CRIT"PM: Hibernation mode set to '%s'\n",
			 hibernation_modes[mode]);
	unlock_system_sleep();
	return error ? error : n;
}

power_attr(disk);

static ssize_t resume_show(struct kobject *kobj, struct kobj_attribute *attr,
			   char *buf)
{
	return sprintf(buf,"%d:%d\n", MAJOR(swsusp_resume_device),
		       MINOR(swsusp_resume_device));
}

static ssize_t resume_store(struct kobject *kobj, struct kobj_attribute *attr,
			    const char *buf, size_t n)
{
	unsigned int maj, min;
	dev_t res;
	int ret = -EINVAL;

	if (sscanf(buf, "%u:%u", &maj, &min) != 2)
		goto out;

	res = MKDEV(maj,min);
	if (maj != MAJOR(res) || min != MINOR(res))
		goto out;

	lock_system_sleep();
	swsusp_resume_device = res;
	unlock_system_sleep();
	printk(KERN_INFO "PM: Starting manual resume from disk\n");
	noresume = 0;
	software_resume();
	ret = n;
 out:
	return ret;
}

power_attr(resume);

static ssize_t image_size_show(struct kobject *kobj, struct kobj_attribute *attr,
			       char *buf)
{
	return sprintf(buf, "%lu\n", image_size);
}

static ssize_t image_size_store(struct kobject *kobj, struct kobj_attribute *attr,
				const char *buf, size_t n)
{
	unsigned long size;

	if (sscanf(buf, "%lu", &size) == 1) {
		image_size = size;
		return n;
	}

	return -EINVAL;
}

power_attr(image_size);

static ssize_t reserved_size_show(struct kobject *kobj,
				  struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%lu\n", reserved_size);
}

static ssize_t reserved_size_store(struct kobject *kobj,
				   struct kobj_attribute *attr,
				   const char *buf, size_t n)
{
	unsigned long size;

	if (sscanf(buf, "%lu", &size) == 1) {
		reserved_size = size;
		return n;
	}

	return -EINVAL;
}

power_attr(reserved_size);

static struct attribute * g[] = {
	&disk_attr.attr,
	&resume_attr.attr,
	&image_size_attr.attr,
	&reserved_size_attr.attr,
	NULL,
};


static struct attribute_group attr_group = {
	.attrs = g,
};


static int __init pm_disk_init(void)
{
	return sysfs_create_group(power_kobj, &attr_group);
}

core_initcall(pm_disk_init);


static int __init resume_setup(char *str)
{
	if (noresume)
		return 1;

	strncpy( resume_file, str, 255 );
	return 1;
}

static int __init resume_offset_setup(char *str)
{
	unsigned long long offset;

	if (noresume)
		return 1;

	if (sscanf(str, "%llu", &offset) == 1)
		swsusp_resume_block = offset;

	return 1;
}

static int __init hibernate_setup(char *str)
{
	if (!strncmp(str, "noresume", 8))
		noresume = 1;
	else if (!strncmp(str, "nocompress", 10))
		nocompress = 1;
	return 1;
}

static int __init noresume_setup(char *str)
{
	noresume = 1;
	return 1;
}

static int __init resumewait_setup(char *str)
{
	resume_wait = 1;
	return 1;
}

static int __init resumedelay_setup(char *str)
{
	resume_delay = simple_strtoul(str, NULL, 0);
	return 1;
}

__setup("noresume", noresume_setup);
__setup("resume_offset=", resume_offset_setup);
__setup("resume=", resume_setup);
__setup("hibernate=", hibernate_setup);
__setup("resumewait", resumewait_setup);
__setup("resumedelay=", resumedelay_setup);

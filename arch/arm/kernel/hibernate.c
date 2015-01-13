/*
 * Hibernation support specific for ARM
 *
 * Derived from work on ARM hibernation support by:
 *
 * Ubuntu project, hibernation support for mach-dove
 * Copyright (C) 2010 Nokia Corporation (Hiroshi Doyu)
 * Copyright (C) 2010 Texas Instruments, Inc. (Teerth Reddy et al.)
 *	https://lkml.org/lkml/2010/6/18/4
 *	https://lists.linux-foundation.org/pipermail/linux-pm/2010-June/027422.html
 *	https://patchwork.kernel.org/patch/96442/
 *
 * Copyright (C) 2006 Rafael J. Wysocki <rjw@sisk.pl>
 *
 * License terms: GNU General Public License (GPL) version 2
 */
/*
	from patch 
	ARM hibernation / suspend-to-disk
	https://github.com/russdill/linux/commit/498079e0d06d2c0bd575ab4114d16b09e1ff3111
	add by roger.
*/
#include <linux/mm.h>
#include <linux/suspend.h>
#include <asm/tlbflush.h>
#include <asm/cacheflush.h>
#include <asm/system_misc.h>
#include <linux/slab.h>
#include <linux/crc32.h>
#include <asm/idmap.h>
#include <asm/suspend.h>

#define _PRINT_PAGE_CRC_ 1//also need to define in kernel/power/power.h
#undef _PRINT_PAGE_CRC_

extern const void __nosave_begin, __nosave_end;
extern void cpu_resume(void);
extern void cpu_resume_restore_nosave(void);

u32 __nosave_backup_phys;	//__nosave data backup here.
u32 __nosave_begin_phys;	//__nosave data orginal location begin from here.
u32 __nosave_end_phys;		//__nosave data orginal location end from here.

void swsusp_arch_add_info(char *archdata, size_t size)
{
	*(u32 *) archdata = virt_to_phys(cpu_resume);
	printk("\n phyaddr of cpu_resume : 0x%x \n", virt_to_phys(cpu_resume));
	*(((u32 *)archdata) + 1) = __nosave_backup_phys;
	*(((u32 *)archdata) + 2) = __nosave_begin_phys;
	*(((u32 *)archdata) + 3) = __nosave_end_phys;
/*
	Save and check idmap_pgd.
	When using Kernel restoring method, the "restoring kernel" should has the same 
	idmap_pgd as the "restored kernel". 
	If not, the mmu table may be corrupted during the restoration, 
	which may cause data abort (MMU translation fail).
*/
	*(((u32 *)archdata) + 4) = (u32)idmap_pgd;
}

int pfn_is_nosave(unsigned long pfn)
{
	unsigned long nosave_begin_pfn =
			__pa_symbol(&__nosave_begin) >> PAGE_SHIFT;
	unsigned long nosave_end_pfn =
			PAGE_ALIGN(__pa_symbol(&__nosave_end)) >> PAGE_SHIFT;

	return (pfn >= nosave_begin_pfn) && (pfn < nosave_end_pfn);
}

void notrace save_processor_state(void)
{
	WARN_ON(num_online_cpus() != 1);
	flush_thread();
	local_fiq_disable();
}

void notrace restore_processor_state(void)
{
	local_fiq_enable();
}

/*
 * Snapshot kernel memory and reset the system.
 * After resume, the hibernation snapshot is written out.
 */
static int notrace __swsusp_arch_save_image(unsigned long unused)
{
	extern int swsusp_save(void);
	int ret;

	ret = swsusp_save();
	if (ret == 0)
		soft_restart(virt_to_phys(cpu_resume));
	return ret;
}

/*
 * Save the current CPU state before suspend / poweroff.
 */
int notrace swsusp_arch_suspend(void)
{
	return cpu_suspend(0, __swsusp_arch_save_image);
}

/*
 * The framework loads the hibernation image into a linked list anchored
 * at restore_pblist, for swsusp_arch_resume() to copy back to the proper
 * destinations.
 *
 * To make this work if resume is triggered from initramfs, the
 * pagetables need to be switched to allow writes to kernel mem.
 */
static void notrace __swsusp_arch_restore_image(void *unused)
{
	extern struct pbe *restore_pblist;
	struct pbe *pbe;
#ifdef _PRINT_PAGE_CRC_// add by roger.
	unsigned long crc_le;
	unsigned char *virt_addr;
	unsigned long pfn;
#endif

	
	cpu_switch_mm(idmap_pgd, &init_mm);
	for (pbe = restore_pblist; pbe; pbe = pbe->next){
		copy_page(pbe->orig_address, pbe->address);
		#ifdef _PRINT_PAGE_CRC_
			virt_addr = pbe->orig_address;
			pfn  = page_to_pfn(virt_to_page(virt_addr));
			crc_le = crc32_le(0, virt_addr, PAGE_SIZE);
			printk("pfn:%lu:phy_addr:0x%x:crc:%lu:virt_addr:0x%x\n",
						pfn, pfn*PAGE_SIZE, crc_le, virt_addr);
		#endif		
	}
	soft_restart_noirq(virt_to_phys(cpu_resume));
}

static u8 __swsusp_resume_stk[PAGE_SIZE/2] __nosavedata;

/*
 * Resume from the hibernation image.
 * Due to the kernel heap / data restore, stack contents change underneath
 * and that would make function calls impossible; switch to a temporary
 * stack within the nosave region to avoid that problem.
 */
int __naked swsusp_arch_resume(void)
{
	extern void call_with_stack(void (*fn)(void *), void *arg, void *sp);

	cpu_init();	/* get a clean PSR */
	call_with_stack(__swsusp_arch_restore_image, 0,
		__swsusp_resume_stk + sizeof(__swsusp_resume_stk));

	return 0;
}

static int __init swsusp_arch_init(void)
{
	char *backup;
	size_t len;
	len = &__nosave_end - &__nosave_begin;
	backup = kmalloc(len, GFP_KERNEL);
	if (backup) {
		printk("%s: Backed up %d byte nosave region\n", __func__, len);
		memcpy(backup, &__nosave_begin, len);
	}

	__nosave_backup_phys = virt_to_phys(backup);
	__nosave_begin_phys = virt_to_phys(&__nosave_begin);
	__nosave_end_phys = virt_to_phys(&__nosave_end);
	printk("__nosave_backup_phys=0x%x\n",__nosave_backup_phys);
	printk("__nosave_begin_phys=0x%x\n",__nosave_begin_phys);
	printk("__nosave_end_phys=0x%x\n",__nosave_end_phys);
	return 0;
}
late_initcall(swsusp_arch_init);

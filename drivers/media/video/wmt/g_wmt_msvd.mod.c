#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
 .name = KBUILD_MODNAME,
 .init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
 .exit = cleanup_module,
#endif
 .arch = MODULE_ARCH_INIT,
};

MODULE_INFO(intree, "Y");

static const struct modversion_info ____versions[]
__used
__attribute__((section("__versions"))) = {
	{ 0x57a42d88, "module_layout" },
	{ 0x8d028e48, "wmt_codec_timer_start" },
	{ 0x8888fdc7, "cdev_del" },
	{ 0x1d122fd8, "kmalloc_caches" },
	{ 0x193eaea8, "cdev_init" },
	{ 0x17a142df, "__copy_from_user" },
	{ 0x311b7963, "_raw_spin_unlock" },
	{ 0x15692c87, "param_ops_int" },
	{ 0x98082893, "__copy_to_user" },
	{ 0xd8e484f0, "register_chrdev_region" },
	{ 0xc6c50923, "dma_release_declared_memory" },
	{ 0xc8b57c27, "autoremove_wake_function" },
	{ 0xdcefeadd, "device_destroy" },
	{ 0x353e3fa5, "__get_user_4" },
	{ 0xe2f241de, "wmt_codec_timer_stop" },
	{ 0xaab4b6f0, "mutex_unlock" },
	{ 0x7485e15e, "unregister_chrdev_region" },
	{ 0xf1a02a62, "wmt_get_codec_clock_count" },
	{ 0xd63e03ec, "mb_do_free" },
	{ 0xcc3bef4d, "wmt_codec_timer_init" },
	{ 0x7d11c268, "jiffies" },
	{ 0x4467122a, "__init_waitqueue_head" },
	{ 0x2e89d1f5, "dma_declare_coherent_memory" },
	{ 0xfa2a45e, "__memzero" },
	{ 0x4873c22f, "__mutex_init" },
	{ 0x27e1a049, "printk" },
	{ 0x89237345, "mb_do_phys_to_virt" },
	{ 0x65f4a251, "wmt_reset_codec_clock_count" },
	{ 0xb8ea94a9, "fasync_helper" },
	{ 0x95687190, "dma_free_coherent" },
	{ 0xadc6f1fa, "mutex_lock" },
	{ 0xe66c5f60, "platform_get_resource" },
	{ 0xfb2dff80, "device_create" },
	{ 0x4c6a3efe, "platform_device_unregister" },
	{ 0x2072ee9b, "request_threaded_irq" },
	{ 0x8c22622f, "platform_driver_register" },
	{ 0x6e5ef1f4, "wmt_codec_msvd_reset" },
	{ 0x29f8b407, "dma_alloc_coherent" },
	{ 0xb39b257, "cdev_add" },
	{ 0xe87acce2, "kmem_cache_alloc" },
	{ 0xc5433fa, "platform_device_register" },
	{ 0xc2018d04, "wmt_codec_pmc_ctl" },
	{ 0x3bd1b1f6, "msecs_to_jiffies" },
	{ 0xd62c833f, "schedule_timeout" },
	{ 0xc2165d85, "__arm_iounmap" },
	{ 0xc2161e33, "_raw_spin_lock" },
	{ 0x72542c85, "__wake_up" },
	{ 0x425456c9, "cpu_cache" },
	{ 0x2aca9e60, "wmt_codec_timer_exit" },
	{ 0x37a0cba, "kfree" },
	{ 0x2921f3a5, "remap_pfn_range" },
	{ 0x9d669763, "memcpy" },
	{ 0x32f80ea9, "prepare_to_wait" },
	{ 0x27d66eb3, "class_destroy" },
	{ 0x59865b90, "kill_fasync" },
	{ 0xf83178bd, "finish_wait" },
	{ 0x40a6f522, "__arm_ioremap" },
	{ 0xc23dbae6, "platform_driver_unregister" },
	{ 0x75349d4a, "mb_do_alloc" },
	{ 0xe5d95985, "param_ops_ulong" },
	{ 0xac8f37b2, "outer_cache" },
	{ 0x9cadc595, "__class_create" },
	{ 0xd5d62366, "wmt_codec_clock_en" },
	{ 0xf20dabd8, "free_irq" },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=g_wmt_vd_com";


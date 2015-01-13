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
	{ 0x1d122fd8, "kmalloc_caches" },
	{ 0x17a142df, "__copy_from_user" },
	{ 0x311b7963, "_raw_spin_unlock" },
	{ 0xaaf31686, "wmt_vd_register" },
	{ 0x98082893, "__copy_to_user" },
	{ 0xc8b57c27, "autoremove_wake_function" },
	{ 0x288cb6b7, "wmt_codec_timer_reset" },
	{ 0xf7802486, "__aeabi_uidivmod" },
	{ 0xe2f241de, "wmt_codec_timer_stop" },
	{ 0x91715312, "sprintf" },
	{ 0xd63e03ec, "mb_do_free" },
	{ 0xcc3bef4d, "wmt_codec_timer_init" },
	{ 0x7f4c1551, "wmt_codec_write_prd" },
	{ 0xe707d823, "__aeabi_uidiv" },
	{ 0xfa2a45e, "__memzero" },
	{ 0x5f754e5a, "memset" },
	{ 0x27e1a049, "printk" },
	{ 0x89237345, "mb_do_phys_to_virt" },
	{ 0x606db7f2, "wmt_codec_unlock" },
	{ 0x2072ee9b, "request_threaded_irq" },
	{ 0x6e5ef1f4, "wmt_codec_msvd_reset" },
	{ 0x8fda82cc, "module_put" },
	{ 0xe87acce2, "kmem_cache_alloc" },
	{ 0xc2018d04, "wmt_codec_pmc_ctl" },
	{ 0x3bd1b1f6, "msecs_to_jiffies" },
	{ 0xd62c833f, "schedule_timeout" },
	{ 0x82b4b450, "wmt_vd_unregister" },
	{ 0xc2161e33, "_raw_spin_lock" },
	{ 0x72542c85, "__wake_up" },
	{ 0x2aca9e60, "wmt_codec_timer_exit" },
	{ 0x37a0cba, "kfree" },
	{ 0xa0fde0d9, "wmt_codec_lock" },
	{ 0x32f80ea9, "prepare_to_wait" },
	{ 0xf83178bd, "finish_wait" },
	{ 0x75349d4a, "mb_do_alloc" },
	{ 0xac8f37b2, "outer_cache" },
	{ 0xd5d62366, "wmt_codec_clock_en" },
	{ 0xf20dabd8, "free_irq" },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=g_wmt_vd_com";


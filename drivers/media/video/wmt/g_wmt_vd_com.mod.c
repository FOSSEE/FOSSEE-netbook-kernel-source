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
	{ 0xb509ece8, "cdev_alloc" },
	{ 0x8888fdc7, "cdev_del" },
	{ 0x1d122fd8, "kmalloc_caches" },
	{ 0x193eaea8, "cdev_init" },
	{ 0xd8e484f0, "register_chrdev_region" },
	{ 0x38f42821, "down_interruptible" },
	{ 0xdcefeadd, "device_destroy" },
	{ 0xb91cfb43, "platform_bus_type" },
	{ 0xaab4b6f0, "mutex_unlock" },
	{ 0x7485e15e, "unregister_chrdev_region" },
	{ 0xe707d823, "__aeabi_uidiv" },
	{ 0xfa2a45e, "__memzero" },
	{ 0x5f754e5a, "memset" },
	{ 0x74c97f9c, "_raw_spin_unlock_irqrestore" },
	{ 0x2f7962a1, "down_trylock" },
	{ 0x27e1a049, "printk" },
	{ 0x95687190, "dma_free_coherent" },
	{ 0xadc6f1fa, "mutex_lock" },
	{ 0x609a1f02, "down" },
	{ 0xfb2dff80, "device_create" },
	{ 0x4c6a3efe, "platform_device_unregister" },
	{ 0x2072ee9b, "request_threaded_irq" },
	{ 0x8c22622f, "platform_driver_register" },
	{ 0x29f8b407, "dma_alloc_coherent" },
	{ 0xb39b257, "cdev_add" },
	{ 0xe87acce2, "kmem_cache_alloc" },
	{ 0xc5433fa, "platform_device_register" },
	{ 0x3bd1b1f6, "msecs_to_jiffies" },
	{ 0x8e2e7ff8, "create_proc_entry" },
	{ 0xbd7083bc, "_raw_spin_lock_irqsave" },
	{ 0x4f68e5c9, "do_gettimeofday" },
	{ 0x37a0cba, "kfree" },
	{ 0x2eb22412, "up" },
	{ 0x27d66eb3, "class_destroy" },
	{ 0xb742fd7, "simple_strtol" },
	{ 0xc23dbae6, "platform_driver_unregister" },
	{ 0x9cadc595, "__class_create" },
	{ 0x3c3e55e, "wmt_getsyspara" },
	{ 0x635b340, "down_timeout" },
	{ 0xf20dabd8, "free_irq" },
	{ 0xd15ada1d, "auto_pll_divisor" },
	{ 0xe914e41e, "strcpy" },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";


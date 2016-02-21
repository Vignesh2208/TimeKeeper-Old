#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

__visible struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

static const struct modversion_info ____versions[]
__used
__attribute__((section("__versions"))) = {
	{ 0xe2c2f020, __VMLINUX_SYMBOL_STR(module_layout) },
	{ 0x9f37a200, __VMLINUX_SYMBOL_STR(kmalloc_caches) },
	{ 0x12da5bb2, __VMLINUX_SYMBOL_STR(__kmalloc) },
	{ 0xb2947494, __VMLINUX_SYMBOL_STR(call_usermodehelper_exec) },
	{ 0x68e2f221, __VMLINUX_SYMBOL_STR(_raw_spin_unlock) },
	{ 0xd0d8621b, __VMLINUX_SYMBOL_STR(strlen) },
	{ 0x2d37342e, __VMLINUX_SYMBOL_STR(cpu_online_mask) },
	{ 0x1661cead, __VMLINUX_SYMBOL_STR(find_vpid) },
	{ 0x81e4d30c, __VMLINUX_SYMBOL_STR(hrtimer_cancel) },
	{ 0xfeebd0f3, __VMLINUX_SYMBOL_STR(remove_proc_entry) },
	{ 0x5b19634d, __VMLINUX_SYMBOL_STR(div_s64_rem) },
	{ 0x93554f64, __VMLINUX_SYMBOL_STR(mutex_unlock) },
	{ 0x54efb5d6, __VMLINUX_SYMBOL_STR(cpu_number) },
	{ 0x91715312, __VMLINUX_SYMBOL_STR(sprintf) },
	{ 0xdfb5d5a3, __VMLINUX_SYMBOL_STR(kthread_create_on_node) },
	{ 0xe2d5255a, __VMLINUX_SYMBOL_STR(strcmp) },
	{ 0x8b18496f, __VMLINUX_SYMBOL_STR(__copy_to_user_ll) },
	{ 0x68dfc59f, __VMLINUX_SYMBOL_STR(__init_waitqueue_head) },
	{ 0x4f8b5ddb, __VMLINUX_SYMBOL_STR(_copy_to_user) },
	{ 0x2bc95bd4, __VMLINUX_SYMBOL_STR(memset) },
	{ 0xa69ad6bd, __VMLINUX_SYMBOL_STR(proc_mkdir) },
	{ 0xfb0c78bd, __VMLINUX_SYMBOL_STR(current_task) },
	{ 0xbaf9b575, __VMLINUX_SYMBOL_STR(__mutex_init) },
	{ 0x50eedeb8, __VMLINUX_SYMBOL_STR(printk) },
	{ 0x8e166e2a, __VMLINUX_SYMBOL_STR(kthread_stop) },
	{ 0x84e79d99, __VMLINUX_SYMBOL_STR(netlink_kernel_release) },
	{ 0xb6ed1e53, __VMLINUX_SYMBOL_STR(strncpy) },
	{ 0xb4390f9a, __VMLINUX_SYMBOL_STR(mcount) },
	{ 0x1fc74210, __VMLINUX_SYMBOL_STR(mutex_lock) },
	{ 0xa3bc4a58, __VMLINUX_SYMBOL_STR(poll_freewait) },
	{ 0x79f1f7d, __VMLINUX_SYMBOL_STR(netlink_unicast) },
	{ 0xa799ae84, __VMLINUX_SYMBOL_STR(pid_task) },
	{ 0xaafaf9e7, __VMLINUX_SYMBOL_STR(init_net) },
	{ 0x54d07ccd, __VMLINUX_SYMBOL_STR(fput) },
	{ 0xee7e03f6, __VMLINUX_SYMBOL_STR(poll_initwait) },
	{ 0x8ff4079b, __VMLINUX_SYMBOL_STR(pv_irq_ops) },
	{ 0xa073e1b, __VMLINUX_SYMBOL_STR(__alloc_skb) },
	{ 0xf0fdf6cb, __VMLINUX_SYMBOL_STR(__stack_chk_fail) },
	{ 0x4292364c, __VMLINUX_SYMBOL_STR(schedule) },
	{ 0x86a4889a, __VMLINUX_SYMBOL_STR(kmalloc_order_trace) },
	{ 0xbdb257ba, __VMLINUX_SYMBOL_STR(hrtimer_start) },
	{ 0x824efe08, __VMLINUX_SYMBOL_STR(pv_cpu_ops) },
	{ 0x29728eaa, __VMLINUX_SYMBOL_STR(wake_up_process) },
	{ 0xe86a637d, __VMLINUX_SYMBOL_STR(fget_light) },
	{ 0x9939e5d0, __VMLINUX_SYMBOL_STR(kmem_cache_alloc_trace) },
	{ 0x67f7403e, __VMLINUX_SYMBOL_STR(_raw_spin_lock) },
	{ 0x70562c49, __VMLINUX_SYMBOL_STR(sched_setscheduler) },
	{ 0xe45f60d8, __VMLINUX_SYMBOL_STR(__wake_up) },
	{ 0xbe2a0dd3, __VMLINUX_SYMBOL_STR(call_usermodehelper_setup) },
	{ 0xb3f7646e, __VMLINUX_SYMBOL_STR(kthread_should_stop) },
	{ 0xa56d356, __VMLINUX_SYMBOL_STR(prepare_to_wait_event) },
	{ 0x38d64aac, __VMLINUX_SYMBOL_STR(proc_create_data) },
	{ 0x4f68e5c9, __VMLINUX_SYMBOL_STR(do_gettimeofday) },
	{ 0x78441ae9, __VMLINUX_SYMBOL_STR(find_get_pid) },
	{ 0x526b8a62, __VMLINUX_SYMBOL_STR(__netlink_kernel_create) },
	{ 0x37a0cba, __VMLINUX_SYMBOL_STR(kfree) },
	{ 0xcbe08737, __VMLINUX_SYMBOL_STR(send_sig_info) },
	{ 0x781c6eca, __VMLINUX_SYMBOL_STR(hrtimer_init) },
	{ 0x74c134b9, __VMLINUX_SYMBOL_STR(__sw_hweight32) },
	{ 0x75bb675a, __VMLINUX_SYMBOL_STR(finish_wait) },
	{ 0x4f6b400b, __VMLINUX_SYMBOL_STR(_copy_from_user) },
	{ 0x66d5f3b5, __VMLINUX_SYMBOL_STR(__nlmsg_put) },
	{        0, __VMLINUX_SYMBOL_STR(sys_close) },
	{ 0x4cdb3178, __VMLINUX_SYMBOL_STR(ns_to_timeval) },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";


MODULE_INFO(srcversion, "A4DA714FDF0C8CF4EB9E4D6");

#include <linux/kernel.h>
#include <linux/prinfo.h>
#include <uapi/asm-generic/errno-base.h>
#include <linux/sched.h>


asmlinkage int sys_ptree(struct prinfo *buf,int *nr) {
	printk("Not implemented\n");
	return 0;
}

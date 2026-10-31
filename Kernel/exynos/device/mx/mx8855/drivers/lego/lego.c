#include <linux/module.h>
#include <linux/init.h>

static int __init lego_init(void)
{
	pr_info("LEGO : Hello from LEGO Module");
	return 0;
}

static void __exit lego_exit(void)
{
	pr_info("LEGO : Bye from LEGO Module");
}

module_init(lego_init);
module_exit(lego_exit);

MODULE_DESCRIPTION("LEGO World Module");
MODULE_AUTHOR("LEGO");
MODULE_LICENSE("GPL v2");


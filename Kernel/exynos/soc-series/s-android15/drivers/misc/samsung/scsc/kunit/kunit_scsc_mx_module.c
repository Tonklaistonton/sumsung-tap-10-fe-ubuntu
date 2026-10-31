static int scsc_mx_module_init(void);
int kunit_scsc_mx_module_init(void)
{
	scsc_mx_module_init();
	return 0;
}

static void scsc_mx_module_exit(void);
void kunit_scsc_mx_module_exit(void)
{
	scsc_mx_module_exit();
	return 0;
}

static void scsc_mx_module_remove(struct scsc_mif_abs *abs);
void kunit_scsc_mx_module_remove(struct scsc_mif_abs *abs)
{
	scsc_mx_module_remove(abs);
}

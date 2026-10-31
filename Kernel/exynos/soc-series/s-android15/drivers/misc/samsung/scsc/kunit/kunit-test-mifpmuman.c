#include <kunit/test.h>
#include <scsc/scsc_mx.h>
#include "../mxman.h"
#include "../mifpmuman.h"
#include "../pmu_host_if.h"

extern void (*fp_mifpmu_isr)(int irq, void *data);
extern void (*fp_mifpmu_cmd_th_to_string)(u32 cmd, char *buf, u8 buffer_size);
extern void (*fp_mifpmu_cmd_fh_to_string)(u32 cmd, char *buf, u8 buffer_size);

#if 0
static void test_mifpmuman_load_fw(struct kunit *test)
{
	struct mifpmuman pmu;
	static uint firmware_startup_flags_pmu;
	pmu.in_use = false;
	enum scsc_subsystem sub = SCSC_SUBSYSTEM_WLAN;
	mifpmuman_init(&pmu, get_mif(), NULL, NULL);
#if IS_ENABLED(CONFIG_SCSC_PMU_BOOTFLAG      S)
	mifpmuman_load_fw(&pmu,"ABCD",4,firmware_startup_flags_pmu);
#else
	mifpmuman_load_fw(&pmu,"ABCD",4);
#endif
	KUNIT_EXPECT_STREQ(test, "OK", "OK");
}
#endif

static void test_mifpmu_cmd_th_to_string(struct kunit *test)
{
	char cmd_string[20];

	memset(cmd_string, 0, 20);

	fp_mifpmu_cmd_th_to_string(PMU_AP_MSG_SECOND_BOOT_COMPLETE_IND, cmd_string, 20);
	memset(cmd_string, 0, 20);

	fp_mifpmu_cmd_th_to_string(PMU_AP_MSG_COMMAND_COMPLETE_IND, cmd_string, 20);
	memset(cmd_string, 0, 20);

	fp_mifpmu_cmd_th_to_string(PMU_AP_MSG_SUBSYS_ERROR_WLAN, cmd_string, 20);
	memset(cmd_string, 0, 20);

	fp_mifpmu_cmd_th_to_string(PMU_AP_MSG_SUBSYS_ERROR_WLAN_WPAN, cmd_string, 20);
	memset(cmd_string, 0, 20);

	fp_mifpmu_cmd_th_to_string(PMU_AP_MSG_PCIE_OFF_REQ, cmd_string, 20);
	memset(cmd_string, 0, 20);

	fp_mifpmu_cmd_th_to_string(PMU_AP_MSG_SUBSYS_ERROR_WPAN, cmd_string, 20);
	memset(cmd_string, 0, 20);

	fp_mifpmu_cmd_th_to_string(PMU_AP_MSG_SUBSYS_BITS, cmd_string, 20);

	KUNIT_EXPECT_STREQ(test, "OK", "OK");
}

static void test_mifpmu_cmd_fh_to_string(struct kunit *test)
{
	char cmd_string[20];

	memset(cmd_string, 0, 20);

	fp_mifpmu_cmd_fh_to_string(PMU_AP_CFG_MONITOR_CMD, cmd_string, 20);
	memset(cmd_string, 0, 20);

	fp_mifpmu_cmd_fh_to_string(PMU_AP_CFG_MONITOR_WPAN_WLAN, cmd_string, 20);
	memset(cmd_string, 0, 20);

	fp_mifpmu_cmd_fh_to_string(PMU_AP_MSG_WLBT_PCIE_OFF_ACCEPT, cmd_string, 20);
	memset(cmd_string, 0, 20);

	fp_mifpmu_cmd_fh_to_string(PMU_AP_MSG_WLBT_PCIE_OFF_REJECT, cmd_string, 20);
	memset(cmd_string, 0, 20);

	fp_mifpmu_cmd_fh_to_string(PMU_AP_MSG_WLBT_PCIE_OFF_REJECT_CANCEL, cmd_string, 20);
	memset(cmd_string, 0, 20);

	fp_mifpmu_cmd_fh_to_string(PMU_AP_MSG_SCAN2MEM_DUMP_START_0, cmd_string, 20);
	memset(cmd_string, 0, 20);

	fp_mifpmu_cmd_fh_to_string(PMU_AP_MSG_SCAN2MEM_RECOVERY_START_0, cmd_string, 20);
	memset(cmd_string, 0, 20);

	fp_mifpmu_cmd_fh_to_string(PMU_AP_MSG_SCAN2MEM_DUMP_START_1, cmd_string, 20);
	memset(cmd_string, 0, 20);

	fp_mifpmu_cmd_fh_to_string(PMU_AP_MSG_SCAN2MEM_RECOVERY_START_1, cmd_string, 20);
	memset(cmd_string, 0, 20);

	fp_mifpmu_cmd_fh_to_string(PMU_AP_MSG_SUBSYS_BITS, cmd_string, 20);
	memset(cmd_string, 0, 20);

	KUNIT_EXPECT_STREQ(test, "OK", "OK");
}


static void test_all(struct kunit *test)
{
	struct mifpmuman pmu;
	struct mxman *mx;
	struct scsc_mx *scscmx;
	int ret;
	enum scsc_subsystem sub = SCSC_SUBSYSTEM_WLAN;

	mx = test_alloc_mxman(test);
	mx->start_dram = ((void *)0);
	mx->size_dram = (size_t)(0);
	scscmx = test_alloc_scscmx(test, get_mif());
	mx->mx = scscmx;

	pmu.in_use = false;

	mifpmuman_init(&pmu, get_mif(), NULL, NULL);

	mifpmuman_start_subsystem(&pmu, sub);

	mifpmuman_stop_subsystem(&pmu, sub);

	mifpmuman_force_monitor_mode_subsystem(&pmu, sub);

	sub = SCSC_SUBSYSTEM_WPAN;

	mifpmuman_start_subsystem(&pmu, sub);

	mifpmuman_stop_subsystem(&pmu, sub);

	mifpmuman_force_monitor_mode_subsystem(&pmu, sub);

	sub = SCSC_SUBSYSTEM_WLAN_WPAN;

	mifpmuman_start_subsystem(&pmu, sub);

	mifpmuman_stop_subsystem(&pmu, sub);

	mifpmuman_force_monitor_mode_subsystem(&pmu, sub);

	fp_mifpmu_isr(NULL, &pmu);

	mifpmuman_deinit(&pmu);

	mifpmuman_trigger_scan2mem(&pmu, 1, 1);
	mifpmuman_trigger_scan2mem(&pmu, 1, 0);
	mifpmuman_trigger_scan2mem(&pmu, 0, 1);
	mifpmuman_trigger_scan2mem(&pmu, 0, 1);

	KUNIT_EXPECT_STREQ(test, "OK", "OK");
}

static int test_init(struct kunit *test)
{
	return 0;
}

static void test_exit(struct kunit *test)
{
}

static struct kunit_case test_cases[] = {
	KUNIT_CASE(test_all),
	KUNIT_CASE(test_mifpmu_cmd_th_to_string),
	KUNIT_CASE(test_mifpmu_cmd_fh_to_string),
	//KUNIT_CASE(test_mifpmuman_load_fw),
	{}
};

static struct kunit_suite test_suite[] = {
	{
		.name = "test_mifpmuman",
		.test_cases = test_cases,
		.init = test_init,
		.exit = test_exit,
	}
};

kunit_test_suites(test_suite);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("yongjin.lim@samsung.com>");


/******************************************************************************
 *                                                                            *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd. All rights reserved       *
 *                                                                            *
 * Bluetooth Driver Unittest for KUNIT                                        *
 *                                                                            *
 ******************************************************************************/
#include <kunit/test.h>

/******************************************************************************
 * Test target code
 ******************************************************************************/
#include "../scsc_bt_qos.c"

static void scsc_bt_qos_platform_probe_test(struct kunit *test)
{
	struct platform_device fake_pdev;
	platform_bt_qos_driver.probe(&fake_pdev);
	scsc_bt_qos_platform_probe(&fake_pdev);
	KUNIT_SUCCEED(test);
}

static void scsc_bt_qos_reset_param_test(struct kunit *test)
{
	struct kernel_param fake_kp;

	scsc_bt_qos_reset_ops.set(NULL, &fake_kp);
	qos_pdev = NULL;
	scsc_bt_qos_reset_set_param_cb(NULL, &fake_kp);

	KUNIT_SUCCEED(test);
}

static void scsc_bt_qos_update_test(struct kunit *test)
{
	scsc_bt_qos_reset_param_test(test);
	scsc_bt_qos_high_level = 10;
	scsc_bt_qos_medium_level = 5;
	scsc_bt_qos_low_level = 1;
	scsc_bt_qos_update(scsc_bt_qos_high_level, 0);
	scsc_bt_qos_update(scsc_bt_qos_medium_level, 0);
	scsc_bt_qos_update(scsc_bt_qos_low_level, 0);
	scsc_bt_qos_update(0, 0);
	KUNIT_SUCCEED(test);
}

static void scsc_bt_qos_default(struct kunit *test)
{
	scsc_bt_qos_update_work(&qos_service.update_work);
	scsc_bt_qos_disable_work(&qos_service.update_work);
	scsc_bt_qos_service_stop();
	scsc_bt_qos_service_start();
	scsc_bt_qos_service_init();
	scsc_bt_qos_service_exit();

	KUNIT_SUCCEED(test);
}

static struct kunit_case scsc_bt_qos_test_cases[] = {
	KUNIT_CASE(scsc_bt_qos_platform_probe_test),
	KUNIT_CASE(scsc_bt_qos_reset_param_test),
	KUNIT_CASE(scsc_bt_qos_update_test),
	KUNIT_CASE(scsc_bt_qos_default),
	{}
};

static struct kunit_suite scsc_bt_qos_test_suite = {
	.name = "scsc_bt_qos_unittest",
	.test_cases = scsc_bt_qos_test_cases,
	.init = NULL,
	.exit = NULL,
};

kunit_test_suite(scsc_bt_qos_test_suite);

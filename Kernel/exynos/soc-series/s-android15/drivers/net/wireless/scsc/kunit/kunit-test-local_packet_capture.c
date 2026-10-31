#include <kunit/test.h>

#include "kunit-common.h"
#include "../local_packet_capture.h"

static int local_packet_capture_test_init(struct kunit *test)
{
	test_dev_init(test);

	kunit_log(KERN_INFO, test, "%s: initialized.", __func__);
	return 0;
}

static void local_packet_capture_test_exit(struct kunit *test)
{
	kunit_log(KERN_INFO, test, "%s: completed.", __func__);
}

static struct kunit_case local_packet_capture_test_cases[] = {
	{}
};

static struct kunit_suite local_packet_capture_test_suite[] = {
	{
		.name = "kunit-local_packet_capture-test",
		.test_cases = local_packet_capture_test_cases,
		.init = local_packet_capture_test_init,
		.exit = local_packet_capture_test_exit,
	}
};

kunit_test_suites(local_packet_capture_test_suite);

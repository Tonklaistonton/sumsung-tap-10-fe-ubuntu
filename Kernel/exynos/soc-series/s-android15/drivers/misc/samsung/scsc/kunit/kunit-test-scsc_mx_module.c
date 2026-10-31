#include <kunit/test.h>
#include "common.h"
#include "srvman.h"
#include "../scsc_mif_abs.h"
#include "../scsc_mx_impl.h"

static void test_all(struct kunit *test)
{

	struct mxman *mx;
	struct scsc_mx *scscmx;
	struct whdr *whdr;
	struct srvman srvman;
	struct srvman *srv;
	struct scsc_service *service;
	struct scsc_mif_abs *mif_abs;

	mx = test_alloc_mxman(test);
	mx->mxman_state = MXMAN_STATE_STARTED_WLAN;
	mx->mxman_next_state = MXMAN_STATE_FAILED_WLAN;
	mx->last_syserr.level = MX_SYSERR_LEVEL_8;
	scscmx = test_alloc_scscmx(test, get_mif());
	srvman.error = false;
	set_srvman(scscmx, srvman);
	mif_abs = scsc_mx_get_mif_abs(scscmx);
	mx->mx = scscmx;
	mx->scsc_panic_code = 0x0000;
	whdr = test_alloc_whdr(test);
	mx->fw_wlan = get_fwhdr_if(whdr);

	kunit_scsc_mx_module_init();
	kunit_scsc_mx_module_remove(mif_abs);

	//srv = scsc_mx_get_srvman(mx->mx);
	//srvman_init(srv, scscmx);
	//service = test_alloc_scscservice(test);
	//list_add_service(service, srv);
	//kunit_scsc_mx_module_exit();

	//scsc_mx_module_client_remove(MXMAN_FAILURE_WORK);
	//srvman_set_error(srv, ALLOWED_START_STOP);
	//scsc_mx_module_client_probe(MXMAN_FAILURE_WORK);
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
	{}
};

static struct kunit_suite test_suite[] = {
	{
		.name = "test_scsc_mx_module",
		.test_cases = test_cases,
		.init = test_init,
		.exit = test_exit,
	}
};

kunit_test_suites(test_suite);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("youngss.kim@samsung.com>");


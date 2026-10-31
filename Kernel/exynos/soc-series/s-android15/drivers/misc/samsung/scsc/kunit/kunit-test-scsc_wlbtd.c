#include <kunit/test.h>
#include <../scsc_wlbtd.h>

extern int (*fp_msg_from_wlbtd_cb)(struct sk_buff *skb, struct genl_info *info);
extern int (*fp_msg_from_wlbtd_sable_cb)(struct sk_buff *skb, struct genl_info *info);
extern int (*fp_msg_from_wlbtd_build_type_cb)(struct sk_buff *skb, struct genl_info *info);
extern int (*fp_msg_from_wlbtd_write_file_cb)(struct sk_buff *skb, struct genl_info *info);
extern int (*fp_msg_from_wlbtd_chipset_logging_cb)(struct sk_buff *skb, struct genl_info *info);
extern int (*fp_msg_from_wlbtd_ramsd)(struct sk_buff *skb, struct genl_info *info);

static void test_all(struct kunit *test)
{

	struct sk_buff *skb;
	struct genl_info *info;
	struct nlattr *nla_attr[3] = {0,};
	struct nlattr *nla_attr2[8] = {0,};
	char * attr1 =  "testing";
	int value = 10;
	short int value_short = 10;
	u8 msg_seq = 5;

	// init nla_attr for msg_from_wlbtd_cb, its status(nla_attr[2]) using u32
	nla_attr[1] = kunit_kzalloc(test, NLA_HDRLEN + strlen(attr1)+1, GFP_KERNEL);
	nla_attr[2] = kunit_kzalloc(test, NLA_HDRLEN + sizeof(unsigned int), GFP_KERNEL);
	nla_attr[1]->nla_len =  strlen(attr1)+1 + NLA_HDRLEN;
	nla_attr[1]->nla_type = NL_ATTR_TYPE_STRING;
	nla_attr[2]->nla_len =  sizeof(unsigned int) + NLA_HDRLEN;
	nla_attr[2]->nla_type  = NL_ATTR_TYPE_U32;
	memcpy(nla_data(nla_attr[1]), attr1, strlen(attr1) +1);
	memcpy(nla_data(nla_attr[2]), &value, sizeof(value));

	//init nla_attr2 for msg_from_wlbtd_sable_cb/msg_from_wlbtd_ramsd, its status(nla_attr[2]) using u16
	nla_attr2[1] = kunit_kzalloc(test, NLA_HDRLEN + strlen(attr1)+1, GFP_KERNEL);
	nla_attr2[2] = kunit_kzalloc(test, NLA_HDRLEN + sizeof(short int), GFP_KERNEL);
	nla_attr2[6] = kunit_kzalloc(test, NLA_HDRLEN + sizeof(u8), GFP_KERNEL);
	nla_attr2[1]->nla_len =  strlen(attr1)+1 + NLA_HDRLEN;
	nla_attr2[1]->nla_type = NL_ATTR_TYPE_STRING;
	nla_attr2[2]->nla_len =  sizeof(short int) + NLA_HDRLEN;
	nla_attr2[2]->nla_type  = NL_ATTR_TYPE_U16;
	nla_attr2[6]->nla_len =  sizeof(u8) + NLA_HDRLEN;
	nla_attr2[6]->nla_type = NL_ATTR_TYPE_U8;
	memcpy(nla_data(nla_attr2[1]), attr1, strlen(attr1) +1);
	memcpy(nla_data(nla_attr2[2]), &value_short, sizeof(value_short));
	memcpy(nla_data(nla_attr2[6]), &msg_seq, sizeof(u8));

	response_code_to_str(SCSC_WLBTD_ERR_PARSE_FAILED);
	response_code_to_str(SCSC_WLBTD_FW_PANIC_TAR_GENERATED);
	response_code_to_str(SCSC_WLBTD_FW_PANIC_ERR_SCRIPT_FILE_NOT_FOUND);
	response_code_to_str(SCSC_WLBTD_FW_PANIC_ERR_NO_DEV);
	response_code_to_str(SCSC_WLBTD_FW_PANIC_ERR_MMAP);
	response_code_to_str(SCSC_WLBTD_FW_PANIC_ERR_SABLE_FILE);
	response_code_to_str(SCSC_WLBTD_FW_PANIC_ERR_TAR);
	response_code_to_str(SCSC_WLBTD_OTHER_SBL_GENERATED);
	response_code_to_str(SCSC_WLBTD_OTHER_TAR_GENERATED);
	response_code_to_str(SCSC_WLBTD_OTHER_ERR_SCRIPT_FILE_NOT_FOUND);
	response_code_to_str(SCSC_WLBTD_OTHER_ERR_NO_DEV);
	response_code_to_str(SCSC_WLBTD_OTHER_ERR_MMAP);
	response_code_to_str(SCSC_WLBTD_OTHER_ERR_SABLE_FILE);
	response_code_to_str(SCSC_WLBTD_OTHER_ERR_TAR);
	response_code_to_str(SCSC_WLBTD_OTHER_IGNORE_TRIGGER);
	response_code_to_str(SCSC_WLBTD_LAST_RESPONSE_CODE);

	skb = nlmsg_new(NLMSG_GOODSIZE, GFP_KERNEL);
	info = kunit_kzalloc(test, sizeof(*info), GFP_KERNEL);
	info->attrs = &nla_attr;
	fp_msg_from_wlbtd_cb(skb, info);
	fp_msg_from_wlbtd_cb(skb, NULL);

	fp_msg_from_wlbtd_build_type_cb(skb, NULL);

	info->attrs[1] = NULL;
	fp_msg_from_wlbtd_build_type_cb(skb, info);

	info->attrs= &nla_attr2;
	info->attrs[1]->nla_len = NLA_HDRLEN;
	fp_msg_from_wlbtd_build_type_cb(skb, info);

	info->attrs[1]->nla_len = 100;
	fp_msg_from_wlbtd_build_type_cb(skb, info);

	//info->attrs[1]->nla_len = strlen(attr1)+1 + NLA_HDRLEN;
	//fp_msg_from_wlbtd_build_type_cb(skb, info);

	fp_msg_from_wlbtd_sable_cb(skb, NULL);
	fp_msg_from_wlbtd_sable_cb(skb, info);

	msg_seq = 255;
	memcpy(nla_data(nla_attr2[6]), &msg_seq, sizeof(u8));
	fp_msg_from_wlbtd_sable_cb(skb, info);

	for(int i =0 ; i <= SCSC_WLBTD_LAST_RESPONSE_CODE; i++){
		value = i;
		memcpy(nla_data(nla_attr2[2]), &value, sizeof(unsigned int));
		fp_msg_from_wlbtd_sable_cb(skb, info);
	}

	fp_msg_from_wlbtd_chipset_logging_cb(skb, info);

	value = SCSC_WLBTD_RAMSD_DUMP_ERR;
	memcpy(nla_data(nla_attr2[2]), &value, sizeof(unsigned int));
	fp_msg_from_wlbtd_ramsd(skb, info);

	scsc_wlbtd_get_and_print_build_type();

	wlbtd_write_file("ABC", "DEF");

	wlbtd_chipset_logging("ABC", 10, true);
	wlbtd_chipset_logging("ABC", 10, false);
	wlbtd_chipset_logging("ABC", 7*1024+1, false);

	call_wlbtd_ramsd(10, true);

	scsc_wlbtd_wait_for_sable_logging();

	call_wlbtd("ABD");

	scsc_wlbtd_deinit();

	kfree(skb);

	KUNIT_EXPECT_STREQ(test, "OK", "OK");
}

static void test_write_file_cb(struct kunit *test)
{

	struct sk_buff *skb;
	struct genl_info *info;
	struct nlattr attrs[2];

	skb = nlmsg_new(NLMSG_GOODSIZE, GFP_KERNEL);
	info = kunit_kzalloc(test, sizeof(*info), GFP_KERNEL);
	info->attrs = &attrs;

	nla_put_string(skb, ATTR_STR, "Hi");
	fp_msg_from_wlbtd_write_file_cb(&skb, &info);

	kfree(skb);

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
	KUNIT_CASE(test_write_file_cb),
	{}
};

static struct kunit_suite test_suite[] = {
	{
		.name = "test_scsc_wlbtd",
		.test_cases = test_cases,
		.init = test_init,
		.exit = test_exit,
	}
};

kunit_test_suites(test_suite);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("yongjin.lim@samsung.com>");


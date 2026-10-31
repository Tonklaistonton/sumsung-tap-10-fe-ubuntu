# Needed since this subdir is symlinked in the main Kernel tree
# without this our samsung subdir is NOT cleaned.
clean-files := *.o *.ko

obj-$(CONFIG_SCSC_BT_KUNIT_TEST) += \
		scsc_bt_module_unittest.o\
		scsc_shm_unittest.o\
		scsc_bt_qos_unittest.o\
		scsc_avdtp_detect_unittest.o\
		scsc_bt_hci_uart_unittest.o\
		scsc_bt_virtual_uart_unittest.o\

obj-$(CONFIG_SCSC_BT_KUNIT_TEST) += fake.o mock_scsc_service.o
obj-$(CONFIG_SCSC_BT_KUNIT_TEST) += test_helper_wait.o

## Belows are related to INDEPENDENT_SUBSYSTEM
ccflags-$(CONFIG_SCSC_BT_KUNIT_TEST) += -DCONFIG_SCSC_INDEPENDENT_SUBSYSTEM
ccflags-$(CONFIG_SCSC_BT_KUNIT_TEST) += -DCONFIG_SCSC_QOS
ccflags-$(CONFIG_SCSC_BT_KUNIT_TEST) += -DUSE_H4_RECV_PKT_API
ccflags-$(CONFIG_SCSC_BT_KUNIT_TEST) += -O1 -fipa-sra

ifeq ($(CONFIG_SCSC_BT_KUNIT_TEST),y)
GCOV_PROFILE := y
endif

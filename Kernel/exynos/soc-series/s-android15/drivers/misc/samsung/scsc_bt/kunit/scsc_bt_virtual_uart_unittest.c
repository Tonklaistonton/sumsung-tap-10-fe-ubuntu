/******************************************************************************
 *                                                                            *
 * Copyright (c) 2024 Samsung Electronics Co., Ltd. All rights reserved       *
 *                                                                            *
 * Bluetooth Driver Unittest for KUNIT                                        *
 *                                                                            *
 ******************************************************************************/
#include <kunit/test.h>
#include <linux/serial_core.h>


/******************************************************************************
 * Mock
 ******************************************************************************/
#ifndef UNUSED
#define UNUSED(x)		((void)(x))
#endif

// function definition
#define uart_write_wakeup mock_uart_write_wakeup
#define uart_remove_one_port mock_uart_remove_one_port
#define uart_add_one_port mock_uart_add_one_port
#define uart_register_driver mock_uart_register_driver
#define uart_unregister_driver mock_uart_unregister_driver
#define tty_flip_buffer_push mock_tty_flip_buffer_push

// function mocked
void mock_uart_write_wakeup(struct uart_port *port)
{
	UNUSED(port);
}

int mock_uart_remove_one_port(struct uart_driver *drv, struct uart_port *uport)
{
	UNUSED(drv);
	UNUSED(uport);

	return 0;
}

int mock_uart_add_one_port(struct uart_driver *drv, struct uart_port *uport)
{
	UNUSED(drv);
	UNUSED(uport);

	return 0;
}

int mock_uart_register_driver(struct uart_driver *drv)
{
	if ((NULL != drv) && (!strcmp(drv->driver_name, "scsc_bt_virtual_serial")))
		return 0;
	else
		return -1;
}

void mock_uart_unregister_driver(struct uart_driver *drv)
{
	UNUSED(drv);
}

void mock_tty_flip_buffer_push(struct tty_port *port)
{
	UNUSED(port);
}

size_t mock_tty_insert_flip_string(struct tty_port *port, const unsigned char *chars, size_t size)
{
	UNUSED(port);
	UNUSED(chars);

	return size-1;
}


/******************************************************************************
 * Include the code under test
 ******************************************************************************/
#include "../scsc_bt_virtual_uart.c"


/******************************************************************************
 * Test target code
 ******************************************************************************/
static struct uart_port *util_mock_uart_port(struct kunit *test)
{
	struct uart_port *p_uart_port;
	p_uart_port = kmalloc(sizeof(struct uart_port), GFP_KERNEL);

	KUNIT_ASSERT_NOT_NULL(test, p_uart_port);

	return p_uart_port;
}

static void util_free_uart_port(struct uart_port *p_uart_port)
{
	if(p_uart_port)
		kfree(p_uart_port);
}

static void scsc_bt_virser_tx_empty_test(struct kunit *test)
{
	unsigned int ret;

	kunit_info(test, "scsc_bt_virser_tx_empty\n");
	ret = scsc_bt_virser_tx_empty(NULL);

	KUNIT_EXPECT_EQ(test, (unsigned int)TIOCSER_TEMT, ret);
}

static void scsc_bt_virser_get_mctrl_test(struct kunit *test)
{
	unsigned int ret;

	kunit_info(test, "scsc_bt_virser_get_mctrl\n");
	ret = scsc_bt_virser_get_mctrl(NULL);

	KUNIT_EXPECT_EQ(test, (unsigned int)(TIOCM_CAR | TIOCM_DSR | TIOCM_CTS), ret);
}

static void scsc_bt_virser_set_mctrl_test(struct kunit *test)
{
	kunit_info(test, "scsc_bt_virser_set_mctrl\n");
	scsc_bt_virser_set_mctrl(NULL, (unsigned int)(TIOCM_RTS | TIOCM_DTR | TIOCM_OUT1 | TIOCM_OUT2| TIOCM_LOOP));

	KUNIT_SUCCEED(test);

	/* Testing for abnormal conditions: exception bit Start */
	scsc_bt_virser_set_mctrl(NULL, (unsigned int)(TIOCM_CAR));

	KUNIT_SUCCEED(test);
	/* Testing for abnormal conditions: exception bit End */
}

static void scsc_bt_virser_stop_tx_test(struct kunit *test)
{
	struct uart_port *p_uport;

	p_uport = util_mock_uart_port(test);

	kunit_info(test, "scsc_bt_virser_stop_tx\n");
	tx_enabled = true;
	scsc_bt_virser_stop_tx(p_uport);

	KUNIT_EXPECT_FALSE(test, tx_enabled);

	util_free_uart_port(p_uport);
}

static void scsc_bt_virser_start_tx_test(struct kunit *test)
{
	struct uart_port *p_uport;
	char *buffer;
	struct tty_struct *tty;
	struct uart_state *u_state;

	p_uport = util_mock_uart_port(test);

	buffer = kmalloc(UART_XMIT_SIZE, GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, buffer);

	tty = kmalloc(sizeof(struct tty_struct), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, tty);
	tty->flow.stopped = false;

	u_state = kmalloc(sizeof(struct uart_state), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, u_state);
	u_state->port.tty = tty;
	u_state->xmit.buf = buffer;
	u_state->xmit.head = 4;
	u_state->xmit.tail = 0;

	p_uport->state = u_state;

	strcpy(buffer, "test");

	kunit_info(test, "scsc_bt_virser_start_tx\n");
	tx_enabled = false;

	/* Testing for abnormal conditions: uart_tx_stopped(port) Start */
	p_uport->hw_stopped = 1;
	scsc_bt_virser_start_tx(p_uport);

	KUNIT_SUCCEED(test);

	p_uport->hw_stopped = 0;
	/* Testing for abnormal conditions: uart_tx_stopped(port) Start */

	scsc_bt_virser_start_tx(p_uport);

	KUNIT_EXPECT_TRUE(test, tx_enabled);

	util_free_uart_port(p_uport);
	kfree(buffer);
	kfree(tty);
	kfree(u_state);
}

static void scsc_bt_virser_throttle_test(struct kunit *test)
{
	kunit_info(test, "scsc_bt_virser_throttle\n");
	throttle_enable = false;
	scsc_bt_virser_throttle(NULL);

	KUNIT_EXPECT_TRUE(test, throttle_enable);
}

static void scsc_bt_virser_unthrottle_test(struct kunit *test)
{
	kunit_info(test, "scsc_bt_virser_unthrottle\n");
	throttle_enable = true;
	scsc_bt_virser_unthrottle(NULL);

	KUNIT_EXPECT_FALSE(test, throttle_enable);
}

static void scsc_bt_virser_start_rx_test(struct kunit *test)
{
	kunit_info(test, "scsc_bt_virser_start_rx\n");
	rx_enabled = false;
	scsc_bt_virser_start_rx(NULL);

	KUNIT_EXPECT_TRUE(test, rx_enabled);
}

static void scsc_bt_virser_stop_rx_test(struct kunit *test)
{
	kunit_info(test, "scsc_bt_virser_stop_rx\n");
	rx_enabled = true;
	scsc_bt_virser_stop_rx(NULL);

	KUNIT_EXPECT_FALSE(test, rx_enabled);
}

static void scsc_bt_virser_startup_test(struct kunit *test)
{
	int ret;

	kunit_info(test, "scsc_bt_virser_startup\n");
	ret = scsc_bt_virser_startup(NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_TRUE(test, rx_enabled);
	KUNIT_EXPECT_TRUE(test, tx_enabled);
}

static void scsc_bt_virser_shutdown_test(struct kunit *test)
{
	kunit_info(test, "scsc_bt_virser_shutdown\n");
	scsc_bt_virser_shutdown(NULL);

	KUNIT_EXPECT_FALSE(test, rx_enabled);
	KUNIT_EXPECT_FALSE(test, tx_enabled);
}

static void scsc_bt_virser_set_termios_test(struct kunit *test)
{
	int ret = 0;

	kunit_info(test, "scsc_bt_virser_set_termios\n");
	scsc_bt_virser_set_termios(NULL, NULL, NULL);

	ret = strcmp(set_termios, "SET_TERMIOS");

	KUNIT_EXPECT_EQ(test, 0, ret);
}

static void scsc_bt_virser_type_test(struct kunit *test)
{
	char *chr;

	kunit_info(test, "scsc_bt_virser_type\n");
	chr = scsc_bt_virser_type(NULL);

	KUNIT_EXPECT_STREQ(test, "SCSC_BT_VIRTUAL_SERIAL", chr);
}

static void scsc_bt_virser_config_port_test(struct kunit *test)
{
	struct uart_port *p_uport;

	p_uport = util_mock_uart_port(test);

	kunit_info(test, "scsc_bt_virser_config_port\n");
	/* Testing for abnormal conditions: null port Start */
	scsc_bt_virser_config_port(NULL, 0);
	/* Testing for abnormal conditions: null port End */
	scsc_bt_virser_config_port(p_uport, UART_CONFIG_TYPE);

	KUNIT_SUCCEED(test);

	util_free_uart_port(p_uport);
}

static void scsc_bt_virtual_serial_rx_drain_test(struct kunit *test)
{
	char data[] = "test_data";
	size_t len = sizeof(data) - 1;
	int ret;

	kunit_info(test, "scsc_bt_virtual_serial_rx_drain\n");
	ret = scsc_bt_virtual_serial_rx_drain(data, len);

	KUNIT_EXPECT_EQ(test, (len-1), ret);
}

static void util_scsc_bt_virtual_serial_probe(struct kunit *test)
{
	int ret;
	struct uart_port *p_uport;
	struct platform_device *p_platform_device;

	p_uport = util_mock_uart_port(test);
	p_platform_device = kmalloc(sizeof(struct platform_device), GFP_KERNEL);

	KUNIT_ASSERT_NOT_NULL(test, p_platform_device);

	p_platform_device->dev.driver_data = p_uport;

	kunit_info(test, "scsc_bt_virtual_serial_probe\n");
	ret = scsc_bt_virtual_serial_probe(&p_platform_device);

	KUNIT_EXPECT_EQ(test, 0, ret);

	kfree(p_platform_device);
	util_free_uart_port(p_uport);
}

static void util_scsc_bt_virtual_serial_remove(struct kunit *test)
{
	int ret;
	struct uart_port *p_uport;
	struct platform_device *p_platform_device;

	p_uport = util_mock_uart_port(test);
	p_platform_device = kmalloc(sizeof(struct platform_device), GFP_KERNEL);

	KUNIT_ASSERT_NOT_NULL(test, p_platform_device);

	p_platform_device->dev.driver_data = p_uport;

	kunit_info(test, "scsc_bt_virtual_serial_remove\n");
	ret = scsc_bt_virtual_serial_remove(&p_platform_device);

	KUNIT_EXPECT_EQ(test, 0, ret);

	kfree(p_platform_device);
	util_free_uart_port(p_uport);
}

static void scsc_bt_virtual_serial_probe_remove_test(struct kunit *test)
{
	util_scsc_bt_virtual_serial_probe(test);
	util_scsc_bt_virtual_serial_remove(test);
}

static void util_scsc_bt_virtual_serial_init(struct kunit *test)
{
	int ret;

	/* Testing for abnormal conditions: uart_register_driver failed Start */
	scsc_bt_virser_drv.driver_name	= "scsc_bt_virtual_serial_temp";
	ret = scsc_bt_virtual_serial_init();

	KUNIT_EXPECT_LT(test, ret, 0);

	scsc_bt_virser_drv.driver_name	= "scsc_bt_virtual_serial";
	/* Testing for abnormal conditions: uart_register_driver failed End */

	kunit_info(test, "scsc_bt_virtual_serial_init\n");
	ret = scsc_bt_virtual_serial_init();

	KUNIT_EXPECT_EQ(test, 0, ret);

	/* Testing for abnormal conditions: platform_driver_register failed Start */
	ret = scsc_bt_virtual_serial_init();
	kunit_info(test, "ret=%d", ret);

	KUNIT_EXPECT_EQ(test, -EBUSY, ret);
	/* Testing for abnormal conditions: platform_driver_register failed End */
}

static void util_scsc_bt_virtual_serial_deinit(struct kunit *test)
{
	kunit_info(test, "scsc_bt_virtual_serial_deinit\n");
	scsc_bt_virtual_serial_deinit();

	KUNIT_SUCCEED(test);
}

static void scsc_bt_virtual_serial_init_deinit_test(struct kunit *test)
{
	util_scsc_bt_virtual_serial_init(test);
	util_scsc_bt_virtual_serial_deinit(test);
}

static struct kunit_case scsc_bt_virtual_uart_test_cases[] = {
	KUNIT_CASE(scsc_bt_virser_tx_empty_test),
	KUNIT_CASE(scsc_bt_virser_get_mctrl_test),
	KUNIT_CASE(scsc_bt_virser_set_mctrl_test),
	KUNIT_CASE(scsc_bt_virser_stop_tx_test),
	KUNIT_CASE(scsc_bt_virser_start_tx_test),
	KUNIT_CASE(scsc_bt_virser_throttle_test),
	KUNIT_CASE(scsc_bt_virser_unthrottle_test),
	KUNIT_CASE(scsc_bt_virser_start_rx_test),
	KUNIT_CASE(scsc_bt_virser_stop_rx_test),
	KUNIT_CASE(scsc_bt_virser_startup_test),
	KUNIT_CASE(scsc_bt_virser_shutdown_test),
	KUNIT_CASE(scsc_bt_virser_set_termios_test),
	KUNIT_CASE(scsc_bt_virser_type_test),
	KUNIT_CASE(scsc_bt_virser_config_port_test),
	KUNIT_CASE(scsc_bt_virtual_serial_rx_drain_test),
	KUNIT_CASE(scsc_bt_virtual_serial_probe_remove_test),
	KUNIT_CASE(scsc_bt_virtual_serial_init_deinit_test),
	{}
};

static struct kunit_suite scsc_bt_virtual_uart_test_suite = {
	.name = "scsc_bt_virtual_uart_unittest",
	.test_cases = scsc_bt_virtual_uart_test_cases,
	.init = NULL,
	.exit = NULL,
};

kunit_test_suite(scsc_bt_virtual_uart_test_suite);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("SSCR");
MODULE_VERSION("-develop");
MODULE_DESCRIPTION("scsc_bt driver KUnit");

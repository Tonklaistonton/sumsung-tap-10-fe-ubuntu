#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/serial.h>
#include <linux/serial_core.h>
#include <linux/of.h>
#include <linux/tty_flip.h>

#include <scsc/scsc_logring.h>

#include "scsc_bt_priv.h"
#include "scsc_shm.h"

#ifndef UNUSED
#define UNUSED(x)       ((void)(x))
#endif

static bool rx_enabled;
static bool tx_enabled;
static bool throttle_enable;
static char set_termios[20];

#ifdef CONFIG_SCSC_BT_KUNIT_TEST
extern size_t mock_tty_insert_flip_string(struct tty_port *port, const unsigned char *chars, size_t size);
#define tty_insert_flip_string(port, chars, size) mock_tty_insert_flip_string(port, chars, size)
#endif

static unsigned int scsc_bt_virser_tx_empty(struct uart_port *port)
{
	return TIOCSER_TEMT;
}

static unsigned int scsc_bt_virser_get_mctrl(struct uart_port *port)
{
	return TIOCM_CAR | TIOCM_DSR | TIOCM_CTS;
}

static void scsc_bt_virser_set_mctrl(struct uart_port *port, unsigned int mctrl)
{
	UNUSED(port);

	if (!(mctrl & TIOCM_RTS) || \
		!(mctrl & TIOCM_DTR) || \
		!(mctrl & TIOCM_OUT1) || \
		!(mctrl & TIOCM_OUT2) || \
		!(mctrl & TIOCM_LOOP)) {
		SCSC_TAG_WARNING(BT_COMMON, "scsc_bt_virser_set_mctrl set unknown ctl=%d\n", mctrl);
	}
}

static void scsc_bt_virser_stop_tx(struct uart_port *port)
{
	SCSC_TAG_DEBUG(BT_TX, "%s\n", __func__);
	if (tx_enabled)
		tx_enabled = false;
}

static void scsc_bt_virser_start_tx(struct uart_port *port)
{
	struct circ_buf *xmit = &port->state->xmit;

	SCSC_TAG_DEBUG(BT_TX, "%s\n", __func__);
	if (uart_circ_empty(xmit) || uart_tx_stopped(port)) {
		scsc_bt_virser_stop_tx(port);
		return;
	}

	if (!tx_enabled)
		tx_enabled = true;

	while (!uart_circ_empty(xmit)) {
		int ret = 0;
		int count = CIRC_CNT_TO_END(xmit->head, xmit->tail,
					    UART_XMIT_SIZE);

		if (count == 0)
			break;

		ret = (int)scsc_bt_shm_h4_write(NULL, xmit->buf + xmit->tail,
						count, NULL);
		if (ret < 0)
			break;

		uart_xmit_advance(port, ret);
	}

	if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS) {
		uart_write_wakeup(port);
	}

	if (uart_circ_empty(xmit))
		scsc_bt_virser_stop_tx(port);
}

static void scsc_bt_virser_throttle(struct uart_port *port)
{
	UNUSED(port);

	if (!throttle_enable)
		throttle_enable = true;
}

static void scsc_bt_virser_unthrottle(struct uart_port *port)
{
	UNUSED(port);

	if(throttle_enable)
		throttle_enable = false;
}

static void scsc_bt_virser_start_rx(struct uart_port *port)
{
	SCSC_TAG_DEBUG(BT_RX, "%s\n", __func__);
	if (!rx_enabled)
		rx_enabled = true;
}

static void scsc_bt_virser_stop_rx(struct uart_port *port)
{
	SCSC_TAG_DEBUG(BT_RX, "%s\n", __func__);
	if (rx_enabled)
		rx_enabled = false;
}

static int scsc_bt_virser_startup(struct uart_port *port)
{
	SCSC_TAG_INFO(BT_COMMON, "%s\n", __func__);
	rx_enabled = true;
	tx_enabled = true;
	return 0;
}

static void scsc_bt_virser_shutdown(struct uart_port *port)
{
	SCSC_TAG_INFO(BT_COMMON, "%s\n", __func__);
	rx_enabled = false;
	tx_enabled = false;
}

static void scsc_bt_virser_set_termios(struct uart_port *port,
       struct ktermios *termios, const struct ktermios *old)
{
	strncpy(set_termios, "SET_TERMIOS", sizeof(set_termios)-1);
	set_termios[sizeof(set_termios)-1] = '\0';
}

static const char *scsc_bt_virser_type(struct uart_port *port)
{
	return "SCSC_BT_VIRTUAL_SERIAL";
}

static void scsc_bt_virser_config_port(struct uart_port *port, int flags)
{
	if(!port)
		return;

	if (flags & UART_CONFIG_TYPE)
		port->type = 0;
}

static struct uart_ops scsc_bt_virser_ops = {
	.tx_empty	= scsc_bt_virser_tx_empty,
	.get_mctrl	= scsc_bt_virser_get_mctrl,
	.set_mctrl	= scsc_bt_virser_set_mctrl,
	.stop_tx	= scsc_bt_virser_stop_tx,
	.start_tx	= scsc_bt_virser_start_tx,
	.throttle       = scsc_bt_virser_throttle,
	.unthrottle     = scsc_bt_virser_unthrottle,
	.start_rx	= scsc_bt_virser_start_rx,
	.stop_rx	= scsc_bt_virser_stop_rx,
	.startup	= scsc_bt_virser_startup,
	.shutdown	= scsc_bt_virser_shutdown,
	.set_termios	= scsc_bt_virser_set_termios,
	.type		= scsc_bt_virser_type,
	.config_port	= scsc_bt_virser_config_port,
};

struct uart_port scsc_bt_virser_port = {
	.lock		= __SPIN_LOCK_UNLOCKED(scsc_bt_virser_port.lock),
	.iotype		= UPIO_MEM,
	.uartclk	= 0,
	.fifosize	= 16,
	.ops		= &scsc_bt_virser_ops,
	.flags		= UPF_BOOT_AUTOCONF,
	.line		= 0,
	.type		= (PORT_UNKNOWN-1), /* It doesn't have type */
};

int scsc_bt_virtual_serial_rx_drain(const void *buf, size_t len)
{
	struct uart_port *port = &scsc_bt_virser_port;
	int ret;

	SCSC_TAG_DEBUG(BT_RX, "scsc_bt_virtual_serial_rx_drain: %d\n", len);
	ret = tty_insert_flip_string(&port->state->port, buf, len);
	tty_flip_buffer_push(&port->state->port);
	return ret;
}

static struct uart_driver scsc_bt_virser_drv = {
	.owner		= THIS_MODULE,
	.driver_name	= "scsc_bt_virtual_serial",
	.nr		= 1,
	.dev_name	= "ttySBT_virtual",
};

static int scsc_bt_virtual_serial_probe(struct platform_device *pdev)
{
	int ret;

	SCSC_TAG_INFO(BT_COMMON, "%s\n", __func__);

	scsc_bt_virser_port.dev = &pdev->dev;
	ret = uart_add_one_port(&scsc_bt_virser_drv, &scsc_bt_virser_port);
	SCSC_TAG_DEBUG(BT_COMMON, "uart_add_one_port: %d\n", ret);

	platform_set_drvdata(pdev, &scsc_bt_virser_port);

	return ret;
}

static int scsc_bt_virtual_serial_remove(struct platform_device *dev)
{
	struct uart_port *port = dev_get_drvdata(&dev->dev);

	SCSC_TAG_INFO(BT_COMMON, "%s\n", __func__);

	uart_remove_one_port(&scsc_bt_virser_drv, port);

	return 0;
}

static const struct of_device_id scsc_bt_virtaul_serial[] = {
	{ .compatible = "samsung,scsc_bt_virtual_serial" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, scsc_bt_virtaul_serial);

static struct platform_driver scsc_bt_virtual_serial_driver = {
	.probe		= scsc_bt_virtual_serial_probe,
	.remove		= scsc_bt_virtual_serial_remove,
	.driver		= {
		.name	= "scsc-bt-virtual-serial",
		.owner	= THIS_MODULE,
		.pm	= NULL,
		.of_match_table	= of_match_ptr(scsc_bt_virtaul_serial),
	},
};

int scsc_bt_virtual_serial_init(void)
{
	int ret;

	SCSC_TAG_INFO(BT_COMMON, "%s\n", __func__);
	ret = uart_register_driver(&scsc_bt_virser_drv);
	if (ret < 0) {
		SCSC_TAG_ERR(BT_COMMON,
			"Failed to register SLSI BT Virtual SERIAL driver\n");
		return ret;
	}

	ret = platform_driver_register(&scsc_bt_virtual_serial_driver);
	if (ret < 0) {
		SCSC_TAG_ERR(BT_COMMON, "Failed to register platform driver\n");
		uart_unregister_driver(&scsc_bt_virser_drv);
		return ret;
	}

	return ret;
}

void scsc_bt_virtual_serial_deinit(void)
{
	SCSC_TAG_INFO(BT_COMMON, "%s\n", __func__);
	platform_driver_unregister(&scsc_bt_virtual_serial_driver);
	uart_unregister_driver(&scsc_bt_virser_drv);
}

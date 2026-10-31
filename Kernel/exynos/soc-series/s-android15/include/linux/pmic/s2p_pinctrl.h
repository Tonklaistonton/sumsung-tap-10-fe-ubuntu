#ifndef __S2P_PINCTRL_H
#define __S2P_PINCTRL_H

#include <linux/gpio/driver.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/pinctrl/pinconf-generic.h>
#include <linux/pinctrl/pinconf.h>
#include <linux/pinctrl/pinmux.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/seq_file.h>

#include <linux/../../drivers/pinctrl/core.h>
#include <linux/../../drivers/pinctrl/pinctrl-utils.h>

#include <linux/pmic/s2p.h>

struct s2p_gpio_bit {
	uint8_t gpio_oen_shift;
	uint8_t gpio_oen_mask;
	uint8_t gpio_out_shift;
	uint8_t gpio_out_mask;
	uint8_t gpio_pull_shift;
	uint8_t gpio_pull_mask;
	uint8_t gpio_drv_str_shift;
	uint8_t gpio_drv_str_mask;
	uint8_t gpio_mode_digit_input;
	uint8_t gpio_mode_digit_output;
	uint8_t gpio_pull_disable;
	uint8_t gpio_pull_down;
	uint8_t gpio_pull_up;
};

struct s2p_gpio_state {
	struct device *dev;
	struct s2p_dev *sdev;
	struct pinctrl_dev *ctrl;
	struct gpio_chip chip;
	struct irq_chip irq;
	uint16_t gpio_addr;
	uint32_t npins;
	const struct pingroup *groups;
	uint32_t ngroups;
	const char *const *gpio_groups;
	uint32_t gpio_groups_size;
	struct pinctrl_pin_desc *pin_desc;
	void __iomem *sysreg_base;
	struct s2p_gpio_bit *bit;
	const char *const *strengths;

	uint32_t (*get_gpio_output_func)(const uint32_t pin, const int val);
	uint8_t (*get_set_reg_func)(const uint32_t pin);
	uint8_t (*get_status_reg_func)(const uint32_t pin);
	int (*set_gpio_conf_output_func)(const struct s2p_gpio_state *state, uint32_t arg,
				     	uint32_t pin, int val_set);
};

/*
 * struct s2p_gpio_pad - keep current GPIO settings
 * @output_enabled: Set to true if GPIO output buffer logic is enabled.
 * @output: Cached pin output value
 * @pull: Constant current which flow trough GPIO output buffer.
 * @strength: Set strength value
 * @function: See s2p_gpio_functions[]
 */
struct s2p_gpio_pad {
	bool		output_enabled;
	uint32_t	output;
	uint32_t	pull;
	uint32_t	strength;
	uint32_t	function;
};

/* Samsung possible pin configuration parameters */
#define S2P_GPIO_CONF_DISABLE                  (PIN_CONFIG_END + 1)
#define S2P_GPIO_CONF_PULL_DOWN                (PIN_CONFIG_END + 2)
#define S2P_GPIO_CONF_PULL_UP                  (PIN_CONFIG_END + 3)
#define S2P_GPIO_CONF_INPUT_ENABLE             (PIN_CONFIG_END + 4)
#define S2P_GPIO_CONF_OUTPUT_ENABLE            (PIN_CONFIG_END + 5)
#define S2P_GPIO_CONF_OUTPUT                   (PIN_CONFIG_END + 6)
#define S2P_GPIO_CONF_DRIVE_STRENGTH           (PIN_CONFIG_END + 7)

static const char *const s2p_biases[] = {
	"no-pull", "pull-down", "pull-up", "not-use"
};

static const struct pinconf_generic_params s2p_gpio_bindings[] = {
	{"pmic-gpio,pull-disable",	S2P_GPIO_CONF_DISABLE,		0},
	{"pmic-gpio,pull-down",		S2P_GPIO_CONF_PULL_DOWN,	1},
	{"pmic-gpio,pull-up",		S2P_GPIO_CONF_PULL_UP,		2},
	{"pmic-gpio,input-enable",	S2P_GPIO_CONF_INPUT_ENABLE,	0},
	{"pmic-gpio,output-enable",	S2P_GPIO_CONF_OUTPUT_ENABLE,	1},
	{"pmic-gpio,output-low",	S2P_GPIO_CONF_OUTPUT,		0},
	{"pmic-gpio,output-high",	S2P_GPIO_CONF_OUTPUT,		1},
	{"pmic-gpio,drive-strength",	S2P_GPIO_CONF_DRIVE_STRENGTH,	0},
};

#ifdef CONFIG_DEBUG_FS
static const struct pin_config_item s2p_conf_items[ARRAY_SIZE(s2p_gpio_bindings)] = {
	PCONFDUMP(S2P_GPIO_CONF_DISABLE,	"pull-disable", NULL, true),
	PCONFDUMP(S2P_GPIO_CONF_PULL_DOWN,	"pull-down", NULL, true),
	PCONFDUMP(S2P_GPIO_CONF_PULL_UP,	"pull-up", NULL, true),
	PCONFDUMP(S2P_GPIO_CONF_INPUT_ENABLE,	"input-enable", NULL, true),
	PCONFDUMP(S2P_GPIO_CONF_OUTPUT_ENABLE,	"output-enable", NULL, true),
	PCONFDUMP(S2P_GPIO_CONF_OUTPUT,	"output-high", NULL, true),
	PCONFDUMP(S2P_GPIO_CONF_DRIVE_STRENGTH,"drive-strength", NULL, true),
};
#endif

/* The index of each function in s2p_gpio_functions[] array */
#define S2P_GPIO_FUNC_NORMAL		"normal"

enum s2p_gpio_func_index {
	S2P_GPIO_FUNC_INDEX_NORMAL,
};

static const char *const s2p_gpio_functions[] = {
	[S2P_GPIO_FUNC_INDEX_NORMAL]	= S2P_GPIO_FUNC_NORMAL,
};

extern int s2p_gpio_read(const struct s2p_gpio_state *state, const uint8_t addr);
extern int s2p_gpio_write(const struct s2p_gpio_state *state, const uint8_t addr, const uint8_t val);
extern int s2p_get_gpio_info(const struct s2p_gpio_state *state, const uint32_t pin);
extern int s2p_gpio_get_function_groups(const struct s2p_gpio_state *state, const char *const **groups,
					uint32_t *const num_qgroups);
extern int s2p_gpio_set_mux(const struct s2p_gpio_state *state, struct pinctrl_dev *pctldev,
				uint32_t func_selector, uint32_t group_selector);
extern int s2p_gpio_get_group_pins(struct s2p_gpio_state *state, uint32_t selector,
			     const uint32_t **pins, uint32_t *num_pins);
extern void s2p_gpio_pin_dbg_show(struct s2p_gpio_state *state, struct s2p_gpio_pad *pad,
				struct seq_file *s, uint32_t pin);
extern int s2p_gpio_pin_config_get(struct s2p_gpio_state *state, struct s2p_gpio_pad *pad,
				uint32_t pin, unsigned long *config);
extern int s2p_gpio_pin_config_set(const struct s2p_gpio_state *state, struct s2p_gpio_pad *pad,
			uint32_t pin, unsigned long *configs, uint32_t num_configs);
extern int s2p_gpio_direction_input(const struct s2p_gpio_state *state, uint32_t pin);
extern int s2p_gpio_direction_output(const struct s2p_gpio_state *state, uint32_t pin, int val);
extern int s2p_gpio_pin_xlate(struct s2p_gpio_state *state, const uint32_t gpio);
extern int s2p_gpio_of_xlate(struct s2p_gpio_state *state, struct gpio_chip *chip,
		       const struct of_phandle_args *gpio_desc, u32 *flags);
extern int s2p_gpio_populate(const struct s2p_gpio_state *state,
				struct s2p_gpio_pad *pad, const uint32_t pin);
#endif /* __S2P_PINCTRL_H */

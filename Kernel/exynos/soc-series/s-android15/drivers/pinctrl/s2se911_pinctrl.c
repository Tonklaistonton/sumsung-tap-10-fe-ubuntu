// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2024 Samsung Electronics Co., Ltd
 */

#include <linux/pmic/s2p_pinctrl.h>
#include <linux/pmic/s2se911-mfd.h>

/* VGPIO regs by AP sides */
//TODO: It is just check comment out for AP SRF, so It will be removed after finishing sw spec.
#define SYSREG_ALIVE				(0x13820000)
#define VGPIO_TX_R11				(0x82C)
#define SYSREG_SHIFT				(0x4)
#define ADDR_EN_SHIFT				(4)

enum s2se911_gpio_vgi_idx {
	SUB1_GPIO1_4,
	SUB2_GPIO1_4,
	SUB3_GPIO1_4,
	SUB4_GPIO1_4,
	SUB5_GPIO1_4,
	SUB_GPIO5,
	SUB_GPIO_GROUP_NUM,
};

static const uint32_t s2se911_gpio_group[SUB_GPIO_GROUP_NUM] = {
	SUB1_GPIO1_4, SUB2_GPIO1_4, SUB3_GPIO1_4,
	SUB4_GPIO1_4, SUB5_GPIO1_4, SUB_GPIO5,
};

enum s2se911_gpio_io_param {
	S2SE911_GPIO_MODE_DIGITAL_INPUT = (0 << S2SE911_GPIO_OEN_SHIFT),
	S2SE911_GPIO_MODE_DIGITAL_OUTPUT = (1 << S2SE911_GPIO_OEN_SHIFT),
};

enum s2se911_gpio_pull_param {
	S2SE911_GPIO_PULL_DISABLE = (0 << S2SE911_GPIO_PULL_SHIFT),
	S2SE911_GPIO_PULL_DOWN = (1 << S2SE911_GPIO_PULL_SHIFT),
	S2SE911_GPIO_PULL_UP = (2 << S2SE911_GPIO_PULL_SHIFT),
};

static const unsigned int s2se911_gpio_pins[] = {0, 1, 2, 3, 4, 5};

#define S2SE911_GROUP_SIZE	1

static const struct pingroup s2se911_pin_groups[] = {
	PINCTRL_PINGROUP("SUB1_PMIC_GPIO", s2se911_gpio_pins, ARRAY_SIZE(s2se911_gpio_pins)),
	PINCTRL_PINGROUP("SUB2_PMIC_GPIO", s2se911_gpio_pins, ARRAY_SIZE(s2se911_gpio_pins)),
	PINCTRL_PINGROUP("SUB3_PMIC_GPIO", s2se911_gpio_pins, ARRAY_SIZE(s2se911_gpio_pins)),
	PINCTRL_PINGROUP("SUB4_PMIC_GPIO", s2se911_gpio_pins, ARRAY_SIZE(s2se911_gpio_pins)),
	PINCTRL_PINGROUP("SUB5_PMIC_GPIO", s2se911_gpio_pins, ARRAY_SIZE(s2se911_gpio_pins)),
};

static const char *const s2se911_gpio_groups[] = {
	"gpio_s0", "gpio_s1", "gpio_s2", "gpio_s3", "gpio_s4", "gpio_s5",
};

static struct pinctrl_pin_desc s2se911_pin_desc[] = {
	PINCTRL_PIN(0, "gpio_s0"),
	PINCTRL_PIN(1, "gpio_s1"),
	PINCTRL_PIN(2, "gpio_s2"),
	PINCTRL_PIN(3, "gpio_s3"),
	PINCTRL_PIN(4, "gpio_s4"),
	PINCTRL_PIN(5, "gpio_s5"),
};

static const char *const s2se911_strengths[] = {
	"2mA", "4mA", "6mA", "8mA"
};

static void s2se911_vgpio_write(const struct s2p_gpio_state *state, const uint32_t pin, uint32_t val)
{
	u32 reg = 0;
	u32 group = pin / 4;
	u32 offset = pin % 4;

	val &= 0x1;
	reg = readl(state->sysreg_base + group * SYSREG_SHIFT);
	reg = (val) ? (reg | (1 << offset)) : (reg & ~(1 << offset));
	reg |= 1 << (offset + ADDR_EN_SHIFT);
	writel(reg, state->sysreg_base + group * SYSREG_SHIFT);
}

static uint32_t s2se911_get_gpio_output(const uint32_t pin, const int val)
{
	return (val >> (pin % S2SE911_GPIO_RANGE)) & 0x1;
}

static uint8_t s2se911_get_set_reg(const uint32_t pin)
{
	return S2SE911_GPIO_GPIO0_SET + pin;
}

static uint8_t s2se911_get_status_reg(const uint32_t pin)
{
	return S2SE911_GPIO_GPIO_STATUS1;
}

static int s2se911_set_gpio_conf_output(const struct s2p_gpio_state *state, uint32_t arg,
		 			 uint32_t pin, int val_set)
{
	const int dev_type = state->sdev->device_type;
	uint32_t vgpio_group = (pin == S2SE911_GPIO5_IDX) ? s2se911_gpio_group[SUB_GPIO5] : s2se911_gpio_group[dev_type];

	vgpio_group *= S2SE911_GPIO_VGI_RANGE;

	if (pin == S2SE911_GPIO0_IDX) {
		arg = (arg << S2SE911_GPIO_OUT_SHIFT) & S2SE911_GPIO_OUT_MASK;
		val_set = (val_set & ~S2SE911_GPIO_OUT_MASK) | arg;
	} else if (pin == S2SE911_GPIO5_IDX) {
		s2se911_vgpio_write(state, vgpio_group + s2se911_gpio_group[dev_type], arg);
	} else
		s2se911_vgpio_write(state, (pin + vgpio_group) - 1, arg);

	return val_set;
}

static int s2se911_gpio_get_functions_count(struct pinctrl_dev *pctldev)
{
	return ARRAY_SIZE(s2p_gpio_functions);
}

static const char *s2se911_gpio_get_function_name(struct pinctrl_dev *pctldev,
					       uint32_t function)
{
	return s2p_gpio_functions[function];
}

static int s2se911_gpio_get_function_groups(struct pinctrl_dev *pctldev,
					 uint32_t selector,
					 const char *const **groups,
					 uint32_t *const num_qgroups)
{
	const struct s2p_gpio_state *s2se911_state = pinctrl_dev_get_drvdata(pctldev);
	int ret = 0;

	ret = s2p_gpio_get_function_groups(s2se911_state, groups, num_qgroups);

	return ret;
}

static int s2se911_gpio_set_mux(struct pinctrl_dev *pctldev, uint32_t func_selector,
				uint32_t group_selector)
{
	const struct s2p_gpio_state *s2se911_state = pinctrl_dev_get_drvdata(pctldev);
	int ret = 0;

	ret = s2p_gpio_set_mux(s2se911_state, pctldev, func_selector, group_selector);

	return ret;
}

static const struct pinmux_ops s2se911_gpio_pinmux_ops = {
	.get_functions_count	= s2se911_gpio_get_functions_count,
	.get_function_name	= s2se911_gpio_get_function_name,
	.get_function_groups	= s2se911_gpio_get_function_groups,
	.set_mux		= s2se911_gpio_set_mux,
};

static int s2se911_gpio_get_groups_count(struct pinctrl_dev *pctldev)
{
	struct s2p_gpio_state *s2se911_state = pinctrl_dev_get_drvdata(pctldev);

	return s2se911_state->ngroups;
}

static const char *s2se911_gpio_get_group_name(struct pinctrl_dev *pctldev, uint32_t selector)
{
	struct s2p_gpio_state *s2se911_state = pinctrl_dev_get_drvdata(pctldev);

	return s2se911_state->groups[selector].name;
}

static int s2se911_gpio_get_group_pins(struct pinctrl_dev *pctldev, uint32_t selector,
					const uint32_t **pins, uint32_t *num_pins)
{
	struct s2p_gpio_state *s2se911_state = pinctrl_dev_get_drvdata(pctldev);
	int ret = 0;

	ret = s2p_gpio_get_group_pins(s2se911_state, selector, pins, num_pins);

	return ret;
}

static void s2se911_gpio_pin_dbg_show(struct pinctrl_dev *pctldev,
					struct seq_file *s, uint32_t pin)
{
	struct s2p_gpio_state *s2se911_state = pinctrl_dev_get_drvdata(pctldev);
	struct s2p_gpio_pad *s2se911_pad = pctldev->desc->pins[pin].drv_data;

	s2p_gpio_pin_dbg_show(s2se911_state, s2se911_pad, s, pin);
}

static const struct pinctrl_ops s2se911_gpio_pinctrl_ops = {
	.get_groups_count	= s2se911_gpio_get_groups_count,
	.get_group_name		= s2se911_gpio_get_group_name,
	.get_group_pins		= s2se911_gpio_get_group_pins,
	.dt_node_to_map		= pinconf_generic_dt_node_to_map_pin,
	.dt_free_map		= pinctrl_utils_free_map,
	.pin_dbg_show		= s2se911_gpio_pin_dbg_show,
};

static int s2se911_gpio_pin_config_get(struct pinctrl_dev *pctldev,
					uint32_t pin, unsigned long *config)
{
	struct s2p_gpio_state *s2se911_state = pinctrl_dev_get_drvdata(pctldev);
	struct s2p_gpio_pad *s2se911_pad = pctldev->desc->pins[pin].drv_data;
	int ret = 0;

	ret = s2p_gpio_pin_config_get(s2se911_state, s2se911_pad, pin, config);

	return ret;
}

static int s2se911_gpio_pin_config_set(struct pinctrl_dev *pctldev, uint32_t pin,
					unsigned long *configs, uint32_t num_configs)
{
	struct s2p_gpio_state *s2se911_state = pinctrl_dev_get_drvdata(pctldev);
	struct s2p_gpio_pad *s2se911_pad = pctldev->desc->pins[pin].drv_data;
	int ret = 0;

	ret = s2p_gpio_pin_config_set(s2se911_state, s2se911_pad, pin, configs, num_configs);

	return ret;
}

static int s2se911_gpio_pin_config_group_get(struct pinctrl_dev *pctldev,
					uint32_t selector, unsigned long *config)
{
	const struct s2p_gpio_state *s2se911_state = pinctrl_dev_get_drvdata(pctldev);
	uint32_t npins = 0;
	uint32_t i = 0;
	int ret = 0;

	npins = s2se911_state->groups[selector].npins;

	for (i = 0; i < npins; i++) {
		ret = s2se911_gpio_pin_config_get(pctldev, i, config);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static int s2se911_gpio_pin_config_group_set(struct pinctrl_dev *pctldev, uint32_t selector,
					unsigned long *configs, uint32_t num_configs)
{
	const struct s2p_gpio_state *s2se911_state = pinctrl_dev_get_drvdata(pctldev);
	uint32_t npins = 0;
	uint32_t i = 0;
	int ret = 0;

	npins = s2se911_state->groups[selector].npins;

	for (i = 0; i < npins; i++) {
		ret = s2se911_gpio_pin_config_set(pctldev, i, configs, num_configs);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static void s2se911_gpio_pin_config_dbg_show(struct pinctrl_dev *pctldev,
					struct seq_file *s, uint32_t pin)
{
	struct s2p_gpio_state *s2se911_state = pinctrl_dev_get_drvdata(pctldev);
	struct s2p_gpio_pad *s2se911_pad = pctldev->desc->pins[pin].drv_data;

	s2p_gpio_pin_dbg_show(s2se911_state, s2se911_pad, s, pin);
}

static const struct pinconf_ops s2se911_gpio_pinconf_ops = {
	.is_generic			= false,
	.pin_config_get			= s2se911_gpio_pin_config_get,
	.pin_config_set			= s2se911_gpio_pin_config_set,
	.pin_config_group_get		= s2se911_gpio_pin_config_group_get,
	.pin_config_group_set		= s2se911_gpio_pin_config_group_set,
	.pin_config_dbg_show		= s2se911_gpio_pin_config_dbg_show,
};

static int s2se911_gpio_direction_input(struct gpio_chip *chip, uint32_t pin)
{
	const struct s2p_gpio_state *s2se911_state = gpiochip_get_data(chip);
	int ret = 0;

	ret = s2p_gpio_direction_input(s2se911_state, pin);

	return ret;
}

static int s2se911_gpio_direction_output(struct gpio_chip *chip, uint32_t pin, int val)
{
	const struct s2p_gpio_state *s2se911_state = gpiochip_get_data(chip);
	int ret = 0;

	ret = s2p_gpio_direction_output(s2se911_state, pin, val);

	return ret;
}

static int s2se911_gpio_get(struct gpio_chip *chip, uint32_t pin)
{
	struct s2p_gpio_state *s2se911_state = gpiochip_get_data(chip);
	struct s2p_gpio_pad *s2se911_pad = s2se911_state->ctrl->desc->pins[pin].drv_data;

	if (s2p_get_gpio_info(s2se911_state, pin) < 0)
		return -EINVAL;

	dev_info(s2se911_state->dev, "[SUB%d_PMIC] %s: pin%d: DAT(%s)\n",
		s2se911_state->sdev->device_type + 1, __func__, pin, s2se911_pad->output ? "high" : "low");

	return s2se911_pad->output;
}

static void s2se911_gpio_set(struct gpio_chip *chip, uint32_t pin, int value)
{
	const struct s2p_gpio_state *s2se911_state = gpiochip_get_data(chip);
	unsigned long config;

	dev_info(s2se911_state->dev, "[SUB%d_PMIC] %s: pin%d: Set DAT(%s_%#x)\n",
		s2se911_state->sdev->device_type + 1, __func__, pin, value ? "high" : "low", value);

	config = pinconf_to_config_packed(S2P_GPIO_CONF_OUTPUT, value);
	s2se911_gpio_pin_config_set(s2se911_state->ctrl, pin, &config, 1);
}

static int s2se911_gpio_of_xlate(struct gpio_chip *chip,
			      const struct of_phandle_args *gpio_desc,
			      u32 *flags)
{
	struct s2p_gpio_state *s2se911_state = gpiochip_get_data(chip);
	int ret = 0;

	ret = s2p_gpio_of_xlate(s2se911_state, chip, gpio_desc, flags);

	return ret;
}

static void s2se911_gpio_dbg_show(struct seq_file *s, struct gpio_chip *chip)
{
	const struct s2p_gpio_state *s2se911_state = gpiochip_get_data(chip);
	uint32_t i;

	for (i = 0; i < chip->ngpio; i++) {
		s2se911_gpio_pin_dbg_show(s2se911_state->ctrl, s, i);
		seq_puts(s, "\n");
	}
}

static int s2se911_gpio_set_config(struct gpio_chip *chip, uint32_t pin, unsigned long config)
{
	const struct s2p_gpio_state *s2se911_state = gpiochip_get_data(chip);

	dev_err(s2se911_state->dev, "[SUB%d_PMIC] %s: pin%d: config(%#lx)\n",
		s2se911_state->sdev->device_type + 1, __func__, pin, config);

	return s2se911_gpio_pin_config_set(s2se911_state->ctrl, pin, &config, 1);
}

static const struct gpio_chip s2se911_gpio_chip = {
	.direction_input	= s2se911_gpio_direction_input,
	.direction_output	= s2se911_gpio_direction_output,
	.get			= s2se911_gpio_get,
	.set			= s2se911_gpio_set,
	.request		= gpiochip_generic_request,
	.free			= gpiochip_generic_free,
	.of_xlate		= s2se911_gpio_of_xlate,
	.dbg_show		= s2se911_gpio_dbg_show,
	.set_config		= s2se911_gpio_set_config,
};

static struct pinctrl_desc s2se911_pinctrl_desc = {
	.pctlops		= &s2se911_gpio_pinctrl_ops,
	.pmxops			= &s2se911_gpio_pinmux_ops,
	.confops		= &s2se911_gpio_pinconf_ops,
	.owner			= THIS_MODULE,
	.pins			= s2se911_pin_desc,
	.npins			= ARRAY_SIZE(s2se911_pin_desc),
	.custom_params		= s2p_gpio_bindings,
	.num_custom_params	= ARRAY_SIZE(s2p_gpio_bindings),
#ifdef CONFIG_DEBUG_FS
	.custom_conf_items	= s2p_conf_items,
#endif
};

static int of_s2se911_gpio_parse_dt(struct s2se911_dev *iodev, struct s2p_gpio_state *state)
{
	struct device_node *s2se911_mfd_np = NULL, *s2se911_gpio_np = NULL;
	uint32_t pin_nums = 0, val = 0;
	int ret = 0;

	if (!iodev->dev->of_node) {
		pr_err("%s: error\n", __func__);
		return -ENODEV;
	}

	s2se911_mfd_np = iodev->dev->of_node;

	s2se911_gpio_np = of_find_node_by_name(s2se911_mfd_np, "s2se911-gpio");
	if (!s2se911_gpio_np) {
		pr_err("%s: could not find current_node\n", __func__);
		return -ENODEV;
	}

	state->dev->of_node = s2se911_gpio_np;

	ret = of_property_read_u32(s2se911_gpio_np, "samsung,npins", &pin_nums);
	if (ret)
		state->npins = ARRAY_SIZE(s2se911_pin_desc);
	else
		state->npins = pin_nums;

	ret = of_property_read_u32(s2se911_gpio_np, "gpio45_lvl_sel", &val);
	if (ret)
		iodev->gpio45_lvl_sel = 0;
	else
		iodev->gpio45_lvl_sel = val;

	return 0;
}

static int s2se911_set_gpio_pad(const struct s2p_gpio_state *state)
{
	struct s2p_gpio_pad *s2se911_pad = NULL, *s2se911_pads = NULL;
	int i = 0, ret = 0;

	s2se911_pads = devm_kcalloc(state->dev, state->npins, sizeof(*s2se911_pads), GFP_KERNEL);
	if (!s2se911_pads)
		return -ENOMEM;

	for (i = 0; i < state->npins; i++) {
		s2se911_pad = &s2se911_pads[i];
		s2se911_pin_desc[i].drv_data = s2se911_pad;

		ret = s2p_gpio_populate(state, s2se911_pad, i);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static void s2se911_set_gpio_chip(struct s2p_gpio_state *s2se911_state)
{
	s2se911_state->chip = s2se911_gpio_chip;
	s2se911_state->chip.parent = s2se911_state->dev;
	s2se911_state->chip.base = -1;
	s2se911_state->chip.ngpio = s2se911_state->npins;
	s2se911_state->chip.label = dev_name(s2se911_state->dev);
	s2se911_state->chip.of_gpio_n_cells = 2;
	s2se911_state->chip.can_sleep = false;
}

static int s2se911_set_vgpio(struct s2p_gpio_state *state)
{
	/* VGPIO setting by AP */
	state->sysreg_base = ioremap(SYSREG_ALIVE + VGPIO_TX_R11, 0x24);
	if (!state->sysreg_base) {
		dev_err(state->dev, "sysreg vgpio ioremap failed\n");
		return -ENOMEM;
	}

	return 0;
}

static int s2se911_set_gpio45_lvl_sel(const struct s2se911_dev *iodev, struct s2p_gpio_state *state)
{
	int ret = 0;

	if (iodev->gpio45_lvl_sel) {
		ret = s2p_update_reg(state->sdev, iodev->pm3, S2SE911_PM3_IO1P2_CTRL,
			S2SE911_GPIO_LVL_SEL_MASK, S2SE911_GPIO_LVL_SEL_MASK);
		if (ret < 0) {
			dev_err(state->dev, "[SUB%d_PMIC] %s: set gpio45 lvl 1.2V config fail\n",
				state->sdev->device_type + 1, __func__);
			return -EINVAL;
		}
		dev_info(state->dev, "[SUB%d_PMIC] %s: set gpio45 lvl 1.2V config (%d)\n",
			state->sdev->device_type + 1, __func__, iodev->gpio45_lvl_sel);
	}
	return 0;
}

static void s2se911_set_gpio_state(struct s2p_gpio_state *state)
{
	state->groups = s2se911_pin_groups;
	state->ngroups = S2SE911_GROUP_SIZE;
	state->gpio_groups = s2se911_gpio_groups;
	state->gpio_groups_size = ARRAY_SIZE(s2se911_gpio_groups);
	state->pin_desc = s2se911_pin_desc;
	state->strengths = s2se911_strengths;

	state->bit->gpio_oen_shift = S2SE911_GPIO_OEN_SHIFT;
	state->bit->gpio_oen_mask = S2SE911_GPIO_OEN_MASK;
	state->bit->gpio_out_shift = S2SE911_GPIO_OUT_SHIFT;
	state->bit->gpio_out_mask = S2SE911_GPIO_OUT_MASK;
	state->bit->gpio_pull_shift = S2SE911_GPIO_PULL_SHIFT;
	state->bit->gpio_pull_mask = S2SE911_GPIO_PULL_MASK;
	state->bit->gpio_drv_str_shift = S2SE911_GPIO_DRV_STR_SHIFT;
	state->bit->gpio_drv_str_mask = S2SE911_GPIO_DRV_STR_MASK;
	state->bit->gpio_mode_digit_input = S2SE911_GPIO_MODE_DIGITAL_INPUT;
	state->bit->gpio_mode_digit_output = S2SE911_GPIO_MODE_DIGITAL_OUTPUT;
	state->bit->gpio_pull_disable = S2SE911_GPIO_PULL_DISABLE;
	state->bit->gpio_pull_down = S2SE911_GPIO_PULL_DOWN;
	state->bit->gpio_pull_up = S2SE911_GPIO_PULL_UP;

	state->get_gpio_output_func = s2se911_get_gpio_output;
	state->get_set_reg_func = s2se911_get_set_reg;
	state->get_status_reg_func = s2se911_get_status_reg;
	state->set_gpio_conf_output_func = s2se911_set_gpio_conf_output;
}

static int s2se911_gpio_probe(struct platform_device *pdev)
{
	struct s2se911_dev *iodev = dev_get_drvdata(pdev->dev.parent);
	struct s2p_gpio_state *s2se911_state = NULL;
	int ret = 0;
	const int dev_type = iodev->sdev->device_type;

	dev_info(&pdev->dev, "[SUB%d_PMIC] %s: start\n", dev_type + 1, __func__);

	s2se911_state = devm_kzalloc(&pdev->dev, sizeof(*s2se911_state), GFP_KERNEL);
	if (!s2se911_state)
		return -ENOMEM;

	s2se911_state->bit = devm_kzalloc(&pdev->dev, sizeof(*s2se911_state->bit), GFP_KERNEL);
	if (!s2se911_state->bit)
		return -ENOMEM;

	platform_set_drvdata(pdev, s2se911_state);
	s2se911_state->dev = &pdev->dev;
	s2se911_state->sdev = iodev->sdev;
	s2se911_state->gpio_addr = iodev->gpio;
	s2se911_pinctrl_desc.name = dev_name(&pdev->dev);

	s2se911_set_gpio_state(s2se911_state);

	ret = of_s2se911_gpio_parse_dt(iodev, s2se911_state);
	if (ret < 0)
		return ret;

	ret = s2se911_set_gpio45_lvl_sel(iodev, s2se911_state);
	if (ret < 0)
		return ret;

	ret = s2se911_set_vgpio(s2se911_state);
	if (ret < 0)
		return ret;

	ret = s2se911_set_gpio_pad(s2se911_state);
	if (ret < 0)
		return ret;

	s2se911_set_gpio_chip(s2se911_state);

	s2se911_state->ctrl = devm_pinctrl_register(s2se911_state->dev, &s2se911_pinctrl_desc, s2se911_state);
	if (IS_ERR(s2se911_state->ctrl))
		return PTR_ERR(s2se911_state->ctrl);

	ret = devm_gpiochip_add_data(s2se911_state->dev, &s2se911_state->chip, s2se911_state);
	if (ret) {
		dev_err(s2se911_state->dev, "can't add gpio chip\n");
		return ret;
	}
	/*
	 * For DeviceTree-supported systems, the gpio core checks the
	 * pinctrl's device node for the "gpio-ranges" property.
	 * If it is present, it takes care of adding the pin ranges
	 * for the driver. In this case the driver can skip ahead.
	 *
	 * In order to remain compatible with older, existing DeviceTree
	 * files which don't set the "gpio-ranges" property or systems that
	 * utilize ACPI the driver has to call gpiochip_add_pin_range().
	 */
	if (!of_property_read_bool(s2se911_state->dev->of_node, "gpio-ranges")) {
		ret = gpiochip_add_pin_range(&s2se911_state->chip, dev_name(s2se911_state->dev), 0, 0,
					     s2se911_state->npins);
		if (ret) {
			dev_err(&pdev->dev, "failed to add pin range\n");
			return ret;
		}
	}

	dev_info(&pdev->dev, "[SUB%d_PMIC] %s: end\n", dev_type + 1, __func__);

	return 0;
}

static const struct platform_device_id s2se911_gpio_id[] = {
	{ "s2se911-1-gpio", TYPE_S2SE911_1},
	{ "s2se911-2-gpio", TYPE_S2SE911_2},
	{ "s2se911-3-gpio", TYPE_S2SE911_3},
	{ "s2se911-4-gpio", TYPE_S2SE911_4},
	{ "s2se911-5-gpio", TYPE_S2SE911_5},
	{ },
};
MODULE_DEVICE_TABLE(platform, s2se911_gpio_id);

static struct platform_driver s2se911_gpio_driver = {
	.driver = {
		   .name = "s2se911-gpio",
		   .owner = THIS_MODULE,
	},
	.probe	= s2se911_gpio_probe,
	.id_table = s2se911_gpio_id,
};

module_platform_driver(s2se911_gpio_driver);

MODULE_AUTHOR("Samsung Electronics");
MODULE_DESCRIPTION("Samsung SPMI PMIC GPIO pin control driver");
MODULE_ALIAS("platform:samsung-spmi-gpio");
MODULE_LICENSE("GPL v2");

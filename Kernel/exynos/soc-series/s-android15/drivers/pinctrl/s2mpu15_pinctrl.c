// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2024 Samsung Electronics Co., Ltd
 */

#include <linux/pmic/s2p_pinctrl.h>
#include <linux/pmic/s2mpu15-mfd.h>

enum s2mpu15_gpio_io_param {
	S2MPU15_GPIO_MODE_DIGITAL_INPUT = (0 << S2MPU15_GPIO_OEN_SHIFT),
	S2MPU15_GPIO_MODE_DIGITAL_OUTPUT = (1 << S2MPU15_GPIO_OEN_SHIFT),
};

enum s2mpu15_gpio_pull_param {
	S2MPU15_GPIO_PULL_DISABLE = (0 << S2MPU15_GPIO_PULL_SHIFT),
	S2MPU15_GPIO_PULL_DOWN = (1 << S2MPU15_GPIO_PULL_SHIFT),
	S2MPU15_GPIO_PULL_UP = (2 << S2MPU15_GPIO_PULL_SHIFT),
};

static const char *const s2mpu15_gpio_groups[] = {
	"gpio_m0", "gpio_m1",
};

static struct pinctrl_pin_desc s2mpu15_pin_desc[] = {
	PINCTRL_PIN(0, "gpio_m0"),
	PINCTRL_PIN(1, "gpio_m1"),
};

static const char *const s2mpu15_strengths[] = {
	"1mA", "2mA", "3mA", "4mA", "5mA", "6mA", "not-use", "not-use"
};

static const unsigned int s2mpu15_gpio_pins[] = {0, 1};

#define S2MPU15_GROUP_SIZE	1

static const struct pingroup s2mpu15_pin_groups[] = {
	PINCTRL_PINGROUP("PMIC_GPIO", s2mpu15_gpio_pins, ARRAY_SIZE(s2mpu15_gpio_pins)),
};

static uint32_t s2mpu15_get_gpio_output(const uint32_t pin, const int val)
{
	return (val >> pin) & 0x1;
}

static uint8_t s2mpu15_get_set_reg(const uint32_t pin)
{
	return S2MPU15_PM1_GPIO0_SET + pin;
}

static uint8_t s2mpu15_get_status_reg(const uint32_t pin)
{
	return S2MPU15_PM1_GPIO_STATUS;
}

static int s2mpu15_set_gpio_conf_output(const struct s2p_gpio_state *state, uint32_t arg,
		 			 uint32_t pin, int val_set)
{
	arg = (arg << state->bit->gpio_out_shift) & state->bit->gpio_out_mask;
	val_set = (val_set & ~state->bit->gpio_out_mask) | arg;

	return val_set;
}

static int s2mpu15_gpio_get_functions_count(struct pinctrl_dev *pctldev)
{
	return ARRAY_SIZE(s2p_gpio_functions);
}

static const char *s2mpu15_gpio_get_function_name(struct pinctrl_dev *pctldev,
					       uint32_t function)
{
	return s2p_gpio_functions[function];
}

static int s2mpu15_gpio_get_function_groups(struct pinctrl_dev *pctldev,
					 uint32_t selector,
					 const char *const **groups,
					 uint32_t *const num_qgroups)
{
	const struct s2p_gpio_state *s2mpu15_state = pinctrl_dev_get_drvdata(pctldev);
	int ret = 0;

	ret = s2p_gpio_get_function_groups(s2mpu15_state, groups, num_qgroups);

	return ret;
}

static int s2mpu15_gpio_set_mux(struct pinctrl_dev *pctldev, uint32_t func_selector,
				uint32_t group_selector)
{
	const struct s2p_gpio_state *s2mpu15_state = pinctrl_dev_get_drvdata(pctldev);
	int ret = 0;

	ret = s2p_gpio_set_mux(s2mpu15_state, pctldev, func_selector, group_selector);

	return ret;
}

static const struct pinmux_ops s2mpu15_gpio_pinmux_ops = {
	.get_functions_count	= s2mpu15_gpio_get_functions_count,
	.get_function_name	= s2mpu15_gpio_get_function_name,
	.get_function_groups	= s2mpu15_gpio_get_function_groups,
	.set_mux		= s2mpu15_gpio_set_mux,
};

static int s2mpu15_gpio_get_groups_count(struct pinctrl_dev *pctldev)
{
	struct s2p_gpio_state *s2mpu15_state = pinctrl_dev_get_drvdata(pctldev);

	return s2mpu15_state->ngroups;
}

static const char *s2mpu15_gpio_get_group_name(struct pinctrl_dev *pctldev,
					uint32_t selector)
{
	struct s2p_gpio_state *s2mpu15_state = pinctrl_dev_get_drvdata(pctldev);

	return s2mpu15_state->groups[selector].name;
}

static int s2mpu15_gpio_get_group_pins(struct pinctrl_dev *pctldev, uint32_t selector,
					const uint32_t **pins, uint32_t *num_pins)
{
	struct s2p_gpio_state *s2mpu15_state = pinctrl_dev_get_drvdata(pctldev);
	int ret = 0;

	ret = s2p_gpio_get_group_pins(s2mpu15_state, selector, pins, num_pins);

	return ret;
}

static void s2mpu15_gpio_pin_dbg_show(struct pinctrl_dev *pctldev,
					struct seq_file *s, uint32_t pin)
{
	struct s2p_gpio_state *s2mpu15_state = pinctrl_dev_get_drvdata(pctldev);
	struct s2p_gpio_pad *s2mpu15_pad = pctldev->desc->pins[pin].drv_data;

	s2p_gpio_pin_dbg_show(s2mpu15_state, s2mpu15_pad, s, pin);
}

static const struct pinctrl_ops s2mpu15_gpio_pinctrl_ops = {
	.get_groups_count	= s2mpu15_gpio_get_groups_count,
	.get_group_name		= s2mpu15_gpio_get_group_name,
	.get_group_pins		= s2mpu15_gpio_get_group_pins,
	.dt_node_to_map		= pinconf_generic_dt_node_to_map_pin,
	.dt_free_map		= pinctrl_utils_free_map,
	.pin_dbg_show		= s2mpu15_gpio_pin_dbg_show,
};

static int s2mpu15_gpio_pin_config_get(struct pinctrl_dev *pctldev,
					uint32_t pin, unsigned long *config)
{
	struct s2p_gpio_state *s2mpu15_state = pinctrl_dev_get_drvdata(pctldev);
	struct s2p_gpio_pad *s2mpu15_pad = pctldev->desc->pins[pin].drv_data;
	int ret = 0;

	ret = s2p_gpio_pin_config_get(s2mpu15_state, s2mpu15_pad, pin, config);

	return ret;
}

static int s2mpu15_gpio_pin_config_set(struct pinctrl_dev *pctldev, uint32_t pin,
					unsigned long *configs, uint32_t num_configs)
{
	struct s2p_gpio_state *s2mpu15_state = pinctrl_dev_get_drvdata(pctldev);
	struct s2p_gpio_pad *s2mpu15_pad = pctldev->desc->pins[pin].drv_data;
	int ret = 0;

	ret = s2p_gpio_pin_config_set(s2mpu15_state, s2mpu15_pad, pin, configs, num_configs);

	return ret;
}

static int s2mpu15_gpio_pin_config_group_get(struct pinctrl_dev *pctldev,
					uint32_t selector, unsigned long *config)
{
	const struct s2p_gpio_state *s2mpu15_state = pinctrl_dev_get_drvdata(pctldev);
	uint32_t npins = 0;
	uint32_t i = 0;
	int ret = 0;

	npins = s2mpu15_state->groups[selector].npins;

	for (i = 0; i < npins; i++) {
		ret = s2mpu15_gpio_pin_config_get(pctldev, i, config);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static int s2mpu15_gpio_pin_config_group_set(struct pinctrl_dev *pctldev, uint32_t selector,
					unsigned long *configs, uint32_t num_configs)
{
	const struct s2p_gpio_state *s2mpu15_state = pinctrl_dev_get_drvdata(pctldev);
	uint32_t npins = 0;
	uint32_t i = 0;
	int ret = 0;

	npins = s2mpu15_state->groups[selector].npins;

	for (i = 0; i < npins; i++) {
		ret = s2mpu15_gpio_pin_config_set(pctldev, i, configs, num_configs);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static void s2mpu15_gpio_pin_config_dbg_show(struct pinctrl_dev *pctldev,
					struct seq_file *s, uint32_t pin)
{
	struct s2p_gpio_state *s2mpu15_state = pinctrl_dev_get_drvdata(pctldev);
	struct s2p_gpio_pad *s2mpu15_pad = pctldev->desc->pins[pin].drv_data;

	s2p_gpio_pin_dbg_show(s2mpu15_state, s2mpu15_pad, s, pin);
}

static const struct pinconf_ops s2mpu15_gpio_pinconf_ops = {
	.is_generic			= false,
	.pin_config_get			= s2mpu15_gpio_pin_config_get,
	.pin_config_set			= s2mpu15_gpio_pin_config_set,
	.pin_config_group_get		= s2mpu15_gpio_pin_config_group_get,
	.pin_config_group_set		= s2mpu15_gpio_pin_config_group_set,
	.pin_config_dbg_show		= s2mpu15_gpio_pin_config_dbg_show,
};

static int s2mpu15_gpio_direction_input(struct gpio_chip *chip, uint32_t pin)
{
	const struct s2p_gpio_state *s2mpu15_state = gpiochip_get_data(chip);
	int ret = 0;

	ret = s2p_gpio_direction_input(s2mpu15_state, pin);

	return ret;
}

static int s2mpu15_gpio_direction_output(struct gpio_chip *chip, uint32_t pin, int val)
{
	const struct s2p_gpio_state *s2mpu15_state = gpiochip_get_data(chip);
	int ret = 0;

	ret = s2p_gpio_direction_output(s2mpu15_state, pin, val);

	return ret;
}

static int s2mpu15_gpio_get(struct gpio_chip *chip, uint32_t pin)
{
	struct s2p_gpio_state *s2mpu15_state = gpiochip_get_data(chip);
	struct s2p_gpio_pad *s2mpu15_pad = s2mpu15_state->ctrl->desc->pins[pin].drv_data;

	if (s2p_get_gpio_info(s2mpu15_state, pin) < 0)
		return -EINVAL;

	dev_info(s2mpu15_state->dev, "[MAIN_PMIC] %s: pin%d: DAT(%s)\n",
		__func__, pin, s2mpu15_pad->output ? "high" : "low");

	return s2mpu15_pad->output;
}

static void s2mpu15_gpio_set(struct gpio_chip *chip, uint32_t pin, int value)
{
	const struct s2p_gpio_state *s2mpu15_state = gpiochip_get_data(chip);
	unsigned long config;

	dev_info(s2mpu15_state->dev, "[MAIN_PMIC] %s: pin%d: Set DAT(%s_%#x)\n",
		__func__, pin, value ? "high" : "low", value);

	config = pinconf_to_config_packed(S2P_GPIO_CONF_OUTPUT, value);
	s2mpu15_gpio_pin_config_set(s2mpu15_state->ctrl, pin, &config, 1);
}

static int s2mpu15_gpio_of_xlate(struct gpio_chip *chip,
			      const struct of_phandle_args *gpio_desc,
			      u32 *flags)
{
	struct s2p_gpio_state *s2mpu15_state = gpiochip_get_data(chip);
	int ret = 0;

	ret = s2p_gpio_of_xlate(s2mpu15_state, chip, gpio_desc, flags);

	return ret;
}

static void s2mpu15_gpio_dbg_show(struct seq_file *s, struct gpio_chip *chip)
{
	const struct s2p_gpio_state *s2mpu15_state = gpiochip_get_data(chip);
	uint32_t i;

	for (i = 0; i < chip->ngpio; i++) {
		s2mpu15_gpio_pin_dbg_show(s2mpu15_state->ctrl, s, i);
		seq_puts(s, "\n");
	}
}

static int s2mpu15_gpio_set_config(struct gpio_chip *chip, uint32_t pin, unsigned long config)
{
	const struct s2p_gpio_state *s2mpu15_state = gpiochip_get_data(chip);

	dev_err(s2mpu15_state->dev, "[MAIN_PMIC] %s: pin%d: config(%#lx)\n",	__func__, pin, config);

	return s2mpu15_gpio_pin_config_set(s2mpu15_state->ctrl, pin, &config, 1);
}

static const struct gpio_chip s2mpu15_gpio_chip = {
	.direction_input	= s2mpu15_gpio_direction_input,
	.direction_output	= s2mpu15_gpio_direction_output,
	.get			= s2mpu15_gpio_get,
	.set			= s2mpu15_gpio_set,
	.request		= gpiochip_generic_request,
	.free			= gpiochip_generic_free,
	.of_xlate		= s2mpu15_gpio_of_xlate,
	.dbg_show		= s2mpu15_gpio_dbg_show,
	.set_config		= s2mpu15_gpio_set_config,
};

static struct pinctrl_desc s2mpu15_pinctrl_desc = {
	.pctlops		= &s2mpu15_gpio_pinctrl_ops,
	.pmxops			= &s2mpu15_gpio_pinmux_ops,
	.confops		= &s2mpu15_gpio_pinconf_ops,
	.owner			= THIS_MODULE,
	.pins			= s2mpu15_pin_desc,
	.npins			= ARRAY_SIZE(s2mpu15_pin_desc),
	.custom_params		= s2p_gpio_bindings,
	.num_custom_params	= ARRAY_SIZE(s2p_gpio_bindings),
#ifdef CONFIG_DEBUG_FS
	.custom_conf_items	= s2p_conf_items,
#endif
};

static int of_s2mpu15_gpio_parse_dt(const struct s2mpu15_dev *iodev, struct s2p_gpio_state *state)
{
	struct device_node *s2mpu15_mfd_np = NULL, *s2mpu15_gpio_np = NULL;
	uint32_t pin_nums = 0;
	int ret = 0;

	if (!iodev->dev->of_node) {
		pr_err("%s: error\n", __func__);
		return -ENODEV;
	}

	s2mpu15_mfd_np = iodev->dev->of_node;

	s2mpu15_gpio_np = of_find_node_by_name(s2mpu15_mfd_np, "s2mpu15-gpio");
	if (!s2mpu15_gpio_np) {
		pr_err("%s: could not find current_node\n", __func__);
		return -ENODEV;
	}

	state->dev->of_node = s2mpu15_gpio_np;

	ret = of_property_read_u32(s2mpu15_gpio_np, "samsung,npins", &pin_nums);
	if (ret)
		state->npins = ARRAY_SIZE(s2mpu15_pin_desc);
	else
		state->npins = pin_nums;

	return 0;
}

static int s2mpu15_set_gpio_pad(const struct s2p_gpio_state *state)
{
	struct s2p_gpio_pad *s2mpu15_pad = NULL, *s2mpu15_pads = NULL;
	int i = 0, ret = 0;

	s2mpu15_pads = devm_kcalloc(state->dev, state->npins, sizeof(*s2mpu15_pads), GFP_KERNEL);
	if (!s2mpu15_pads)
		return -ENOMEM;

	for (i = 0; i < state->npins; i++) {
		s2mpu15_pad = &s2mpu15_pads[i];
		s2mpu15_pin_desc[i].drv_data = s2mpu15_pad;

		ret = s2p_gpio_populate(state, s2mpu15_pad, i);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static void s2mpu15_set_gpio_chip(struct s2p_gpio_state *s2mpu15_state)
{
	s2mpu15_state->chip = s2mpu15_gpio_chip;
	s2mpu15_state->chip.parent = s2mpu15_state->dev;
	s2mpu15_state->chip.base = -1;
	s2mpu15_state->chip.ngpio = s2mpu15_state->npins;
	s2mpu15_state->chip.label = dev_name(s2mpu15_state->dev);
	s2mpu15_state->chip.of_gpio_n_cells = 2;
	s2mpu15_state->chip.can_sleep = false;
}

static void s2mpu15_set_gpio_state(struct s2p_gpio_state *state)
{
	state->groups = s2mpu15_pin_groups;
	state->ngroups = S2MPU15_GROUP_SIZE;
	state->gpio_groups = s2mpu15_gpio_groups;
	state->gpio_groups_size = ARRAY_SIZE(s2mpu15_gpio_groups);
	state->pin_desc = s2mpu15_pin_desc;
	state->strengths = s2mpu15_strengths;

	state->bit->gpio_oen_shift = S2MPU15_GPIO_OEN_SHIFT;
	state->bit->gpio_oen_mask = S2MPU15_GPIO_OEN_MASK;
	state->bit->gpio_out_shift = S2MPU15_GPIO_OUT_SHIFT;
	state->bit->gpio_out_mask = S2MPU15_GPIO_OUT_MASK;
	state->bit->gpio_pull_shift = S2MPU15_GPIO_PULL_SHIFT;
	state->bit->gpio_pull_mask = S2MPU15_GPIO_PULL_MASK;
	state->bit->gpio_drv_str_shift = S2MPU15_GPIO_DRV_STR_SHIFT;
	state->bit->gpio_drv_str_mask = S2MPU15_GPIO_DRV_STR_MASK;
	state->bit->gpio_mode_digit_input = S2MPU15_GPIO_MODE_DIGITAL_INPUT;
	state->bit->gpio_mode_digit_output = S2MPU15_GPIO_MODE_DIGITAL_OUTPUT;
	state->bit->gpio_pull_disable = S2MPU15_GPIO_PULL_DISABLE;
	state->bit->gpio_pull_down = S2MPU15_GPIO_PULL_DOWN;
	state->bit->gpio_pull_up = S2MPU15_GPIO_PULL_UP;

	state->get_gpio_output_func = s2mpu15_get_gpio_output;
	state->get_set_reg_func = s2mpu15_get_set_reg;
	state->get_status_reg_func = s2mpu15_get_status_reg;
	state->set_gpio_conf_output_func = s2mpu15_set_gpio_conf_output;
}

static int s2mpu15_gpio_probe(struct platform_device *pdev)
{
	const struct s2mpu15_dev *iodev = dev_get_drvdata(pdev->dev.parent);
	struct s2p_gpio_state *s2mpu15_state = NULL;
	int ret = 0;

	dev_info(&pdev->dev, "[MAIN_PMIC] %s: start\n", __func__);

	s2mpu15_state = devm_kzalloc(&pdev->dev, sizeof(*s2mpu15_state), GFP_KERNEL);
	if (!s2mpu15_state)
		return -ENOMEM;

	s2mpu15_state->bit = devm_kzalloc(&pdev->dev, sizeof(*s2mpu15_state->bit), GFP_KERNEL);
	if (!s2mpu15_state->bit)
		return -ENOMEM;

	platform_set_drvdata(pdev, s2mpu15_state);
	s2mpu15_state->dev = &pdev->dev;
	s2mpu15_state->sdev = iodev->sdev;
	s2mpu15_state->gpio_addr = iodev->pmic1;  //TODO: check the gpio base addr
	s2mpu15_pinctrl_desc.name = dev_name(&pdev->dev);

	s2mpu15_set_gpio_state(s2mpu15_state);

	ret = of_s2mpu15_gpio_parse_dt(iodev, s2mpu15_state);
	if (ret < 0)
		return ret;

	ret = s2mpu15_set_gpio_pad(s2mpu15_state);
	if (ret < 0)
		return ret;

	s2mpu15_set_gpio_chip(s2mpu15_state);

	s2mpu15_state->ctrl = devm_pinctrl_register(s2mpu15_state->dev, &s2mpu15_pinctrl_desc, s2mpu15_state);
	if (IS_ERR(s2mpu15_state->ctrl))
		return PTR_ERR(s2mpu15_state->ctrl);

	ret = devm_gpiochip_add_data(s2mpu15_state->dev, &s2mpu15_state->chip, s2mpu15_state);
	if (ret) {
		dev_err(s2mpu15_state->dev, "can't add gpio chip\n");
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
	if (!of_property_read_bool(s2mpu15_state->dev->of_node, "gpio-ranges")) {
		ret = gpiochip_add_pin_range(&s2mpu15_state->chip, dev_name(s2mpu15_state->dev), 0, 0,
					     s2mpu15_state->npins);
		if (ret) {
			dev_err(&pdev->dev, "failed to add pin range\n");
			return ret;
		}
	}

	dev_info(&pdev->dev, "[MAIN_PMIC] %s: end\n", __func__);

	return 0;
}

static const struct platform_device_id s2mpu15_gpio_id[] = {
	{ "s2mpu15-gpio", TYPE_S2MPU15},
	{ },
};
MODULE_DEVICE_TABLE(platform, s2mpu15_gpio_id);

static struct platform_driver s2mpu15_gpio_driver = {
	.driver = {
		   .name = "s2mpu15-gpio",
		   .owner = THIS_MODULE,
	},
	.probe	= s2mpu15_gpio_probe,
	.id_table = s2mpu15_gpio_id,
};

module_platform_driver(s2mpu15_gpio_driver);

MODULE_AUTHOR("Samsung Electronics");
MODULE_DESCRIPTION("Samsung SPMI PMIC GPIO pin control driver");
MODULE_ALIAS("platform:samsung-spmi-gpio");
MODULE_LICENSE("GPL v2");

/*
 * /sound/soc/samsung/exynos/codecs/s5m3500x.c
 *
 * ALSA SoC Audio Layer - Samsung Codec Driver
 *
 * Copyright (C) 2024 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/io.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/initval.h>
#include <sound/tlv.h>

#include <linux/regulator/consumer.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_gpio.h>

#ifdef CONFIG_DEBUG_FS
#include <linux/debugfs.h>
#endif

#include "s5m3500x.h"
#include "s5m3500x-jack.h"
#include "s5m3500x-register.h"
#include "s5m3500x-regmap.h"

#define S5M3500X_CODEC_SW_VER	1
#define S5M3500X_SLAVE_ADDR		0x14

#define DAC_DVOL_MAXNUM				0x30
#define DAC_DVOL_DEFAULT			0x54
#define DAC_DVOL_MINNUM				0xEA

#define ADC_DVOL_MAXNUM				0xE5
#define CTVOL_BST_MAXNUM			0x05
#define AMIC_MAX_DELAY				1000

#define ADC_INP_SEL_ZERO			2

#define AMIC_ZERO						0
#define AMIC_MONO						1
#define AMIC_STEREO					2


/* Codec Function */
int s5m3500x_codec_enable(struct device *dev, bool force);
int s5m3500x_codec_disable(struct device *dev, bool force);
static void s5m3500x_codec_power_enable(struct s5m3500x_priv *s5m3500x, bool enable);
static void s5m3500x_register_initialize(struct s5m3500x_priv *s5m3500x);

void s5m3500x_adc_digital_mute(struct s5m3500x_priv *s5m3500x, unsigned int channel, bool on);
static void s5m3500x_dac_soft_mute(struct s5m3500x_priv *s5m3500x, unsigned int channel, bool on);
static void s5m3500x_adc_mute_work(struct work_struct *work);

/* DebugFS for register read / write */
#ifdef CONFIG_DEBUG_FS
static void s5m3500x_debug_init(struct s5m3500x_priv *s5m3500x);
static void s5m3500x_debug_remove(struct s5m3500x_priv *s5m3500x);
#endif

void s5m3500x_usleep(unsigned int u_sec)
{
	usleep_range(u_sec, u_sec + 10);
}
EXPORT_SYMBOL_GPL(s5m3500x_usleep);

bool is_cache_bypass_enabled(struct s5m3500x_priv *s5m3500x)
{
	return (s5m3500x->cache_bypass > 0) ? true : false;
}

bool is_cache_only_enabled(struct s5m3500x_priv *s5m3500x)
{
	return (s5m3500x->cache_only > 0) ? true : false;
}

void s5m3500x_add_device_status(struct s5m3500x_priv *s5m3500x, unsigned int status)
{
	s5m3500x->status |= status;
}

void s5m3500x_remove_device_status(struct s5m3500x_priv *s5m3500x, unsigned int status)
{
	s5m3500x->status &= ~status;
}

bool s5m3500x_check_device_status(struct s5m3500x_priv *s5m3500x, unsigned int status)
{
	unsigned int compare_device = s5m3500x->status & status;

	if (compare_device)
		return true;
	else
		return false;
}
EXPORT_SYMBOL_GPL(s5m3500x_check_device_status);

/*
 * Return Value
 * True: If the register value cannot be cached, hence we have to read from the
 * hardware directly.
 * False: If the register value can be read from cache.
 */
static bool s5m3500x_volatile_register(struct device *dev, unsigned int reg)
{
	/*
	 * For all the registers for which we want to restore the value during
	 * regcache_sync operation, we need to return true here. For registers
	 * whose value need not be cached and restored should return false here.
	 *
	 * For the time being, let us cache the value of all registers other
	 * than the IRQ pending and IRQ status registers.
	 */
	switch (reg) {
	/* Digital Block */
	case S5M3500X_001_IRQ1 ... S5M3500X_006_IRQ6:
	case S5M3500X_00F_LV_IRQ:
	case S5M3500X_080_ANA_INTR ... S5M3500X_088_OCP_INTR_NEG:
	case S5M3500X_08F_NOISE_INTR_NEG:
		return true;
	default:
		return false;
	}
}

/*
 * Return Value
 * True: If the register value can be read
 * False: If the register cannot be read
 */
static bool s5m3500x_readable_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	/* Digital Block */
		/* Interrupt Register */
	case S5M3500X_001_IRQ1 ... S5M3500X_006_IRQ6:
	case S5M3500X_008_IRQ1M ... S5M3500X_00D_IRQ6M:
	case S5M3500X_00F_LV_IRQ:
		/* Clock & Reset */
	case S5M3500X_010_CLKGATE0 ... S5M3500X_01F_COM_OTP_REGADD1:
		/* Digital Audio Interface */
	case S5M3500X_020_IF_FORM1 ... S5M3500X_029_IF_FORM6:
	case S5M3500X_02F_GPIO_ST:
		/* Recording Path Digital */
	case S5M3500X_030_ADC1 ... S5M3500X_031_ADC2:
	case S5M3500X_033_ADC3 ... S5M3500X_035_AD_VOLR:
	case S5M3500X_037_AD_HPF ... S5M3500X_03E_AD_OFFSETR:
		/* Playback Path Digital */
	case S5M3500X_040_PLAY_MODE ... S5M3500X_042_PLAY_VOLR:
	case S5M3500X_044_PLAY_MIX0 ... S5M3500X_04F_HPR_OFFMSK2:
		/* Adaptive Volume Control */
	case S5M3500X_050_AVC1 ... S5M3500X_07F_AVC46:
		/* IRQ for LV */
	case S5M3500X_080_ANA_INTR ... S5M3500X_08F_NOISE_INTR_NEG:
		/* Digital DSM COntrol */
	case S5M3500X_093_DSM_CON1 ... S5M3500X_09E_GPIO123_CON:
		/* Auto Sequence Control */
	case S5M3500X_0A0_AMU_CTRL1 ... S5M3500X_0AE_RESERVED:
	case S5M3500X_0B0_TEST_CTRL1 ... S5M3500X_0BA_TEST_CTRL11:
		/* Headphone Management Unit Control */
	case S5M3500X_0C0_ACTR_JD1 ... S5M3500X_0CD_RESERVED:
	case S5M3500X_0D1_RESERVED ... S5M3500X_0DD_DCTR_DBNC6:
	case S5M3500X_0DF_RESERVED:
	case S5M3500X_0E0_DCTR_FSM1 ... S5M3500X_0E7_RESERVED:
	case S5M3500X_0F0_STATUS1 ... S5M3500X_0F5_STATUS6:
	case S5M3500X_0F9_STATUS10 ... S5M3500X_0FB_STATUS12:
	case S5M3500X_0FD_ACTR_GP ... S5M3500X_0FF_DCTR_GP2:
		return true;
	/* Analog Block */
		/* Analog Clock & Reference & CP Control */
	case S5M3500X_100_CTRL_REF1 ... S5M3500X_101_CTRL_REF2:
	case S5M3500X_105_AD_CLK0 ... S5M3500X_10F_DA_CP7:
		/* Analog Recording Path Control */
	case S5M3500X_110_CTRL_MIC1 ... S5M3500X_121_CTRL_MIC18:
		/* Analog Playback Path Control */
	case S5M3500X_130_CTRL_RXREF1 ... S5M3500X_14F_CTRL_OVP2:
		/* Analog Read Register */
	case S5M3500X_150_ANA_RO0 ... S5M3500X_160_ANA_RO16:
		return true;
	/* OTP Block */
	case S5M3500X_200_HPL_OFFSET0 ... S5M3500X_23F_CHIP_ID6:
		return true;
	default:
		return false;
	}
}

/*
 * Return Value
 * True: If the register value can be write
 * False: If the register cannot be write
 */
static bool s5m3500x_writeable_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	/* Digital Block */
		/* Interrupt Register */
	case S5M3500X_008_IRQ1M ... S5M3500X_00D_IRQ6M:
		/* Clock & Reset */
	case S5M3500X_010_CLKGATE0 ... S5M3500X_01C_COM_OTP_ADRH:
		/* Digital Audio Interface */
	case S5M3500X_020_IF_FORM1 ... S5M3500X_025_IF_LOOPBACK:
	case S5M3500X_029_IF_FORM6:
	case S5M3500X_02F_GPIO_ST:
		/* Recording Path Digital */
	case S5M3500X_030_ADC1 ... S5M3500X_031_ADC2:
	case S5M3500X_033_ADC3 ... S5M3500X_035_AD_VOLR:
	case S5M3500X_037_AD_HPF ... S5M3500X_03E_AD_OFFSETR:
		/* Playback Path Digital */
	case S5M3500X_040_PLAY_MODE ... S5M3500X_042_PLAY_VOLR:
	case S5M3500X_044_PLAY_MIX0 ... S5M3500X_04F_HPR_OFFMSK2:
		/* Adaptive Volume Control */
	case S5M3500X_050_AVC1 ... S5M3500X_056_AVC7:
	case S5M3500X_058_AVC9 ... S5M3500X_074_AVC37:
	case S5M3500X_077_AVC40 ... S5M3500X_07F_AVC46:
		/* IRQ for LV */
	case S5M3500X_089_ANA_INTR_MASK ... S5M3500X_08E_OCP_INTR_MASK:
		/* Digital DSM COntrol */
	case S5M3500X_093_DSM_CON1 ... S5M3500X_09A_GPIO3_CON:
	case S5M3500X_09D_AVC_DWA_OFF_THRES ... S5M3500X_09E_GPIO123_CON:
		/* Auto Sequence Control */
	case S5M3500X_0A0_AMU_CTRL1 ... S5M3500X_0AE_RESERVED:
	case S5M3500X_0B0_TEST_CTRL1 ... S5M3500X_0BA_TEST_CTRL11:
		/* Headphone Management Unit Control */
	case S5M3500X_0C0_ACTR_JD1 ... S5M3500X_0CD_RESERVED:
	case S5M3500X_0D1_RESERVED ... S5M3500X_0DD_DCTR_DBNC6:
	case S5M3500X_0DF_RESERVED:
	case S5M3500X_0E0_DCTR_FSM1 ... S5M3500X_0E7_RESERVED:
	case S5M3500X_0FD_ACTR_GP ... S5M3500X_0FF_DCTR_GP2:
		return true;
	/* Analog Block */
		/* Analog Clock & Reference & CP Control */
	case S5M3500X_100_CTRL_REF1 ... S5M3500X_101_CTRL_REF2:
	case S5M3500X_105_AD_CLK0 ... S5M3500X_10F_DA_CP7:
		/* Analog Recording Path Control */
	case S5M3500X_110_CTRL_MIC1 ... S5M3500X_11E_CTRL_MIC15:
		/* Analog Playback Path Control */
	case S5M3500X_130_CTRL_RXREF1 ... S5M3500X_145_CTRL_HP14:
	case S5M3500X_148_CTRL_EP1 ... S5M3500X_14C_CTRL_EP5:
	case S5M3500X_14E_CTRL_OVP1 ... S5M3500X_14F_CTRL_OVP2:
		return true;
	/* OTP Block */
	case S5M3500X_200_HPL_OFFSET0 ... S5M3500X_23F_CHIP_ID6:
		return true;
	default:
		return false;
	}
}

/*
 * Put a register map into cache only mode, not cause any effect HW device.
 */
void s5m3500x_regcache_cache_only_switch(struct s5m3500x_priv *s5m3500x, bool on)
{
	mutex_lock(&s5m3500x->regcache_lock);
	if (on)
		s5m3500x->cache_only++;
	else
		s5m3500x->cache_only--;
	mutex_unlock(&s5m3500x->regcache_lock);

	dev_dbg(s5m3500x->dev, "%s count %d enable %d\n", __func__, s5m3500x->cache_only, on);

	if (s5m3500x->cache_only < 0) {
		s5m3500x->cache_only = 0;
		return;
	}

	if (on) {
		if (s5m3500x->cache_only == 1)
			regcache_cache_only(s5m3500x->regmap, true);
	} else {
		if (s5m3500x->cache_only == 0)
			regcache_cache_only(s5m3500x->regmap, false);
	}
}
EXPORT_SYMBOL_GPL(s5m3500x_regcache_cache_only_switch);

/*
 * Put a register map into cache bypass mode, not cause any effect Cache.
 * priority of cache_bypass is higher than cache_only
 */
void s5m3500x_regcache_cache_bypass_switch(struct s5m3500x_priv *s5m3500x, bool on)
{
	mutex_lock(&s5m3500x->regcache_lock);
	if (on)
		s5m3500x->cache_bypass++;
	else
		s5m3500x->cache_bypass--;
	mutex_unlock(&s5m3500x->regcache_lock);

	dev_dbg(s5m3500x->dev, "%s count %d enable %d\n", __func__, s5m3500x->cache_bypass, on);

	if (s5m3500x->cache_bypass < 0) {
		s5m3500x->cache_bypass = 0;
		return;
	}

	if (on) {
		if (s5m3500x->cache_bypass == 1)
			regcache_cache_bypass(s5m3500x->regmap, true);
	} else {
		if (s5m3500x->cache_bypass == 0)
			regcache_cache_bypass(s5m3500x->regmap, false);
	}
}
EXPORT_SYMBOL_GPL(s5m3500x_regcache_cache_bypass_switch);

int s5m3500x_read(struct s5m3500x_priv *s5m3500x,
	unsigned int addr, unsigned int *value)
{
	int ret = 0;
	mutex_lock(&s5m3500x->reg_lock);
	ret = regmap_read(s5m3500x->regmap, addr, value);
	mutex_unlock(&s5m3500x->reg_lock);
	return ret;
}
EXPORT_SYMBOL_GPL(s5m3500x_read);

int s5m3500x_write(struct s5m3500x_priv *s5m3500x,
	unsigned int addr, unsigned int value)
{
	int ret = 0;
	mutex_lock(&s5m3500x->reg_lock);
	ret = regmap_write(s5m3500x->regmap, addr, value);
	mutex_unlock(&s5m3500x->reg_lock);
	return ret;
}
EXPORT_SYMBOL_GPL(s5m3500x_write);

int s5m3500x_update_bits(struct s5m3500x_priv *s5m3500x,
	unsigned int addr, unsigned int mask, unsigned int value)
{
	int ret = 0;
	mutex_lock(&s5m3500x->reg_lock);
	ret = regmap_update_bits(s5m3500x->regmap, addr, mask, value);
	mutex_unlock(&s5m3500x->reg_lock);
	return ret;
}
EXPORT_SYMBOL_GPL(s5m3500x_update_bits);

int s5m3500x_read_only_cache(struct s5m3500x_priv *s5m3500x,
	unsigned int addr, unsigned int *value)
{
	int ret = 0;
	struct device *dev = s5m3500x->dev;

	mutex_lock(&s5m3500x->reg_lock);

	if (!is_cache_bypass_enabled(s5m3500x)) {
		s5m3500x_regcache_cache_only_switch(s5m3500x, true);
		ret = regmap_read(s5m3500x->regmap, addr, value);
		s5m3500x_regcache_cache_only_switch(s5m3500x, false);
	} else {
		dev_err(dev, "%s: cannot read 0x%x cache register.\n", __func__,addr);
		ret = -1;
	}

	mutex_unlock(&s5m3500x->reg_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(s5m3500x_read_only_cache);

int s5m3500x_write_only_cache(struct s5m3500x_priv *s5m3500x,
	unsigned int addr, unsigned int value)
{
	int ret = 0;
	struct device *dev = s5m3500x->dev;

	mutex_lock(&s5m3500x->reg_lock);

	if (!is_cache_bypass_enabled(s5m3500x)) {
		s5m3500x_regcache_cache_only_switch(s5m3500x, true);
		ret = regmap_write(s5m3500x->regmap, addr, value);
		s5m3500x_regcache_cache_only_switch(s5m3500x, false);
	} else {
		dev_err(dev, "%s: cannot write 0x%x cache register.\n", __func__,addr);
		ret = -1;
	}

	mutex_unlock(&s5m3500x->reg_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(s5m3500x_write_only_cache);

int s5m3500x_update_bits_only_cache(struct s5m3500x_priv *s5m3500x,
	 int addr, unsigned int mask, unsigned int value)
{
	int ret = 0;
	struct device *dev = s5m3500x->dev;

	mutex_lock(&s5m3500x->reg_lock);

	if (!is_cache_bypass_enabled(s5m3500x)) {
		s5m3500x_regcache_cache_only_switch(s5m3500x, true);
		ret = regmap_update_bits(s5m3500x->regmap, addr, mask, value);
		s5m3500x_regcache_cache_only_switch(s5m3500x, false);
	} else {
		dev_err(dev, "%s: cannot update 0x%x cache register.\n", __func__,addr);
		ret = -1;
	}

	mutex_unlock(&s5m3500x->reg_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(s5m3500x_update_bits_only_cache);

int s5m3500x_read_only_hardware(struct s5m3500x_priv *s5m3500x,
	unsigned int addr, unsigned int *value)
{
	int ret = 0;
	struct device *dev = s5m3500x->dev;

	mutex_lock(&s5m3500x->reg_lock);

	if (!is_cache_only_enabled(s5m3500x)) {
		s5m3500x_regcache_cache_bypass_switch(s5m3500x, true);
		ret = regmap_read(s5m3500x->regmap, addr, value);
		s5m3500x_regcache_cache_bypass_switch(s5m3500x, false);
	} else {
		dev_err(dev, "%s: cannot read 0x%x HW register.\n", __func__,addr);
		ret = -1;
	}

	mutex_unlock(&s5m3500x->reg_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(s5m3500x_read_only_hardware);

int s5m3500x_write_only_hardware(struct s5m3500x_priv *s5m3500x,
	unsigned int addr, unsigned int value)
{
	int ret = 0;
	struct device *dev = s5m3500x->dev;

	mutex_lock(&s5m3500x->reg_lock);

	if (!is_cache_only_enabled(s5m3500x)) {
		s5m3500x_regcache_cache_bypass_switch(s5m3500x, true);
		ret = regmap_write(s5m3500x->regmap, addr, value);
		s5m3500x_regcache_cache_bypass_switch(s5m3500x, false);
	} else {
		dev_err(dev, "%s: cannot write 0x%x HW register.\n", __func__,addr);
		ret = -1;
	}

	mutex_unlock(&s5m3500x->reg_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(s5m3500x_write_only_hardware);

int s5m3500x_update_bits_only_hardware(struct s5m3500x_priv *s5m3500x,
	unsigned int addr, unsigned int mask, unsigned int value)
{
	int ret = 0;
	struct device *dev = s5m3500x->dev;

	mutex_lock(&s5m3500x->reg_lock);

	if (!is_cache_only_enabled(s5m3500x)) {
		s5m3500x_regcache_cache_bypass_switch(s5m3500x, true);
		ret = regmap_update_bits(s5m3500x->regmap, addr, mask, value);
		s5m3500x_regcache_cache_bypass_switch(s5m3500x, false);
	} else {
		dev_err(dev, "%s: cannot update 0x%x HW register.\n", __func__,addr);
		ret = -1;
	}

	mutex_unlock(&s5m3500x->reg_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(s5m3500x_update_bits_only_hardware);

static void s5m3500x_print_dump(struct s5m3500x_priv *s5m3500x, int reg[], int register_block)
{
	u16 line, p_line;

	if(register_block == S5M3500X_DIGITAL_BLOCK) {
		dev_info(s5m3500x->dev, "========== Codec Digital Block(0x0) Dump ==========\n");
	} else if(register_block == S5M3500X_ANALOG_BLOCK) {
		dev_info(s5m3500x->dev, "========== Codec Analog Block(0x1) Dump ==========\n");
	} else if(register_block == S5M3500X_OTP_BLOCK) {
		dev_info(s5m3500x->dev, "========== Codec OTP Block(0x2) Dump ==========\n");
	}

	dev_info(s5m3500x->dev, "      00 01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f\n");

	for (line = 0; line <= 0xf; line++) {
		p_line = line << 4;
		dev_info(s5m3500x->dev,
				"%04x: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
				p_line,	reg[p_line + 0x0], reg[p_line + 0x1], reg[p_line + 0x2],
				reg[p_line + 0x3], reg[p_line + 0x4], reg[p_line + 0x5],
				reg[p_line + 0x6], reg[p_line + 0x7], reg[p_line + 0x8],
				reg[p_line + 0x9], reg[p_line + 0xa], reg[p_line + 0xb],
				reg[p_line + 0xc], reg[p_line + 0xd], reg[p_line + 0xe],
				reg[p_line + 0xf]);
	}
}

static void s5m3500x_show_regmap_cache_registers(struct s5m3500x_priv *s5m3500x, int register_block)
{
	int reg[S5M3500X_BLOCK_SIZE] = {0,};
	int i = S5M3500X_REGISTER_START, end = (register_block + 1) * S5M3500X_BLOCK_SIZE;

	dev_info(s5m3500x->dev, "%s enter\n", __func__);

	/* read from Cache */
	for (i = register_block * S5M3500X_BLOCK_SIZE; i < end ; i++)
		s5m3500x_read_only_cache(s5m3500x, i, &reg[i % S5M3500X_BLOCK_SIZE]);

	s5m3500x_print_dump(s5m3500x, reg, register_block);

	dev_info(s5m3500x->dev, "%s done\n", __func__);
}

static void s5m3500x_show_regmap_hardware_registers(struct s5m3500x_priv *s5m3500x, int register_block)
{
	int reg[S5M3500X_BLOCK_SIZE] = {0,};
	int i = S5M3500X_REGISTER_START, end = (register_block + 1) * S5M3500X_BLOCK_SIZE;

	dev_info(s5m3500x->dev, "%s enter\n", __func__);

	/* read from HW */
	for (i = register_block * S5M3500X_BLOCK_SIZE; i < end ; i++)
		s5m3500x_read_only_hardware(s5m3500x, i, &reg[i % S5M3500X_BLOCK_SIZE]);

	s5m3500x_print_dump(s5m3500x, reg, register_block);

	dev_info(s5m3500x->dev, "%s done\n", __func__);
}

/*
 * Sync the register cache with the hardware
 * if cache register value is same as reg_defaults value, write is not occurred.
 */
int s5m3500x_regmap_register_sync(struct s5m3500x_priv *s5m3500x)
{
	int ret = 0;

	dev_dbg(s5m3500x->dev, "%s enter\n", __func__);

	mutex_lock(&s5m3500x->regsync_lock);

	if (!s5m3500x->cache_only) {
		regcache_mark_dirty(s5m3500x->regmap);
		ret = regcache_sync(s5m3500x->regmap);
		if (ret) {
			dev_err(s5m3500x->dev, "%s: failed to sync regmap : %d\n",
				__func__, ret);
		}
	} else {
		dev_err(s5m3500x->dev, "%s: regcache_cache_only is already occupied.(%d)\n",
			__func__, s5m3500x->cache_only);
		ret = -1;
	}
	mutex_unlock(&s5m3500x->regsync_lock);

	dev_dbg(s5m3500x->dev, "%s exit\n", __func__);

	return ret;
}

/*
 * Sync up with cache register to hardware
 * need to start and end address for register sync
 */
int s5m3500x_hardware_register_sync(struct s5m3500x_priv *s5m3500x, int start, int end)
{
	int reg = 0, i = S5M3500X_REGISTER_START;
	int ret = 0;

	dev_dbg(s5m3500x->dev, "%s enter\n", __func__);

	mutex_lock(&s5m3500x->regsync_lock);

	for (i = start; i <= end ; i++) {
		if(s5m3500x_writeable_register(s5m3500x->dev,i)) {
			/* read from cache */
			s5m3500x_read_only_cache(s5m3500x, i, &reg);
			/* write to Cache */
			s5m3500x_write_only_hardware(s5m3500x, i, reg);
		}
	}

	mutex_unlock(&s5m3500x->regsync_lock);

	/* call s5m3500x_regmap_register_sync for sync from HW to cache */
	s5m3500x_regmap_register_sync(s5m3500x);

	dev_dbg(s5m3500x->dev, "%s exit\n", __func__);
	return ret;
}

/*
 * Sync Cache with reg_defaults when power is off
 */
int s5m3500x_cache_register_sync_default(struct s5m3500x_priv *s5m3500x)
{
	int ret = 0, i = 0, array_size = ARRAY_SIZE(s5m3500x_reg_defaults), value = 0;

	dev_dbg(s5m3500x->dev, "%s enter\n", __func__);

	mutex_lock(&s5m3500x->regsync_lock);

	for (i = 0 ; i < array_size ; i++) {
		ret = s5m3500x_read_only_cache(s5m3500x, s5m3500x_reg_defaults[i].reg, &value);
		if (!ret) {
			if (value != s5m3500x_reg_defaults[i].def)
				s5m3500x_write_only_cache(s5m3500x, s5m3500x_reg_defaults[i].reg, s5m3500x_reg_defaults[i].def);
		}
	}

	mutex_unlock(&s5m3500x->regsync_lock);

	dev_dbg(s5m3500x->dev, "%s exit\n", __func__);

	return ret;
}

const struct regmap_config s5m3500x_regmap = {
	.reg_bits = 16,
	.val_bits = 8,
	.reg_format_endian = REGMAP_ENDIAN_LITTLE,
	.val_format_endian = REGMAP_ENDIAN_LITTLE,

	.name = "i2c,S5M3500X",
	.max_register = S5M3500X_REGISTER_END,
	.readable_reg = s5m3500x_readable_register,
	.writeable_reg = s5m3500x_writeable_register,
	.volatile_reg = s5m3500x_volatile_register,
	.use_single_read = true,
	.use_single_write = true,
	.cache_type = REGCACHE_RBTREE,
	.reg_defaults = s5m3500x_reg_defaults,
	.num_reg_defaults = ARRAY_SIZE(s5m3500x_reg_defaults),
};

/* initialize regmap again */
int s5m3500x_regmap_reinit_cache(struct s5m3500x_priv *s5m3500x)
{
	return regmap_reinit_cache(s5m3500x->regmap, &s5m3500x_regmap);
}

/*
 * patches are only updated HW registers, not cache.
 * Update reg_defaults value so that update hw and cache both
 * when regcache_sync is called.
 */
void s5m3500x_update_reg_defaults(const struct reg_sequence *patch, const int array_size)
{
	int i = 0, j = 0;

	for (i = 0; i < array_size; i++) {
		for (j = 0; j < ARRAY_SIZE(s5m3500x_reg_defaults); j++) {
			if (s5m3500x_reg_defaults[j].reg == patch[i].reg) {
				s5m3500x_reg_defaults[j].def = patch[i].def;
				break;
			}
		}
	}
}

/*
 * Digital Audio Interface - struct snd_kcontrol_new
 */

/*
 * s5m3500x_i2s_df_format - I2S Data Format Selection
 *
 * Map as per data-sheet:
 * 000 : I2S, PCM Long format
 * 001 : Left Justified format
 * 010 : Right Justified format
 * 011 : Not used
 * 100 : I2S, PCM Short format
 * 101 ~ 111 : Not used
 * I2S_DF : reg(0x20), shift(4), width(3)
 */
static const char * const s5m3500x_i2s_df_format[] = {
	"PCM-L", "LJ", "RJ", "Zero0", "PCM-S", "Zero1", "Zero2", "Zero3"
};

static SOC_ENUM_SINGLE_DECL(s5m3500x_i2s_df_enum, S5M3500X_020_IF_FORM1,
		I2S_DF_SHIFT, s5m3500x_i2s_df_format);

/*
 * s5m3500x_i2s_bck_format - I2S BCLK Data Selection
 *
 * Map as per data-sheet:
 * 0 : Normal
 * 1 : Invert
 * I2S_DF : reg(0x20), shift(1)
 */

static const char * const s5m3500x_i2s_bck_format[] = {
	"Normal", "Invert"
};

static SOC_ENUM_SINGLE_DECL(s5m3500x_i2s_bck_enum, S5M3500X_020_IF_FORM1,
		BCLK_POL_SHIFT, s5m3500x_i2s_bck_format);

/*
 * s5m3500x_i2s_lrck_format - I2S LRCLK Data Selection
 *
 * Map as per data-sheet:
 * 0 : 0&2 Low, 1&3 High
 * 1 : 1&3 Low, 0&2 High
 * I2S_DF : reg(0x20), shift(0)
 */

static const char * const s5m3500x_i2s_lrck_format[] = {
	"Normal", "Invert"
};

static SOC_ENUM_SINGLE_DECL(s5m3500x_i2s_lrck_enum, S5M3500X_020_IF_FORM1,
		LRCLK_POL_SHIFT, s5m3500x_i2s_lrck_format);

/*
 * s5m3500x_dvol_adc_tlv - Digital volume for ADC
 *
 * Map as per data-sheet:
 * 0x00 ~ 0xE0 : +42dB to -70dB, step 0.5dB
 * 0xE0 ~ 0xE5 : -70dB to -80dB, step 2.0dB
 *
 * When the map is in descending order, we need to set the invert bit
 * and arrange the map in ascending order. The offsets are calculated as
 * (max - offset).
 *
 * offset_in_table = max - offset_actual;
 *
 * DVOL_ADCL : reg(0x34), shift(0), width(8), invert(1), max(0xE5)
 * DVOL_ADCR : reg(0x35), shift(0), width(8), invert(1), max(0xE5)
 */
static const unsigned int s5m3500x_dvol_adc_tlv[] = {
	TLV_DB_RANGE_HEAD(2),
	0x00, 0x05, TLV_DB_SCALE_ITEM(-8000, 200, 0),
	0x06, 0xE5, TLV_DB_SCALE_ITEM(-6950, 50, 0),
};


static int mic1_boost_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3500x_priv *s5m3500x = snd_soc_component_get_drvdata(codec);
	unsigned int mic_boost;

	s5m3500x_read(s5m3500x, S5M3500X_112_CTRL_MIC3, &mic_boost);
	ucontrol->value.integer.value[0] = (mic_boost & CTVOL_BST1_MASK) >> CTVOL_BST1_SHIFT;

	return 0;
}

static int mic1_boost_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3500x_priv *s5m3500x = snd_soc_component_get_drvdata(codec);
	int value = ucontrol->value.integer.value[0];

	s5m3500x_update_bits(s5m3500x, S5M3500X_112_CTRL_MIC3, CTVOL_BST1_MASK, value << CTVOL_BST1_SHIFT);

	return 0;
}

static int mic2_boost_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3500x_priv *s5m3500x = snd_soc_component_get_drvdata(codec);
	unsigned int mic_boost;

	s5m3500x_read(s5m3500x, S5M3500X_112_CTRL_MIC3, &mic_boost);
	ucontrol->value.integer.value[0] = (mic_boost & CTVOL_BST2_MASK) >> CTVOL_BST2_SHIFT;

	return 0;
}

static int mic2_boost_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3500x_priv *s5m3500x = snd_soc_component_get_drvdata(codec);
	int value = ucontrol->value.integer.value[0];

	s5m3500x_update_bits(s5m3500x, S5M3500X_112_CTRL_MIC3, CTVOL_BST2_MASK, value << CTVOL_BST2_SHIFT);

	return 0;
}


static int micbias1_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3500x_priv *s5m3500x = snd_soc_component_get_drvdata(codec);
	unsigned int value;
	bool micbias_enable;

	s5m3500x_read(s5m3500x, S5M3500X_0CA_ACTR_MCB6, &value);
	micbias_enable = value & APW_MCB1_MASK;

	ucontrol->value.integer.value[0] = micbias_enable;

	return 0;
}

static int micbias1_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3500x_priv *s5m3500x = snd_soc_component_get_drvdata(codec);
	int value = ucontrol->value.integer.value[0];

	if (value)
		s5m3500x_update_bits(s5m3500x, S5M3500X_0CA_ACTR_MCB6, APW_MCB1_MASK, APW_MCB1_ENABLE);
	else
		s5m3500x_update_bits(s5m3500x, S5M3500X_0CA_ACTR_MCB6, APW_MCB1_MASK, 0x00);

	dev_info(codec->dev, "%s called, micbias1 turn %s\n", __func__, value ? "On" : "Off");
	return 0;
}

static int micbias2_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3500x_priv *s5m3500x = snd_soc_component_get_drvdata(codec);
	unsigned int value;
	bool micbias_enable;

	s5m3500x_read(s5m3500x, S5M3500X_0CA_ACTR_MCB6, &value);
	micbias_enable = value & APW_MCB2_MASK;

	ucontrol->value.integer.value[0] = micbias_enable;

	return 0;
}

static int micbias2_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3500x_priv *s5m3500x = snd_soc_component_get_drvdata(codec);
	int value = ucontrol->value.integer.value[0];

	if (value)
		s5m3500x_update_bits(s5m3500x, S5M3500X_0CA_ACTR_MCB6, APW_MCB2_MASK, APW_MCB2_ENABLE);
	else
		s5m3500x_update_bits(s5m3500x, S5M3500X_0CA_ACTR_MCB6, APW_MCB2_MASK, 0x00);

	dev_info(codec->dev, "%s called, micbias2 turn %s\n", __func__, value ? "On" : "Off");

	return 0;
}

/*
 * s5m3500x_adc_dat_src - I2S channel input data selection
 *
 * Map as per data-sheet:
 * 00 : ADC Left Channel Data (ADC0)
 * 01 : ADC Right Channel Data (ADC1)
 * 10 : LR Mix : (ADC0 + ADC1) / 2
 * 11 : Zero or DAC (0x21[6] : 1)
 *
 * SEL_ADC0 : reg(0x23), shift(0), width(2)
 * SEL_ADC1 : reg(0x23), shift(2), width(2)
 */
static const char * const s5m3500x_adc_dat_src[] = {
		"ADCL", "ADCR", "LRMIX", "Zero"
};

static SOC_ENUM_SINGLE_DECL(s5m3500x_adc_dat_enum0, S5M3500X_023_IF_FORM4,
		SEL_ADC0_SHIFT, s5m3500x_adc_dat_src);

static SOC_ENUM_SINGLE_DECL(s5m3500x_adc_dat_enum1,	S5M3500X_023_IF_FORM4,
		SEL_ADC1_SHIFT, s5m3500x_adc_dat_src);

static int amic_delay_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3500x_priv *s5m3500x = snd_soc_component_get_drvdata(codec);

	ucontrol->value.integer.value[0] = s5m3500x->amic_delay;
	return 0;
}

static int amic_delay_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3500x_priv *s5m3500x = snd_soc_component_get_drvdata(codec);
	int value = ucontrol->value.integer.value[0];

	s5m3500x->amic_delay = value;
	return 0;
}

static const char * const s5m3500x_hpf_sel_text[] = {
	"HPF-15Hz", "HPF-33Hz", "HPF-60Hz", "HPF-113Hz"
};

static SOC_ENUM_SINGLE_DECL(s5m3500x_hpf_sel_enum, S5M3500X_037_AD_HPF,
		HPF_MODE_SHIFT, s5m3500x_hpf_sel_text);

static const char * const s5m3500x_hpf_order_text[] = {
	"HPF-2nd", "HPF-1st"
};

static SOC_ENUM_SINGLE_DECL(s5m3500x_hpf_order_enum, S5M3500X_037_AD_HPF,
		HPF_ORDER_SHIFT, s5m3500x_hpf_order_text);

static const char * const s5m3500x_hpf_channel_text[] = {
	"Off", "On"
};

static SOC_ENUM_SINGLE_DECL(s5m3500x_hpf_channel_enum_l, S5M3500X_037_AD_HPF,
		HPF_ENL_SHIFT, s5m3500x_hpf_channel_text);

static SOC_ENUM_SINGLE_DECL(s5m3500x_hpf_channel_enum_r, S5M3500X_037_AD_HPF,
		HPF_ENR_SHIFT, s5m3500x_hpf_channel_text);

/*
 * s5m3500x_amic2_in_src - AMIC2 input data selection
 *
 * Map as per data-sheet:
 * 0 : MICIN2
 * 1 : MICIN3
 *
 * SEL_DAC0 : reg(0x111), shift(0), width(1)
 */
static const char * const s5m3500x_amic2_in_src[] = {
		"MICIN3", "MICIN2",
};

static SOC_ENUM_SINGLE_DECL(s5m3500x_amic2_in_enum0, S5M3500X_111_CTRL_MIC2,
		SEL_MICIN2_SHIFT, s5m3500x_amic2_in_src);

/*
 * DAC(Rx) volume control - struct snd_kcontrol_new
 */
/*
 * s5m3500x_dvol_dac_tlv - gain control for EAR/RCV path
 *
 * Map as per data-sheet:
 * 0x30 ~ 0xE0 : +18dB to -70dB, step 0.5dB
 * 0xE1 ~ 0xE5 : -72dB to -80dB, step 2.0dB
 * 0xE6 : -82.4dB
 * 0xE7 ~ 0xE9 : -84.3dB to -96.3dB, step 6dB
 *
 * When the map is in descending order, we need to set the invert bit
 * and arrange the map in ascending order. The offsets are calculated as
 * (max - offset).
 *
 * offset_in_table = max - offset_actual;
 *
 * DVOL_DAL : reg(0x41), shift(0), width(8), invert(1), max(0xE9)
 * DVOL_DAR : reg(0x42), shift(0), width(8), invert(1), max(0xE9)
 */
static const unsigned int s5m3500x_dvol_dac_tlv[] = {
	TLV_DB_RANGE_HEAD(4),
	0x01, 0x03, TLV_DB_SCALE_ITEM(-9630, 600, 0),
	0x04, 0x04, TLV_DB_SCALE_ITEM(-8240, 0, 0),
	0x05, 0x09, TLV_DB_SCALE_ITEM(-8000, 200, 0),
	0x0A, 0xBA, TLV_DB_SCALE_ITEM(-7000, 50, 0),
};


/*
 * DAC(Rx) path control - struct snd_kcontrol_new
 */
/*
 * s5m3500x_dac_dat_src - I2S channel input data selection
 *
 * Map as per data-sheet:
 * 00 : DAC I2S Channel 0
 * 01 : DAC I2S Channel 1
 *
 * SEL_DAC0 : reg(0x24), shift(0), width(2)
 * SEL_DAC1 : reg(0x24), shift(4), width(2)
 */
static const char * const s5m3500x_dac_dat_src[] = {
		"CH0", "CH1",
};

static SOC_ENUM_SINGLE_DECL(s5m3500x_dac_dat_enum0, S5M3500X_024_IF_FORM5,
		SEL_DAC0_SHIFT, s5m3500x_dac_dat_src);

static SOC_ENUM_SINGLE_DECL(s5m3500x_dac_dat_enum1, S5M3500X_024_IF_FORM5,
		SEL_DAC1_SHIFT, s5m3500x_dac_dat_src);

/*
 * s5m3500x_dac_mixl_mode_text - DACL Mixer Selection
 *
 * Map as per data-sheet:
 * 000 : Data L
 * 001 : (L+R)/2 Mono
 * 010 : (L+R) Mono
 * 011 : (L+R)/2 Polarity Changed
 * 100 : (L+R) Polarity Changed
 * 101 : Zero Padding
 * 110 : Data L Polarity Changed
 * 111 : Data R
 *
 * DAC_MIXL : reg(0x44), shift(4), width(3)
 */
static const char * const s5m3500x_dac_mixl_mode_text[] = {
		"Data L", "LR/2 Mono", "LR Mono", "LR/2 PolCh",
		"LR PolCh", "Zero", "Data L PolCh", "Data R"
};

static SOC_ENUM_SINGLE_DECL(s5m3500x_dac_mixl_mode_enum, S5M3500X_044_PLAY_MIX0,
		DAC_MIXL_SHIFT, s5m3500x_dac_mixl_mode_text);

/*
 * s5m3500x_dac_mixr_mode_text - DACR Mixer Selection
 *
 * Map as per data-sheet:
 * 000 : Data R
 * 001 : (L+R)/2 Polarity Changed
 * 010 : (L+R) Polarity Changed
 * 011 : (L+R)/2 Mono
 * 100 : (L+R) Mono
 * 101 : Zero Padding
 * 110 : Data R Polarity Changed
 * 111 : Data L
 *
 * DAC_MIXR : reg(0x44), shift(0), width(3)
 */
static const char * const s5m3500x_dac_mixr_mode_text[] = {
		"Data R", "LR/2 PolCh", "LR PolCh", "LR/2 Mono",
		"LR Mono", "Zero", "Data R PolCh", "Data L"
};

static SOC_ENUM_SINGLE_DECL(s5m3500x_dac_mixr_mode_enum, S5M3500X_044_PLAY_MIX0,
		DAC_MIXR_SHIFT, s5m3500x_dac_mixr_mode_text);


/*
 * Codec control - struct snd_kcontrol_new
 */
static int codec_enable_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3500x_priv *s5m3500x = snd_soc_component_get_drvdata(codec);
	int val = gpio_get_value_cansleep(s5m3500x->resetb_gpio);

	ucontrol->value.integer.value[0] = val;

	return 0;
}

static int codec_enable_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = codec->dev;
	struct s5m3500x_priv *s5m3500x = dev_get_drvdata(dev);
	int value = ucontrol->value.integer.value[0];

	dev_info(dev, "%s: codec enable : %s\n",
			__func__, (value) ? "On" : "Off");

	if (s5m3500x->power_always_on) {
		dev_info(dev, "%s: codec power_always_on is set. codec power can't be controlled.\n", __func__);
		return 0;
	}

	if (value)
		s5m3500x_codec_enable(dev, true);
	else
		s5m3500x_codec_disable(dev, true);

	return 0;
}

static int codec_regdump_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int codec_regdump_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_kcontrol_component(kcontrol);
	struct s5m3500x_priv *s5m3500x = snd_soc_component_get_drvdata(codec);
	int value = ucontrol->value.integer.value[0];
	int i = 0;

	if (value) {
		for ( i = 0; i < S5M3500X_REGISTER_BLOCKS; i++) {
			s5m3500x_show_regmap_cache_registers(s5m3500x,i);
			s5m3500x_show_regmap_hardware_registers(s5m3500x,i);
		}
	}

	return 0;
}

/*
 * struct snd_kcontrol_new s5m3500x_snd_control
 *
 * Every distinct bit-fields within the CODEC SFR range may be considered
 * as a control elements. Such control elements are defined here.
 *
 * Depending on the access mode of these registers, different macros are
 * used to define these control elements.
 *
 * SOC_ENUM: 1-to-1 mapping between bit-field value and provided text
 * SOC_SINGLE: Single register, value is a number
 * SOC_SINGLE_TLV: Single register, value corresponds to a TLV scale
 * SOC_SINGLE_TLV_EXT: Above + custom get/set operation for this value
 * SOC_SINGLE_RANGE_TLV: Register value is an offset from minimum value
 * SOC_DOUBLE: Two bit-fields are updated in a single register
 * SOC_DOUBLE_R: Two bit-fields in 2 different registers are updated
 */

/*
 * All the data goes into s5m3500x_snd_controls.
 * All path inter-connections goes into s5m3500x_dapm_routes
 */
static const struct snd_kcontrol_new s5m3500x_snd_controls[] = {
	/*
	 * Digital Audio Interface
	 */
	SOC_ENUM("I2S DF", s5m3500x_i2s_df_enum),
	SOC_ENUM("I2S BCLK POL", s5m3500x_i2s_bck_enum),
	SOC_ENUM("I2S LRCK POL", s5m3500x_i2s_lrck_enum),

	/*
	 * ADC(Tx) Volume control
	 */
	SOC_SINGLE_TLV("ADC Left Gain", S5M3500X_034_AD_VOLL,
			DVOL_ADCL_SHIFT,
			ADC_DVOL_MAXNUM, 1, s5m3500x_dvol_adc_tlv),
	SOC_SINGLE_TLV("ADC Right Gain", S5M3500X_035_AD_VOLR,
			DVOL_ADCR_SHIFT,
			ADC_DVOL_MAXNUM, 1, s5m3500x_dvol_adc_tlv),

	SOC_SINGLE_EXT("MIC1 Boost Gain", SND_SOC_NOPM, 0, CTVOL_BST_MAXNUM, 0,
			mic1_boost_get, mic1_boost_put),
	SOC_SINGLE_EXT("MIC2 Boost Gain", SND_SOC_NOPM, 0, CTVOL_BST_MAXNUM, 0,
			mic2_boost_get, mic2_boost_put),

	/*
	 * ADC(Tx) Mic Bias control
	 */

	SOC_SINGLE_EXT("MIC BIAS1", SND_SOC_NOPM, 0, 1, 0,
			micbias1_get, micbias1_put),
	SOC_SINGLE_EXT("MIC BIAS2", SND_SOC_NOPM, 0, 1, 0,
			micbias2_get, micbias2_put),

	/*
	 * ADC(Tx) path control
	 */

	SOC_ENUM("ADC DAT Mux0", s5m3500x_adc_dat_enum0),
	SOC_ENUM("ADC DAT Mux1", s5m3500x_adc_dat_enum1),

	/*
	 * ADC(Tx) Mic Enable Delay
	 */

	SOC_SINGLE_EXT("AMIC delay", SND_SOC_NOPM, 0, AMIC_MAX_DELAY, 0,
			amic_delay_get, amic_delay_put),

	/*
	 * ADC(Tx) HPF Tuning Control
	 */
	SOC_ENUM("HPF Tuning", s5m3500x_hpf_sel_enum),
	SOC_ENUM("HPF Order", s5m3500x_hpf_order_enum),
	SOC_ENUM("HPF Left", s5m3500x_hpf_channel_enum_l),
	SOC_ENUM("HPF Right", s5m3500x_hpf_channel_enum_r),

	/*
	 * ADC(Tx) Select MIC2IN
	 */
	 SOC_ENUM("AMIC2 Input", s5m3500x_amic2_in_enum0),
	 

	/*
	 * DAC(Rx) volume control
	 */
	SOC_DOUBLE_R_RANGE_TLV("DAC Gain", S5M3500X_041_PLAY_VOLL, S5M3500X_042_PLAY_VOLR,
			DVOL_DAL_SHIFT,
			DAC_DVOL_MAXNUM, DAC_DVOL_MINNUM, 1, s5m3500x_dvol_dac_tlv),

	/*
	 * DAC(Rx) path control
	 */
	SOC_ENUM("DAC DAT Mux0", s5m3500x_dac_dat_enum0),
	SOC_ENUM("DAC DAT Mux1", s5m3500x_dac_dat_enum1),
	SOC_ENUM("DAC MIXL Mode", s5m3500x_dac_mixl_mode_enum),
	SOC_ENUM("DAC MIXR Mode", s5m3500x_dac_mixr_mode_enum),

	/*
	 * Codec control
	 */
	SOC_SINGLE_EXT("Codec Enable", SND_SOC_NOPM, 0, 1, 0,
			codec_enable_get, codec_enable_put),
	SOC_SINGLE_EXT("Codec Regdump", SND_SOC_NOPM, 0, 1, 0,
			codec_regdump_get, codec_regdump_put),
};

/*
 * snd_soc_dapm_widget controls set
 */
static const char * const s5m3500x_device_enable_enum_src[] = {
	"Off", "On",
};

static SOC_ENUM_SINGLE_DECL(s5m3500x_device_enable_enum, SND_SOC_NOPM, 0,
								s5m3500x_device_enable_enum_src);

/* Tx Devices */
/*Common Configuration for Tx*/
int check_adc_mode(struct s5m3500x_priv *s5m3500x)
{
	unsigned int mode = 0;
	unsigned int val1 = 0;
	unsigned int adc_l_status, adc_r_status;

	s5m3500x_read(s5m3500x, S5M3500X_031_ADC2, &val1);

	adc_l_status = (val1 & SEL_INPL_MASK) >> SEL_INPL_SHIFT;
	adc_r_status = (val1 & SEL_INPR_MASK) >> SEL_INPR_SHIFT;

	if (adc_l_status != ADC_INP_SEL_ZERO)
		mode++;
	if (adc_r_status != ADC_INP_SEL_ZERO)
		mode++;

	return mode;
}

static int vmid_ev(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *codec = snd_soc_dapm_to_component(w->dapm);
	struct s5m3500x_priv *s5m3500x = snd_soc_component_get_drvdata(codec);
	bool dac_on;
	int adc_status = 0;

	dac_on = s5m3500x_check_device_status(s5m3500x, DEVICE_DAC_ON);
	adc_status = check_adc_mode(s5m3500x);
	dev_info(codec->dev, "%s called, event = %d, adc_status: %d\n", __func__, event, adc_status);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		s5m3500x_update_bits(s5m3500x,S5M3500X_010_CLKGATE0,
			SEQ_CLK_GATE_MASK|AVC_CLK_GATE_MASK|CED_CLK_GATE_MASK|ADC_CLK_GATE_MASK,
			SEQ_CLK_GATE_ENABLE|AVC_CLK_GATE_ENABLE|CED_CLK_GATE_ENABLE|ADC_CLK_GATE_ENABLE);
		switch (adc_status) {
			/* Setting Mono Mode */
			case AMIC_MONO:
				s5m3500x_update_bits(s5m3500x,S5M3500X_012_CLKGATE2,
					ADC_CIC_CGL_MASK|ADC_CIC_CGR_MASK, ADC_CIC_CGL_ENABLE);
					
				
				if(s5m3500x->capture_params.aifrate == SAMPLE_RATE_48KHZ)
				{
					s5m3500x_update_bits(s5m3500x,S5M3500X_016_CLK_MODE_SEL0,
						ADC_FSEL_MASK|DAC_FSEL_MASK,
						ADC_FSEL_NORMAL_MONO|DAC_FSEL_48KHZ);

					s5m3500x_update_bits(s5m3500x,S5M3500X_030_ADC1,
						FS_SEL_MASK|CH_MODE_MASK,
						FS_SEL_NORMAL|CH_MODE_MONO);
				}
				else
				{
					s5m3500x_update_bits(s5m3500x,S5M3500X_016_CLK_MODE_SEL0,
						ADC_FSEL_MASK|DAC_FSEL_MASK,
						ADC_FSEL_UHQA_MONO|DAC_FSEL_192KHZ);

					s5m3500x_update_bits(s5m3500x,S5M3500X_030_ADC1,
						FS_SEL_MASK|CH_MODE_MASK,
						FS_SEL_UHQA|CH_MODE_MONO);
				}
				break;
			/* Setting Stereo Mode */
			case AMIC_STEREO:
			default:
				s5m3500x_update_bits(s5m3500x,S5M3500X_012_CLKGATE2,
					ADC_CIC_CGL_MASK|ADC_CIC_CGR_MASK, ADC_CIC_CGL_ENABLE|ADC_CIC_CGR_ENABLE);

				if(s5m3500x->capture_params.aifrate == SAMPLE_RATE_48KHZ)
				{
					s5m3500x_update_bits(s5m3500x,S5M3500X_016_CLK_MODE_SEL0,
						ADC_FSEL_MASK|DAC_FSEL_MASK,
						ADC_FSEL_NORMAL_STEREO|DAC_FSEL_48KHZ);

					s5m3500x_update_bits(s5m3500x,S5M3500X_030_ADC1,
						FS_SEL_MASK|CH_MODE_MASK,
						FS_SEL_NORMAL|CH_MODE_STEREO);
				}
				else
				{
					s5m3500x_update_bits(s5m3500x,S5M3500X_016_CLK_MODE_SEL0,
						ADC_FSEL_MASK|DAC_FSEL_MASK,
						ADC_FSEL_UHQA_STEREO|DAC_FSEL_192KHZ);

					s5m3500x_update_bits(s5m3500x,S5M3500X_030_ADC1,
						FS_SEL_MASK|CH_MODE_MASK,
						FS_SEL_UHQA|CH_MODE_STEREO);
				}
				break;
		}
		s5m3500x_adc_digital_mute(s5m3500x, ADC_MUTE_ALL, true);
		break;
	case SND_SOC_DAPM_POST_PMU:
		s5m3500x_update_bits(s5m3500x,S5M3500X_014_RESETB0, RSTB_ADC_MASK|ADC_RESETB_MASK, ADC_RESETB_NORMAL);
		s5m3500x_update_bits(s5m3500x,S5M3500X_014_RESETB0, RSTB_ADC_MASK, RSTB_ADC_NORMAL);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		s5m3500x_adc_digital_mute(s5m3500x, ADC_MUTE_ALL, true);
		break;
	case SND_SOC_DAPM_POST_PMD:
		s5m3500x_update_bits(s5m3500x,S5M3500X_014_RESETB0, RSTB_ADC_MASK|ADC_RESETB_MASK, 0x00);
		s5m3500x_update_bits(s5m3500x,S5M3500X_012_CLKGATE2, ADC_CIC_CGL_MASK|ADC_CIC_CGR_MASK, 0x00);
		/* CLKGATE 0 */
		s5m3500x_update_bits(s5m3500x, S5M3500X_010_CLKGATE0, ADC_CLK_GATE_MASK, 0x00);
		if(!dac_on)
			s5m3500x_update_bits(s5m3500x, S5M3500X_010_CLKGATE0,
				SEQ_CLK_GATE_MASK|AVC_CLK_GATE_MASK|CED_CLK_GATE_MASK, 0x00);
		break;
	}
	return 0;
}

static int adc_ev(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *codec = snd_soc_dapm_to_component(w->dapm);
	struct s5m3500x_priv *s5m3500x = snd_soc_component_get_drvdata(codec);
	bool amic_on;

	amic_on = s5m3500x_check_device_status(s5m3500x, DEVICE_AMIC_ON);

	dev_info(codec->dev, "%s called, event = %d, status: 0x%x\n", __func__, event, s5m3500x->status);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		break;
	case SND_SOC_DAPM_POST_PMU:
		break;
	case SND_SOC_DAPM_PRE_PMD:
		s5m3500x_adc_digital_mute(s5m3500x, ADC_MUTE_ALL, true);
		break;
	case SND_SOC_DAPM_POST_PMD:
		break;
	}
	return 0;
}

/* INP SEL */
static const char * const s5m3500x_inp_sel_src_l[] = {
	"AMIC_L ADC_L", "AMIC_R ADC_L", "Zero ADC_L",
};
static const char * const s5m3500x_inp_sel_src_r[] = {
	"AMIC_R ADC_R", "AMIC_L ADC_R", "Zero ADC_R",
};

static SOC_ENUM_SINGLE_DECL(s5m3500x_inp_sel_enum_l, S5M3500X_031_ADC2,
		SEL_INPL_SHIFT, s5m3500x_inp_sel_src_l);
static SOC_ENUM_SINGLE_DECL(s5m3500x_inp_sel_enum_r, S5M3500X_031_ADC2,
		SEL_INPR_SHIFT, s5m3500x_inp_sel_src_r);

static const struct snd_kcontrol_new s5m3500x_inp_sel_l =
		SOC_DAPM_ENUM("INP_SEL_L", s5m3500x_inp_sel_enum_l);
static const struct snd_kcontrol_new s5m3500x_inp_sel_r =
		SOC_DAPM_ENUM("INP_SEL_R", s5m3500x_inp_sel_enum_r);

/*Specific Configuration for Tx*/
static int mic1_pga_ev(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *codec = snd_soc_dapm_to_component(w->dapm);
	struct s5m3500x_priv *s5m3500x = snd_soc_component_get_drvdata(codec);

	dev_info(codec->dev, "%s called, event = %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		s5m3500x_write(s5m3500x, S5M3500X_115_CTRL_MIC6, 0x22);
		s5m3500x_write(s5m3500x, S5M3500X_11B_CTRL_MIC12, 0xF0);
		s5m3500x_write(s5m3500x, S5M3500X_11C_CTRL_MIC13, 0x04);
		break;
	case SND_SOC_DAPM_POST_PMU:
		cancel_delayed_work(&s5m3500x->adc_mute_work);
		s5m3500x_update_bits(s5m3500x, S5M3500X_018_PWAUTO_AD, APW_MIC1_MASK, APW_MIC1_ENABLE);
		s5m3500x_add_device_status(s5m3500x, DEVICE_AMIC1);
		queue_delayed_work(s5m3500x->adc_mute_wq, &s5m3500x->adc_mute_work, msecs_to_jiffies(s5m3500x->amic_delay));
		break;
	case SND_SOC_DAPM_PRE_PMD:
		s5m3500x_update_bits(s5m3500x, S5M3500X_018_PWAUTO_AD, APW_MIC1_MASK, 0x00);
		s5m3500x_remove_device_status(s5m3500x, DEVICE_AMIC1);
		break;
	case SND_SOC_DAPM_POST_PMD:
		s5m3500x_write(s5m3500x, S5M3500X_115_CTRL_MIC6, 0x22);
		s5m3500x_write(s5m3500x, S5M3500X_11B_CTRL_MIC12, 0xD0);
		s5m3500x_write(s5m3500x, S5M3500X_11C_CTRL_MIC13, 0x04);
		break;
	}
	return 0;
}

static int s5m3500x_dapm_mic1_on_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_dapm_kcontrol_component(kcontrol);
	struct s5m3500x_priv *s5m3500x = snd_soc_component_get_drvdata(codec);

	if (s5m3500x_check_device_status(s5m3500x, DEVICE_AMIC1))
		ucontrol->value.enumerated.item[0] = 1;
	else
		ucontrol->value.enumerated.item[0] = 0;
	return 0;
}

static int s5m3500x_dapm_mic1_on_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_dapm_kcontrol_component(kcontrol);
	struct snd_soc_dapm_context *dapm = snd_soc_dapm_kcontrol_dapm(kcontrol);
	struct s5m3500x_priv *s5m3500x = snd_soc_component_get_drvdata(codec);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	int value = ucontrol->value.enumerated.item[0];

	if (value > e->items - 1)
		return -EINVAL;

	if (value) {
		s5m3500x_add_device_status(s5m3500x, DEVICE_AMIC1);
		snd_soc_dapm_mixer_update_power(dapm, kcontrol, 1, NULL);
	} else {
		s5m3500x_remove_device_status(s5m3500x, DEVICE_AMIC1);
		snd_soc_dapm_mixer_update_power(dapm, kcontrol, 0, NULL);
	}

	dev_info(codec->dev, "%s : AMIC1 %s , status 0x%x\n", __func__, value ? "ON" : "OFF", s5m3500x->status);

	return 0;
}


static const struct snd_kcontrol_new mic1_on[] = {
	SOC_DAPM_ENUM_EXT("MIC1 On", s5m3500x_device_enable_enum,
					s5m3500x_dapm_mic1_on_get, s5m3500x_dapm_mic1_on_put),
};

static int mic2_pga_ev(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *codec = snd_soc_dapm_to_component(w->dapm);
	struct s5m3500x_priv *s5m3500x = snd_soc_component_get_drvdata(codec);

	dev_info(codec->dev, "%s called, event = %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		s5m3500x_update_bits(s5m3500x, S5M3500X_111_CTRL_MIC2, SEL_MICIN2_MASK, SEL_MICIN2_MICIN2);
		s5m3500x_write(s5m3500x, S5M3500X_115_CTRL_MIC6, 0x22);
		s5m3500x_write(s5m3500x, S5M3500X_11B_CTRL_MIC12, 0xF0);
		s5m3500x_write(s5m3500x, S5M3500X_11C_CTRL_MIC13, 0x04);
		break;
	case SND_SOC_DAPM_POST_PMU:
		cancel_delayed_work(&s5m3500x->adc_mute_work);
		s5m3500x_update_bits(s5m3500x, S5M3500X_018_PWAUTO_AD, APW_MIC2_MASK, APW_MIC2_ENABLE);
		s5m3500x_add_device_status(s5m3500x, DEVICE_AMIC2);
		queue_delayed_work(s5m3500x->adc_mute_wq, &s5m3500x->adc_mute_work, msecs_to_jiffies(s5m3500x->amic_delay));
		break;
	case SND_SOC_DAPM_PRE_PMD:
		s5m3500x_update_bits(s5m3500x, S5M3500X_018_PWAUTO_AD, APW_MIC2_MASK, 0x00);
		s5m3500x_remove_device_status(s5m3500x, DEVICE_AMIC2);
		break;
	case SND_SOC_DAPM_POST_PMD:
		s5m3500x_update_bits(s5m3500x, S5M3500X_111_CTRL_MIC2, SEL_MICIN2_MASK, SEL_MICIN2_MICIN2);
		s5m3500x_write(s5m3500x, S5M3500X_115_CTRL_MIC6, 0x22);
		s5m3500x_write(s5m3500x, S5M3500X_11B_CTRL_MIC12, 0xD0);
		s5m3500x_write(s5m3500x, S5M3500X_11C_CTRL_MIC13, 0x04);
		break;
	}
	return 0;
}

static int s5m3500x_dapm_mic2_on_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_dapm_kcontrol_component(kcontrol);
	struct s5m3500x_priv *s5m3500x = snd_soc_component_get_drvdata(codec);

	if (s5m3500x_check_device_status(s5m3500x, DEVICE_AMIC2))
		ucontrol->value.enumerated.item[0] = 1;
	else
		ucontrol->value.enumerated.item[0] = 0;
	return 0;
}

static int s5m3500x_dapm_mic2_on_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_dapm_kcontrol_component(kcontrol);
	struct snd_soc_dapm_context *dapm = snd_soc_dapm_kcontrol_dapm(kcontrol);
	struct s5m3500x_priv *s5m3500x = snd_soc_component_get_drvdata(codec);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	int value = ucontrol->value.enumerated.item[0];

	if (value > e->items - 1)
		return -EINVAL;

	if (value) {
		s5m3500x_add_device_status(s5m3500x, DEVICE_AMIC2);
		snd_soc_dapm_mixer_update_power(dapm, kcontrol, 1, NULL);
	} else {
		s5m3500x_remove_device_status(s5m3500x, DEVICE_AMIC2);
		snd_soc_dapm_mixer_update_power(dapm, kcontrol, 0, NULL);
	}

	dev_info(codec->dev, "%s : AMIC2 %s , status 0x%x\n", __func__, value ? "ON" : "OFF", s5m3500x->status);

	return 0;
}

static const struct snd_kcontrol_new mic2_on[] = {
	SOC_DAPM_ENUM_EXT("MIC2 On", s5m3500x_device_enable_enum,
					s5m3500x_dapm_mic2_on_get, s5m3500x_dapm_mic2_on_put),
};

static int mic3_pga_ev(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *codec = snd_soc_dapm_to_component(w->dapm);
	struct s5m3500x_priv *s5m3500x = snd_soc_component_get_drvdata(codec);

	dev_info(codec->dev, "%s called, event = %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		s5m3500x_update_bits(s5m3500x, S5M3500X_111_CTRL_MIC2, SEL_MICIN2_MASK, SEL_MICIN2_MICIN3);
		s5m3500x_write(s5m3500x, S5M3500X_115_CTRL_MIC6, 0x22);
		s5m3500x_write(s5m3500x, S5M3500X_11B_CTRL_MIC12, 0xF0);
		s5m3500x_write(s5m3500x, S5M3500X_11C_CTRL_MIC13, 0x04);
		break;
	case SND_SOC_DAPM_POST_PMU:
		cancel_delayed_work(&s5m3500x->adc_mute_work);
		s5m3500x_update_bits(s5m3500x, S5M3500X_018_PWAUTO_AD, APW_MIC2_MASK, APW_MIC2_ENABLE);
		s5m3500x_add_device_status(s5m3500x, DEVICE_AMIC3);
		queue_delayed_work(s5m3500x->adc_mute_wq, &s5m3500x->adc_mute_work, msecs_to_jiffies(s5m3500x->amic_delay));
		break;
	case SND_SOC_DAPM_PRE_PMD:
		s5m3500x_update_bits(s5m3500x, S5M3500X_018_PWAUTO_AD, APW_MIC2_MASK, 0x00);
		s5m3500x_remove_device_status(s5m3500x, DEVICE_AMIC3);
		break;
	case SND_SOC_DAPM_POST_PMD:
		s5m3500x_update_bits(s5m3500x, S5M3500X_111_CTRL_MIC2, SEL_MICIN2_MASK, SEL_MICIN2_MICIN2);
		s5m3500x_write(s5m3500x, S5M3500X_115_CTRL_MIC6, 0x22);
		s5m3500x_write(s5m3500x, S5M3500X_11B_CTRL_MIC12, 0xD0);
		s5m3500x_write(s5m3500x, S5M3500X_11C_CTRL_MIC13, 0x04);
		break;
	}
	return 0;
}

static int s5m3500x_dapm_mic3_on_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_dapm_kcontrol_component(kcontrol);
	struct s5m3500x_priv *s5m3500x = snd_soc_component_get_drvdata(codec);

	if (s5m3500x_check_device_status(s5m3500x, DEVICE_AMIC3))
		ucontrol->value.enumerated.item[0] = 1;
	else
		ucontrol->value.enumerated.item[0] = 0;
	return 0;
}

static int s5m3500x_dapm_mic3_on_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_dapm_kcontrol_component(kcontrol);
	struct snd_soc_dapm_context *dapm = snd_soc_dapm_kcontrol_dapm(kcontrol);
	struct s5m3500x_priv *s5m3500x = snd_soc_component_get_drvdata(codec);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	int value = ucontrol->value.enumerated.item[0];

	if (value > e->items - 1)
		return -EINVAL;

	if (value) {
		s5m3500x_add_device_status(s5m3500x, DEVICE_AMIC3);
		snd_soc_dapm_mixer_update_power(dapm, kcontrol, 1, NULL);
	} else {
		s5m3500x_remove_device_status(s5m3500x, DEVICE_AMIC3);
		snd_soc_dapm_mixer_update_power(dapm, kcontrol, 0, NULL);
	}

	dev_info(codec->dev, "%s : AMIC3 %s , status 0x%x\n", __func__, value ? "ON" : "OFF", s5m3500x->status);

	return 0;
}

static const struct snd_kcontrol_new mic3_on[] = {
	SOC_DAPM_ENUM_EXT("MIC3 On", s5m3500x_device_enable_enum,
					s5m3500x_dapm_mic3_on_get, s5m3500x_dapm_mic3_on_put),
};

/* Rx Devices */
/*Common Configuration for Rx*/
/*Control Clock & Reset, CP */
static int dac_ev(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *codec = snd_soc_dapm_to_component(w->dapm);
	struct s5m3500x_priv *s5m3500x = snd_soc_component_get_drvdata(codec);
	bool hp_on, ep_on, mic_on;

	hp_on = s5m3500x_check_device_status(s5m3500x, DEVICE_HP);
	ep_on = s5m3500x_check_device_status(s5m3500x, DEVICE_EP);
	mic_on = s5m3500x_check_device_status(s5m3500x, DEVICE_AMIC_ON);

	dev_info(codec->dev, "%s called, event = %d, status: 0x%x\n", __func__, event, s5m3500x->status);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* CLKGATE 0 */
		s5m3500x_update_bits(s5m3500x, S5M3500X_010_CLKGATE0,
			SEQ_CLK_GATE_MASK|AVC_CLK_GATE_MASK|CED_CLK_GATE_MASK|DAC_CLK_GATE_MASK,
			SEQ_CLK_GATE_ENABLE|AVC_CLK_GATE_ENABLE|CED_CLK_GATE_ENABLE|DAC_CLK_GATE_ENABLE);
		/* CLKGATE 1 */
		s5m3500x_update_bits(s5m3500x, S5M3500X_011_CLKGATE1,
			DAC_CIC_CGL_MASK,
			DAC_CIC_CGL_ENABLE);

		if(hp_on) {
			/* CLKGATE 0 */
			s5m3500x_update_bits(s5m3500x, S5M3500X_010_CLKGATE0, OVP_CLK_GATE_MASK, OVP_CLK_GATE_ENABLE);
			/* CLKGATE 1 */
			s5m3500x_update_bits(s5m3500x, S5M3500X_011_CLKGATE1, DAC_CIC_CGR_MASK,	DAC_CIC_CGR_ENABLE);
		}
			
		/* RESETB 0 */
		s5m3500x_update_bits(s5m3500x, S5M3500X_014_RESETB0,
			RSTB_DAC_DSM_MASK|DAC_RESETB_MASK, 0x00);
		s5m3500x_update_bits(s5m3500x, S5M3500X_014_RESETB0,
			RSTB_DAC_DSM_MASK|DAC_RESETB_MASK
			,RSTB_DAC_DSM_NORMAL|DAC_RESETB_NORMAL);
		break;
	case SND_SOC_DAPM_POST_PMU:
		break;
	case SND_SOC_DAPM_PRE_PMD:
		break;
	case SND_SOC_DAPM_POST_PMD:
		if(!hp_on) {
			/* CLKGATE 0 */
			s5m3500x_update_bits(s5m3500x, S5M3500X_010_CLKGATE0, OVP_CLK_GATE_MASK, 0x00);
			/* CLKGATE 1 */
			s5m3500x_update_bits(s5m3500x, S5M3500X_011_CLKGATE1, DAC_CIC_CGR_MASK,	0x00);
		}

		//All Rx Path are off
		if(!(hp_on||ep_on))
		{
			/* RESETB 0 */
			s5m3500x_update_bits(s5m3500x, S5M3500X_014_RESETB0,
				RSTB_DAC_DSM_MASK|DAC_RESETB_MASK, 0x00);
			/* CLKGATE 1 */
			s5m3500x_update_bits(s5m3500x, S5M3500X_011_CLKGATE1,
				DAC_CIC_CGL_MASK, 0x00);

			/* CLKGATE 0 */
			s5m3500x_update_bits(s5m3500x, S5M3500X_010_CLKGATE0,
				DAC_CLK_GATE_MASK, 0x00);

			/* CLKGATE 0 */
			if(!mic_on)
				s5m3500x_update_bits(s5m3500x, S5M3500X_010_CLKGATE0,
					SEQ_CLK_GATE_MASK|AVC_CLK_GATE_MASK|CED_CLK_GATE_MASK, 0x00);
		}
		break;
	}
	return 0;
}

/*Specific Configuration for Rx*/
/* EP Device */
static int epdrv_ev(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *codec = snd_soc_dapm_to_component(w->dapm);
	struct s5m3500x_priv *s5m3500x = snd_soc_component_get_drvdata(codec);

	dev_info(codec->dev, "%s called, event = %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		s5m3500x_write(s5m3500x, S5M3500X_107_DA_CLK1, 0x01);

		s5m3500x_write(s5m3500x, S5M3500X_108_DA_CP0, 0xA0);
		s5m3500x_write(s5m3500x, S5M3500X_135_CTRL_IDAC4, 0x20);
		s5m3500x_write(s5m3500x, S5M3500X_136_CTRL_IDAC5, 0x03);
		break;
	case SND_SOC_DAPM_POST_PMU:
		s5m3500x_update_bits(s5m3500x, S5M3500X_019_PWAUTO_DA, APW_EP_MASK, APW_EP_ENABLE);
		s5m3500x_add_device_status(s5m3500x, DEVICE_EP);
		s5m3500x_usleep(1000);
		s5m3500x_dac_soft_mute(s5m3500x, DAC_MUTE_ALL, false);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		s5m3500x_dac_soft_mute(s5m3500x, DAC_MUTE_ALL, true);
		s5m3500x_update_bits(s5m3500x, S5M3500X_019_PWAUTO_DA, APW_EP_MASK, 0x00);
		s5m3500x_remove_device_status(s5m3500x, DEVICE_EP);
		s5m3500x_usleep(1000);
		break;
	case SND_SOC_DAPM_POST_PMD:
		s5m3500x_write(s5m3500x, S5M3500X_107_DA_CLK1, 0x91);
		s5m3500x_write(s5m3500x, S5M3500X_108_DA_CP0, 0xA0);
		s5m3500x_write(s5m3500x, S5M3500X_135_CTRL_IDAC4, 0x00);
		s5m3500x_write(s5m3500x, S5M3500X_136_CTRL_IDAC5, 0x06);
		break;
	}

	return 0;
}

static int s5m3500x_dapm_ep_on_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_dapm_kcontrol_component(kcontrol);
	struct s5m3500x_priv *s5m3500x = snd_soc_component_get_drvdata(codec);

	if (s5m3500x_check_device_status(s5m3500x, DEVICE_EP))
		ucontrol->value.enumerated.item[0] = 1;
	else
		ucontrol->value.enumerated.item[0] = 0;
	return 0;
}

static int s5m3500x_dapm_ep_on_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_dapm_kcontrol_component(kcontrol);
	struct snd_soc_dapm_context *dapm = snd_soc_dapm_kcontrol_dapm(kcontrol);
	struct s5m3500x_priv *s5m3500x = snd_soc_component_get_drvdata(codec);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	int value = ucontrol->value.enumerated.item[0];

	if (value > e->items - 1)
		return -EINVAL;

	if (value) {
		s5m3500x_add_device_status(s5m3500x, DEVICE_EP);
		snd_soc_dapm_mixer_update_power(dapm, kcontrol, 1, NULL);
	} else {
		s5m3500x_remove_device_status(s5m3500x, DEVICE_EP);
		snd_soc_dapm_mixer_update_power(dapm, kcontrol, 0, NULL);
	}

	dev_info(codec->dev, "%s : EP %s , status 0x%x\n", __func__, value ? "ON" : "OFF", s5m3500x->status);

	return 0;
}

static const struct snd_kcontrol_new ep_on[] = {
	SOC_DAPM_ENUM_EXT("EP On", s5m3500x_device_enable_enum,
					s5m3500x_dapm_ep_on_get, s5m3500x_dapm_ep_on_put),
};

/* Headphone Device */
static int hpdrv_ev(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *codec = snd_soc_dapm_to_component(w->dapm);
	struct s5m3500x_priv *s5m3500x = snd_soc_component_get_drvdata(codec);

	dev_info(codec->dev, "%s called, event = %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		s5m3500x_write(s5m3500x, S5M3500X_107_DA_CLK1, 0x90);
		s5m3500x_write(s5m3500x, S5M3500X_108_DA_CP0, 0xA0);
		s5m3500x_write(s5m3500x, S5M3500X_10E_DA_CP6, 0x24);
		s5m3500x_write(s5m3500x, S5M3500X_135_CTRL_IDAC4, 0x20);
		s5m3500x_write(s5m3500x, S5M3500X_136_CTRL_IDAC5, 0x06);

		switch (s5m3500x->playback_params.aifrate) {
		case SAMPLE_RATE_192KHZ:
			s5m3500x_write(s5m3500x, S5M3500X_13C_CTRL_HP5, 0x41);
			break;
		case SAMPLE_RATE_48KHZ:
		default:
			s5m3500x_write(s5m3500x, S5M3500X_13C_CTRL_HP5, 0x00);
			break;
		}
		s5m3500x_write(s5m3500x, S5M3500X_13F_CTRL_HP8, 0x22);
		s5m3500x_write(s5m3500x, S5M3500X_141_CTRL_HP10, 0x9A);
		s5m3500x_write(s5m3500x, S5M3500X_14E_CTRL_OVP1, 0x10);
		break;
	case SND_SOC_DAPM_POST_PMU:
		s5m3500x_update_bits(s5m3500x, S5M3500X_019_PWAUTO_DA, APW_HP_MASK, APW_HP_ENABLE);
		s5m3500x_add_device_status(s5m3500x, DEVICE_HP);
		s5m3500x_usleep(1000);
		s5m3500x_dac_soft_mute(s5m3500x, DAC_MUTE_ALL, false);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		s5m3500x_dac_soft_mute(s5m3500x, DAC_MUTE_ALL, true);
		s5m3500x_update_bits(s5m3500x, S5M3500X_019_PWAUTO_DA, APW_HP_MASK, 0x00);
		s5m3500x_remove_device_status(s5m3500x, DEVICE_HP);
		s5m3500x_usleep(1000);
		break;
	case SND_SOC_DAPM_POST_PMD:
		s5m3500x_write(s5m3500x, S5M3500X_107_DA_CLK1, 0x91);
		s5m3500x_write(s5m3500x, S5M3500X_108_DA_CP0, 0xA0);
		s5m3500x_write(s5m3500x, S5M3500X_10E_DA_CP6, 0x25);
		s5m3500x_write(s5m3500x, S5M3500X_135_CTRL_IDAC4, 0x00);
		s5m3500x_write(s5m3500x, S5M3500X_136_CTRL_IDAC5, 0x06);
		s5m3500x_write(s5m3500x, S5M3500X_13C_CTRL_HP5, 0x04);
		s5m3500x_write(s5m3500x, S5M3500X_13F_CTRL_HP8, 0x32);
		s5m3500x_write(s5m3500x, S5M3500X_141_CTRL_HP10, 0x2A);
		s5m3500x_write(s5m3500x, S5M3500X_14E_CTRL_OVP1, 0x00);
		break;
	}
	return 0;
}

static int s5m3500x_dapm_hp_on_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_dapm_kcontrol_component(kcontrol);
	struct s5m3500x_priv *s5m3500x = snd_soc_component_get_drvdata(codec);

	if (s5m3500x_check_device_status(s5m3500x, DEVICE_HP))
		ucontrol->value.enumerated.item[0] = 1;
	else
		ucontrol->value.enumerated.item[0] = 0;
	return 0;
}

static int s5m3500x_dapm_hp_on_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *codec = snd_soc_dapm_kcontrol_component(kcontrol);
	struct snd_soc_dapm_context *dapm = snd_soc_dapm_kcontrol_dapm(kcontrol);
	struct s5m3500x_priv *s5m3500x = snd_soc_component_get_drvdata(codec);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	int value = ucontrol->value.enumerated.item[0];

	if (value > e->items - 1)
		return -EINVAL;

	if (value) {
		s5m3500x_add_device_status(s5m3500x, DEVICE_HP);
		snd_soc_dapm_mixer_update_power(dapm, kcontrol, 1, NULL);
	} else {
		s5m3500x_remove_device_status(s5m3500x, DEVICE_HP);
		snd_soc_dapm_mixer_update_power(dapm, kcontrol, 0, NULL);
	}

	dev_info(codec->dev, "%s : Headphone %s , status 0x%x\n", __func__, value ? "ON" : "OFF", s5m3500x->status);

	return 0;
}

static const struct snd_kcontrol_new hp_on[] = {
	SOC_DAPM_ENUM_EXT("HP On", s5m3500x_device_enable_enum,
					s5m3500x_dapm_hp_on_get, s5m3500x_dapm_hp_on_put),
};

static const struct snd_soc_dapm_widget s5m3500x_dapm_widgets[] = {
	/*
	 * ADC(Tx) dapm widget
	 */
	SND_SOC_DAPM_INPUT("IN1L"),
	SND_SOC_DAPM_INPUT("IN2L"),
	SND_SOC_DAPM_INPUT("IN3L"),

	SND_SOC_DAPM_SUPPLY("VMID", SND_SOC_NOPM, 0, 0, vmid_ev,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_PGA_E("MIC1_PGA", SND_SOC_NOPM, 0, 0, NULL, 0, mic1_pga_ev,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("MIC2_PGA", SND_SOC_NOPM, 0, 0, NULL, 0, mic2_pga_ev,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("MIC3_PGA", SND_SOC_NOPM, 0, 0, NULL, 0, mic3_pga_ev,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_SWITCH("MIC1", SND_SOC_NOPM, 0, 0, mic1_on),
	SND_SOC_DAPM_SWITCH("MIC2", SND_SOC_NOPM, 0, 0, mic2_on),
	SND_SOC_DAPM_SWITCH("MIC3", SND_SOC_NOPM, 0, 0, mic3_on),

	SND_SOC_DAPM_MUX("INP_SEL_L", SND_SOC_NOPM, 0, 0, &s5m3500x_inp_sel_l),
	SND_SOC_DAPM_MUX("INP_SEL_R", SND_SOC_NOPM, 0, 0, &s5m3500x_inp_sel_r),

	SND_SOC_DAPM_ADC_E("ADC", "AIF Capture", SND_SOC_NOPM, 0, 0, adc_ev,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	/*
	 * DAC(Rx) dapm widget
	 */
	SND_SOC_DAPM_DAC_E("DAC", "AIF Playback", SND_SOC_NOPM, 0, 0, dac_ev,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_OUT_DRV_E("EPDRV", SND_SOC_NOPM, 0, 0, NULL, 0, epdrv_ev,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_OUT_DRV_E("HPDRV", SND_SOC_NOPM, 0, 0, NULL, 0, hpdrv_ev,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),


	SND_SOC_DAPM_SWITCH("EP", SND_SOC_NOPM, 0, 0, ep_on),
	SND_SOC_DAPM_SWITCH("HP", SND_SOC_NOPM, 0, 0, hp_on),

	SND_SOC_DAPM_OUTPUT("EPOUTN"),
	SND_SOC_DAPM_OUTPUT("HPOUTLN"),
};

/*
 * snd_soc_dapm_route set
 */
static const struct snd_soc_dapm_route s5m3500x_dapm_routes[] = {
	/*
	 * ADC(Tx) dapm route
	 */
	{"MIC1_PGA", NULL, "IN1L"},
	{"MIC1_PGA", NULL, "VMID"},
	{"MIC1", "MIC1 On", "MIC1_PGA"},

	{"MIC2_PGA", NULL, "IN2L"},
	{"MIC2_PGA", NULL, "VMID"},
	{"MIC2", "MIC2 On", "MIC2_PGA"},

	{"MIC3_PGA", NULL, "IN3L"},
	{"MIC3_PGA", NULL, "VMID"},
	{"MIC3", "MIC3 On", "MIC3_PGA"},

	{"INP_SEL_L", "AMIC_L ADC_L", "MIC1"},
	{"INP_SEL_L", "AMIC_R ADC_L", "MIC2"},
	{"INP_SEL_L", "AMIC_R ADC_L", "MIC3"},

	{"INP_SEL_R", "AMIC_L ADC_R", "MIC1"},
	{"INP_SEL_R", "AMIC_R ADC_R", "MIC2"},
	{"INP_SEL_R", "AMIC_R ADC_R", "MIC3"},

	{"ADC", NULL, "INP_SEL_L"},
	{"ADC", NULL, "INP_SEL_R"},

	{"AIF Capture", NULL, "ADC"},

	/*
	 * DAC(Rx) dapm route
	 */
	{"DAC", NULL, "AIF Playback"},

	{"EPDRV", NULL, "DAC"},
	{"EP", "EP On", "EPDRV"},
	{"EPOUTN", NULL, "EP"},

	{"HPDRV", NULL, "DAC"},
	{"HP", "HP On", "HPDRV"},
	{"HPOUTLN", NULL, "HP"},
};

/*
 * snd_soc_dai_driver set
 */
static int s5m3500x_dai_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct snd_soc_component *codec = dai->component;
	struct s5m3500x_priv *s5m3500x = snd_soc_component_get_drvdata(codec);

	dev_info(codec->dev, "%s called. fmt: %d\n", __func__, fmt);

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		dev_info(codec->dev, "%s : SND_SOC_DAIFMT_I2S\n", __func__);
		s5m3500x_update_bits(s5m3500x, S5M3500X_020_IF_FORM1,
				I2S_DF_MASK, I2S_DF_I2S_F);
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		dev_info(codec->dev, "%s : SND_SOC_DAIFMT_LEFT_J\n", __func__);
		s5m3500x_update_bits(s5m3500x, S5M3500X_020_IF_FORM1,
				I2S_DF_MASK, I2S_DF_LEFT_J_F);
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		dev_info(codec->dev, "%s : SND_SOC_DAIFMT_RIGHT_J\n", __func__);
		s5m3500x_update_bits(s5m3500x, S5M3500X_020_IF_FORM1,
				I2S_DF_MASK, I2S_DF_RIGHT_J_F);
		break;
	case SND_SOC_DAIFMT_DSP_A:
		dev_info(codec->dev, "%s : SND_SOC_DAIFMT_DSP_A\n", __func__);
		s5m3500x_update_bits(s5m3500x, S5M3500X_020_IF_FORM1,
				I2S_DF_MASK, I2S_DF_PCM_S_F);
		break;
	}

	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		dev_info(codec->dev, "%s : SND_SOC_DAIFMT_NB_NF\n", __func__);
		s5m3500x_update_bits(s5m3500x, S5M3500X_020_IF_FORM1,
				BCLK_POL_MASK|LRCLK_POL_MASK, BCLK_POL_NORMAL|LRCLK_POL_NORMAL);
		break;
	case SND_SOC_DAIFMT_NB_IF:
		dev_info(codec->dev, "%s : SND_SOC_DAIFMT_NB_IF\n", __func__);
		s5m3500x_update_bits(s5m3500x, S5M3500X_020_IF_FORM1,
				BCLK_POL_MASK|LRCLK_POL_MASK, BCLK_POL_NORMAL|LRCLK_POL_INVERT);
		break;
	case SND_SOC_DAIFMT_IB_NF:
		dev_info(codec->dev, "%s : SND_SOC_DAIFMT_IB_NF\n", __func__);
		s5m3500x_update_bits(s5m3500x, S5M3500X_020_IF_FORM1,
				BCLK_POL_MASK|LRCLK_POL_MASK, BCLK_POL_INVERT|LRCLK_POL_NORMAL);
		break;
	case SND_SOC_DAIFMT_IB_IF:
		dev_info(codec->dev, "%s : SND_SOC_DAIFMT_IB_IF\n", __func__);
		s5m3500x_update_bits(s5m3500x, S5M3500X_020_IF_FORM1,
				BCLK_POL_MASK|LRCLK_POL_MASK, BCLK_POL_INVERT|LRCLK_POL_INVERT);
		break;
	}

	return 0;
}

static const unsigned int s5m3500x_src_rates[] = {
	48000, 192000,
};

static const struct snd_pcm_hw_constraint_list s5m3500x_constraints = {
	.count = ARRAY_SIZE(s5m3500x_src_rates),
	.list = s5m3500x_src_rates,
};

static int s5m3500x_dai_startup(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct snd_soc_component *codec = dai->component;
	struct s5m3500x_priv *s5m3500x = snd_soc_component_get_drvdata(codec);
	int ret = 0;

	dev_info(s5m3500x->dev, "%s : startup for %s\n", __func__, substream->stream ? "Capture" : "Playback");

	if (substream->runtime)
		ret = snd_pcm_hw_constraint_list(substream->runtime, 0,
						SNDRV_PCM_HW_PARAM_RATE, &s5m3500x_constraints);

	if (ret < 0)
		dev_err(s5m3500x->dev, "%s : Unsupported sample rates", __func__);

	return 0;
}

/*
 * capture_hw_params() - Register setting for capture
 *
 * @codec: SoC audio codec device
 * @cur_aifrate: current sample rate
 *
 * Desc: Set codec register related sample rate format before capture.
 */
static void capture_hw_params(struct snd_soc_component *codec,
		unsigned int cur_aifrate)
{
	struct s5m3500x_priv *s5m3500x = snd_soc_component_get_drvdata(codec);

	dev_info(codec->dev, "%s called. priv_aif: %d, cur_aif %d\n",
			__func__, s5m3500x->capture_params.aifrate, cur_aifrate);
	s5m3500x_adc_digital_mute(s5m3500x, ADC_MUTE_ALL, true);

	switch (cur_aifrate) {
	case SAMPLE_RATE_192KHZ:
		s5m3500x_update_bits(s5m3500x, S5M3500X_029_IF_FORM6, ADC_OUT_FORMAT_SEL_MASK, ADC_OUT_FORMAT_SEL_FOTMAT2);
		s5m3500x_update_bits(s5m3500x, S5M3500X_030_ADC1, FS_SEL_MASK, FS_SEL_UHQA);
		s5m3500x_update_bits(s5m3500x, S5M3500X_033_ADC3, CP_TYPE_MASK, CP_TYPE_UHQA_WITH_LPF);
		break;
	case SAMPLE_RATE_48KHZ:
	default:
		s5m3500x_update_bits(s5m3500x, S5M3500X_029_IF_FORM6, ADC_OUT_FORMAT_SEL_MASK, ADC_OUT_FORMAT_SEL_FOTMAT1);
		s5m3500x_update_bits(s5m3500x, S5M3500X_030_ADC1, FS_SEL_MASK, FS_SEL_NORMAL);
		s5m3500x_update_bits(s5m3500x, S5M3500X_033_ADC3, CP_TYPE_MASK, CP_TYPE_NORMAL);
		break;
	}
}

/*
 * playback_hw_params() - Register setting for playback
 *
 * @codec: SoC audio codec device
 * @cur_aifrate: current sample rate
 *
 * Desc: Set codec register related sample rate format before playback.
 */
static void playback_hw_params(struct snd_soc_component *codec,
		unsigned int cur_aifrate)
{
	struct s5m3500x_priv *s5m3500x = snd_soc_component_get_drvdata(codec);

	dev_info(codec->dev, "%s called. priv_aif: %d, cur_aif %d\n",
			__func__, s5m3500x->playback_params.aifrate, cur_aifrate);

	switch (cur_aifrate) {
	case SAMPLE_RATE_192KHZ:
		s5m3500x_update_bits(s5m3500x, S5M3500X_040_PLAY_MODE, PLAYMODE_SEL_MASK, PLAYMODE_SEL_192KHZ);
		s5m3500x_update_bits(s5m3500x, S5M3500X_016_CLK_MODE_SEL0, DAC_FSEL_MASK, DAC_FSEL_192KHZ);

		s5m3500x_write(s5m3500x, S5M3500X_047_TRIM_DAC0, 0xFF);
		s5m3500x_write(s5m3500x, S5M3500X_048_TRIM_DAC1, 0x1E);

		s5m3500x_write(s5m3500x, S5M3500X_050_AVC1, 0x07);
		s5m3500x_write(s5m3500x, S5M3500X_053_AVC4, 0xA0);

		s5m3500x_update_bits(s5m3500x, S5M3500X_071_AVC34, 0x3F, 0xC7);
		s5m3500x_write(s5m3500x, S5M3500X_072_AVC35, 0x3A);
		s5m3500x_update_bits(s5m3500x, S5M3500X_073_AVC36, 0x3F, 0xC4);
		s5m3500x_write(s5m3500x, S5M3500X_074_AVC37, 0x3A);
		s5m3500x_update_bits(s5m3500x, S5M3500X_07B_AVC44, 0xE0, 0x40);
		s5m3500x_update_bits(s5m3500x, S5M3500X_07E_AVC45, 0x07, 0x02);
		
		break;
	case SAMPLE_RATE_48KHZ:
	default:
		s5m3500x_update_bits(s5m3500x, S5M3500X_040_PLAY_MODE, PLAYMODE_SEL_MASK, PLAYMODE_SEL_48KHZ);
		s5m3500x_update_bits(s5m3500x, S5M3500X_016_CLK_MODE_SEL0, DAC_FSEL_MASK, DAC_FSEL_48KHZ);

		s5m3500x_write(s5m3500x, S5M3500X_047_TRIM_DAC0, 0xF7);
		s5m3500x_write(s5m3500x, S5M3500X_048_TRIM_DAC1, 0x4F);

		s5m3500x_write(s5m3500x, S5M3500X_050_AVC1, 0x07);
		s5m3500x_write(s5m3500x, S5M3500X_053_AVC4, 0xA0);

		s5m3500x_update_bits(s5m3500x, S5M3500X_071_AVC34, 0x3F, 0x18);
		s5m3500x_write(s5m3500x, S5M3500X_072_AVC35, 0xDD);
		s5m3500x_update_bits(s5m3500x, S5M3500X_073_AVC36, 0x3F, 0x17);
		s5m3500x_write(s5m3500x, S5M3500X_074_AVC37, 0xE9);
		s5m3500x_update_bits(s5m3500x, S5M3500X_07B_AVC44, 0xE0, 0x00);
		s5m3500x_update_bits(s5m3500x, S5M3500X_07E_AVC45, 0x07, 0x00);
		break;
	}
}


/*
 *set i2s data length and Sampling frequency
 * XFS = Data Length * Channels
 */
static void s5m3500x_set_i2s_configuration(struct s5m3500x_priv *s5m3500x, unsigned int width, unsigned int channels)
{
	switch (width) {
	case BIT_RATE_16:
		/* I2S 16bit Set */
		s5m3500x_update_bits(s5m3500x, S5M3500X_021_IF_FORM2, I2S_DL_MASK, I2S_DL_16BIT);

		/* I2S Channel Set */
		switch (channels) {
		case CHANNEL_2:
		default:
			s5m3500x_update_bits(s5m3500x, S5M3500X_022_IF_FORM3, I2S_XFS_MASK, I2S_XFS_32FS);
			break;
		}
		break;
	case BIT_RATE_24:
		/* I2S 24bit Set */
		s5m3500x_update_bits(s5m3500x, S5M3500X_021_IF_FORM2, I2S_DL_MASK, I2S_DL_24BIT);

		/* I2S Channel Set */
		switch (channels) {
		case CHANNEL_2:
		default:
			s5m3500x_update_bits(s5m3500x, S5M3500X_022_IF_FORM3, I2S_XFS_MASK, I2S_XFS_64FS);
			break;
		}
		break;
	case BIT_RATE_32:
		/* I2S 32bit Set */
		s5m3500x_update_bits(s5m3500x, S5M3500X_021_IF_FORM2, I2S_DL_MASK, I2S_DL_32BIT);

		/* I2S Channel Set */
		switch (channels) {
		case CHANNEL_2:
		default:
			s5m3500x_update_bits(s5m3500x, S5M3500X_022_IF_FORM3, I2S_XFS_MASK, I2S_XFS_64FS);
			break;
		}
		break;
	default:
		dev_err(s5m3500x->dev, "%s: bit rate error!\n", __func__);
		/* I2S 16bit Set */
		s5m3500x_update_bits(s5m3500x, S5M3500X_021_IF_FORM2, I2S_DL_MASK, I2S_DL_16BIT);

		/* I2S Channel Set */
		switch (channels) {
		case CHANNEL_2:
		default:
			s5m3500x_update_bits(s5m3500x, S5M3500X_022_IF_FORM3, I2S_XFS_MASK, I2S_XFS_32FS);
			break;
		}
		break;
	}
}


static int s5m3500x_dai_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params,
		struct snd_soc_dai *dai)
{
	struct snd_soc_component *codec = dai->component;
	struct s5m3500x_priv *s5m3500x = snd_soc_component_get_drvdata(codec);
	unsigned int cur_aifrate, width, channels;

	/* Get params */
	cur_aifrate = params_rate(params);
	width = params_width(params);
	channels = params_channels(params);

	dev_info(codec->dev, "(%s) %s called. aifrate: %d, width: %d, channels: %d\n",
			substream->stream ? "C" : "P", __func__, cur_aifrate, width, channels);

	s5m3500x_set_i2s_configuration(s5m3500x, width, channels);

	if(cur_aifrate == SAMPLE_RATE_48KHZ)
		s5m3500x_update_bits(s5m3500x, S5M3500X_02F_GPIO_ST, SDOUT_ST_MASK, SDOUT_ST_4MA);
	else
		s5m3500x_update_bits(s5m3500x, S5M3500X_02F_GPIO_ST, SDOUT_ST_MASK, SDOUT_ST_8MA);

	if (substream->stream) {
		capture_hw_params(codec, cur_aifrate);
		s5m3500x->capture_params.aifrate = cur_aifrate;
		s5m3500x->capture_params.width = width;
		s5m3500x->capture_params.channels = channels;
	} else {
		playback_hw_params(codec, cur_aifrate);
		s5m3500x->playback_params.aifrate = cur_aifrate;
		s5m3500x->playback_params.width = width;
		s5m3500x->playback_params.channels = channels;
	}

	return 0;
}

static void s5m3500x_dai_shutdown(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct snd_soc_component *codec = dai->component;
	dev_info(codec->dev, "(%s) %s completed\n", substream->stream ? "C" : "P", __func__);
}

static const struct snd_soc_dai_ops s5m3500x_dai_ops = {
	.set_fmt = s5m3500x_dai_set_fmt,
	.startup = s5m3500x_dai_startup,
	.hw_params = s5m3500x_dai_hw_params,
	.shutdown = s5m3500x_dai_shutdown,
};

#define S5M3500X_RATES		SNDRV_PCM_RATE_8000_192000
#define S5M3500X_FORMATS	(SNDRV_PCM_FMTBIT_S16_LE | \
							SNDRV_PCM_FMTBIT_S20_3LE | \
							SNDRV_PCM_FMTBIT_S24_LE  | \
							SNDRV_PCM_FMTBIT_S32_LE)

static struct snd_soc_dai_driver s5m3500x_dai[] = {
	{
		.name = "s5m3500x-aif",
		.id = 1,
		.playback = {
			.stream_name = "AIF Playback",
			.channels_min = 1,
			.channels_max = 8,
			.rates = S5M3500X_RATES,
			.formats = S5M3500X_FORMATS,
		},
		.capture = {
			.stream_name = "AIF Capture",
			.channels_min = 1,
			.channels_max = 8,
			.rates = S5M3500X_RATES,
			.formats = S5M3500X_FORMATS,
		},
		.ops = &s5m3500x_dai_ops,
		.symmetric_rate = 1,
	},
};

static void s5m3500x_i2c_parse_dt(struct s5m3500x_priv *s5m3500x)
{
	struct device *dev = s5m3500x->dev;
	struct device_node *np = dev->of_node;
	unsigned int jack_type;

	/* RESETB gpio */
	s5m3500x->resetb_gpio = of_get_named_gpio(np, "s5m3500x-resetb", 0);
	if (s5m3500x->resetb_gpio < 0)
		dev_err(dev, "%s: cannot find resetb gpio in the dt\n", __func__);
	else
		dev_info(dev, "%s: resetb gpio = %d\n", __func__, s5m3500x->resetb_gpio);

	/* Always Power On*/
	s5m3500x->power_always_on = of_property_read_bool(np, "s5m3500x-power-always-on");
	dev_info(dev, "%s: power_always_on = %d\n", __func__, s5m3500x->power_always_on);

	/*
	 * Setting Jack type
	 * 0: 3.5 Pi
	 */
	if (!of_property_read_u32(dev->of_node, "jack-type", &jack_type))
		s5m3500x->jack_type = jack_type;
	else
		s5m3500x->jack_type = JACK;

	dev_info(dev, "Jack type: %s\n", s5m3500x->jack_type ? "Unsupported" : "3.5 PI");
}

static void s5m3500x_power_configuration(struct s5m3500x_priv *s5m3500x)
{
	/* Setting RESETB gpio */
	if (s5m3500x->resetb_gpio > 0) {
		if (gpio_request(s5m3500x->resetb_gpio, "s5m3500x_resetb_gpio") < 0)
			dev_err(s5m3500x->dev, "%s: Request for %d GPIO failed\n", __func__, (int)s5m3500x->resetb_gpio);
		/* turn off for default */
		if (gpio_direction_output(s5m3500x->resetb_gpio, 0) < 0)
			dev_err(s5m3500x->dev, "%s: GPIO direction to output failed!\n", __func__);
	}

	/* register codec power */
	s5m3500x->vdd_1_8v = devm_regulator_get(s5m3500x->dev, "vdd_ldo30");
	if (IS_ERR(s5m3500x->vdd_1_8v))
		dev_warn(s5m3500x->dev, "failed to get regulator vdd %ld\n", PTR_ERR(s5m3500x->vdd_1_8v));
}

/* Control external Regulator and GPIO for power supplier */
static void s5m3500x_power_supplier_enable(struct s5m3500x_priv *s5m3500x, bool enable)
{
	unsigned int ret = 0;
	if (enable) {
		if (!IS_ERR(s5m3500x->vdd_1_8v))
			if (!regulator_is_enabled(s5m3500x->vdd_1_8v))
				ret = regulator_enable(s5m3500x->vdd_1_8v);

		/* set resetb gpio high */
		if (s5m3500x->resetb_gpio >= 0)
			if (gpio_direction_output(s5m3500x->resetb_gpio, 1) < 0)
				dev_err(s5m3500x->dev, "%s: resetb_gpio GPIO direction to output failed!\n", __func__);
	} else {
		/* set resetb gpio low */
		if (s5m3500x->resetb_gpio >= 0)
			if (gpio_direction_output(s5m3500x->resetb_gpio, 0) < 0)
				dev_err(s5m3500x->dev, "%s: GPIO direction to output failed!\n", __func__);

		if (!IS_ERR(s5m3500x->vdd_1_8v))
			if (regulator_is_enabled(s5m3500x->vdd_1_8v))
				ret = regulator_disable(s5m3500x->vdd_1_8v);
	}
	dev_info(s5m3500x->dev, "%s : %d %d %d\n", __func__, IS_ERR(s5m3500x->vdd_1_8v),regulator_is_enabled(s5m3500x->vdd_1_8v),ret);
}

/* Register is set on only HW Register */
/* It doesn't need to sync up with cache register */
static void s5m3500x_codec_power_enable(struct s5m3500x_priv *s5m3500x, bool enable)
{
	dev_info(s5m3500x->dev, "%s : enable %d\n", __func__, enable);
	if (enable) {
		/* power supplier */
		s5m3500x_power_supplier_enable(s5m3500x, true);
		/* PDB_BGR */
		s5m3500x_update_bits(s5m3500x, S5M3500X_100_CTRL_REF1, 0x80,0x80);
		/* PDB_IBIAS */
		s5m3500x_update_bits(s5m3500x, S5M3500X_101_CTRL_REF2, 0x20,0x20);
		/* PDB_ALDO */
		s5m3500x_update_bits(s5m3500x, S5M3500X_130_CTRL_RXREF1, PDB_ALDO_MASK,PDB_ALDO_NORMAL);
		/* 3ms HW Delay */
		s5m3500x_usleep(3000);
		/* EN_REF_LP_DLDO */
		s5m3500x_update_bits(s5m3500x, S5M3500X_100_CTRL_REF1, 0x1,0x01);
	} else {
		/* EN_REF_LP_DLDO */
		s5m3500x_update_bits(s5m3500x, S5M3500X_100_CTRL_REF1, 0x1,0x00);
		/* PDB_ALDO */
		s5m3500x_update_bits(s5m3500x, S5M3500X_130_CTRL_RXREF1, PDB_ALDO_MASK,0x00);
		/* 3ms HW Delay */
		s5m3500x_usleep(3000);
		/* PDB_IBIAS */
		s5m3500x_update_bits(s5m3500x, S5M3500X_101_CTRL_REF2, 0x20,0x00);
		/* PDB_BGR */
		s5m3500x_update_bits(s5m3500x, S5M3500X_100_CTRL_REF1, 0x80,0x00);
		/* power supplier */
		s5m3500x_power_supplier_enable(s5m3500x, false);
	}
}

void s5m3500x_update_otp_register(struct s5m3500x_priv *s5m3500x)
{
	int i = 0, array_size = ARRAY_SIZE(s5m3500x_reg_defaults), value = 0;

	pr_info("%s: enter\n", __func__);

	/* Load OTP From Memory to Register */
	/* OTP Load On */
	s5m3500x_update_bits(s5m3500x, S5M3500X_010_CLKGATE0, COM_CLK_GATE_MASK,COM_CLK_GATE_ENABLE);
	s5m3500x_update_bits(s5m3500x, S5M3500X_013_CLKGATE3, 0x80,0x80);
	s5m3500x_update_bits(s5m3500x, S5M3500X_01A_COM_OTP_TEST, 0x01,0x01);
	s5m3500x_update_bits(s5m3500x, S5M3500X_01A_COM_OTP_TEST, 0xC0,0xC0);
	/* 15ms HW Delay */
	s5m3500x_usleep(15000);
	/* OTP Load Off */
	s5m3500x_update_bits(s5m3500x, S5M3500X_01A_COM_OTP_TEST, 0xC0,0x00);
	s5m3500x_update_bits(s5m3500x, S5M3500X_01A_COM_OTP_TEST, 0x01,0x00);
	s5m3500x_update_bits(s5m3500x, S5M3500X_013_CLKGATE3, 0x80,0x00);

	/* Read OTP Register from HW and update reg_defaults. */
	for (i = 0; i < array_size; i++) {
		if (s5m3500x_reg_defaults[i].reg >= S5M3500X_200_HPL_OFFSET0) {
			if (!s5m3500x_read_only_hardware(s5m3500x, s5m3500x_reg_defaults[i].reg, &value))
				s5m3500x_reg_defaults[i].def = value;
		}
	}
}


/*
 * apply register initialize by patch
 * patch is only updated hw register, so cache need to be updated.
 */
int s5m3500x_regmap_register_patch(struct s5m3500x_priv *s5m3500x)
{
	struct device *dev = s5m3500x->dev;
	int ret = 0, size = ARRAY_SIZE(s5m3500x_init_patch);

	if(size == 0)
		return -1;

	/* register patch for playback / capture initialze */
	ret = regmap_register_patch(s5m3500x->regmap, s5m3500x_init_patch, size);
	if (ret < 0) {
		dev_err(dev, "Failed to apply s5m3500x_init_patch %d\n", ret);
		return ret;
	}

	/* update reg_defaults with registered patch */
	s5m3500x_update_reg_defaults(s5m3500x_init_patch, size);
	return ret;
}

static void s5m3500x_register_initialize(struct s5m3500x_priv *s5m3500x)
{
	/* update otp initial registers on cache */
	s5m3500x_update_otp_register(s5m3500x);

	/* update initial registers on cache and hw registers */
	s5m3500x_regmap_register_patch(s5m3500x);

	/* reinit regmap cache because reg_defaults are updated*/
	s5m3500x_regmap_reinit_cache(s5m3500x);
}

static void s5m3500x_adc_mute_work(struct work_struct *work)
{
	struct s5m3500x_priv *s5m3500x =
		container_of(work, struct s5m3500x_priv, adc_mute_work.work);

	unsigned int val = 0;
	unsigned int adc_l_status, adc_r_status;

	dev_info(s5m3500x->dev, "%s called, status: 0x%x\n", __func__, s5m3500x->status);

	if (s5m3500x_check_device_status(s5m3500x, DEVICE_CAPTURE_ON)) {
		s5m3500x_read(s5m3500x, S5M3500X_031_ADC2, &val);
		adc_l_status = (val & SEL_INPL_MASK) >> SEL_INPL_SHIFT;
		adc_r_status = (val & SEL_INPR_MASK) >> SEL_INPR_SHIFT;

		if((adc_l_status != ADC_INP_SEL_ZERO) && (adc_r_status != ADC_INP_SEL_ZERO))
			s5m3500x_adc_digital_mute(s5m3500x, ADC_MUTE_ALL, false);
		else if (adc_l_status != ADC_INP_SEL_ZERO)
			s5m3500x_adc_digital_mute(s5m3500x, ADC_MUTE_CH_L, false);
		else if (adc_r_status != ADC_INP_SEL_ZERO)
			s5m3500x_adc_digital_mute(s5m3500x, ADC_MUTE_CH_R, false);
	}
}

static char *get_adc_channel_name(unsigned int channel)
{
	switch(channel)
	{
	case ADC_MUTE_CH_L:
		return "ADC Left";
	case ADC_MUTE_CH_R:
		return "ADC Right";
	case ADC_MUTE_ALL:
	default:
		return "ADC All";
	}
	return "None";
}

/*
 * s5m3500x_adc_digital_mute() - Set ADC digital Mute
 *
 * @codec: SoC audio codec device
 * @channel: Digital mute control for ADC channel
 * @on: mic mute is true, mic unmute is false
 *
 * Desc: When ADC path turn on, analog block noise can be recorded.
 * For remove this, ADC path was muted always except that it was used.
 */
void s5m3500x_adc_digital_mute(struct s5m3500x_priv *s5m3500x,
		unsigned int channel, bool on)
{
	struct device *dev = s5m3500x->dev;
	unsigned int mask, value;

	mutex_lock(&s5m3500x->mute_lock);

	switch(channel)
	{
	case ADC_MUTE_CH_L:
		mask = ADC_MUTEL_MASK;
		value = ADC_MUTEL_ENABLE;
		break;
	case ADC_MUTE_CH_R:
		mask = ADC_MUTER_MASK;
		value = ADC_MUTER_ENABLE;
		break;
	case ADC_MUTE_ALL:
	default:
		mask = ADC_MUTEL_MASK|ADC_MUTER_MASK;
		value = ADC_MUTEL_ENABLE|ADC_MUTER_ENABLE;
		break;
	}

	if (on)
		s5m3500x_update_bits(s5m3500x, S5M3500X_030_ADC1, mask, value);
	else
		s5m3500x_update_bits(s5m3500x, S5M3500X_030_ADC1, mask, 0);

	mutex_unlock(&s5m3500x->mute_lock);

	dev_info(dev, "%s called, Channel %s %s work done.\n", __func__, get_adc_channel_name(channel), on ? "Mute" : "Unmute");
}
EXPORT_SYMBOL_GPL(s5m3500x_adc_digital_mute);

static char *get_dac_channel_name(unsigned int channel)
{
	switch(channel)
	{
	case DAC_MUTE_CH_L:
		return "DAC Left";
	case DAC_MUTE_CH_R:
		return "DAC Right";
	case DAC_MUTE_ALL:
	default:
		return "DAC All";
	}
	return "None";
}

/*
 * DAC(Rx) functions
 */
/*
 * s5m3500x_dac_soft_mute() - Set DAC soft mute
 *
 * @codec: SoC audio codec device
 * @channel: Soft mute control for DAC channel
 * @on: dac mute is true, dac unmute is false
 *
 * Desc: When DAC path turn on, analog block noise can be played.
 * For remove this, DAC path was muted always except that it was used.
 */
static void s5m3500x_dac_soft_mute(struct s5m3500x_priv *s5m3500x,
		unsigned int channel, bool on)
{
	struct device *dev = s5m3500x->dev;
	unsigned int mask, value;

	mutex_lock(&s5m3500x->mute_lock);

	switch(channel)
	{
	case DAC_MUTE_CH_L:
		mask = DA_SMUTEL_MASK;
		value = DA_SMUTEL_ENABLE;
		break;
	case DAC_MUTE_CH_R:
		mask = DA_SMUTER_MASK;
		value = DA_SMUTER_ENABLE;
		break;
	case DAC_MUTE_ALL:
	default:
		mask = DA_SMUTEL_MASK|DA_SMUTER_MASK;
		value = DA_SMUTEL_ENABLE|DA_SMUTER_ENABLE;
		break;
	}

	if (on)
		s5m3500x_update_bits(s5m3500x, S5M3500X_040_PLAY_MODE, mask, value);
	else
		s5m3500x_update_bits(s5m3500x, S5M3500X_040_PLAY_MODE, mask, 0);

	mutex_unlock(&s5m3500x->mute_lock);

	dev_info(dev, "%s called, Channel %s %s work done.\n", __func__, get_dac_channel_name(channel), on ? "Mute" : "Unmute");
}

static int s5m3500x_codec_probe(struct snd_soc_component *codec)
{
	struct s5m3500x_priv *s5m3500x = snd_soc_component_get_drvdata(codec);

	dev_info(codec->dev, "%s enter (SW Version : %d)\n", __func__, S5M3500X_CODEC_SW_VER);

	s5m3500x->codec = codec;

	/* initialize Codec Power Supply */
	s5m3500x_power_configuration(s5m3500x);
	
	/* Codec Power Up */
	if (s5m3500x->power_always_on)
		s5m3500x_codec_power_enable(s5m3500x, true);

#if IS_ENABLED(CONFIG_PM)
	if (pm_runtime_enabled(s5m3500x->dev))
		pm_runtime_get_sync(s5m3500x->dev);
#endif

	/* register additional initialize */
	s5m3500x_register_initialize(s5m3500x);

	/* ADC/DAC Mute */
	s5m3500x_adc_digital_mute(s5m3500x, ADC_MUTE_ALL, true);
	s5m3500x_dac_soft_mute(s5m3500x, DAC_MUTE_ALL, true);

#if IS_ENABLED(CONFIG_PM)
	if (pm_runtime_enabled(s5m3500x->dev))
		pm_runtime_put_sync(s5m3500x->dev);
#endif

	/* Ignore suspend status for DAPM endpoint */
	snd_soc_dapm_ignore_suspend(snd_soc_component_get_dapm(codec), "EPOUTN");
	snd_soc_dapm_ignore_suspend(snd_soc_component_get_dapm(codec), "HPOUTLN");
	snd_soc_dapm_ignore_suspend(snd_soc_component_get_dapm(codec), "AIF Playback");
	snd_soc_dapm_ignore_suspend(snd_soc_component_get_dapm(codec), "IN1L");
	snd_soc_dapm_ignore_suspend(snd_soc_component_get_dapm(codec), "IN2L");
	snd_soc_dapm_ignore_suspend(snd_soc_component_get_dapm(codec), "IN3L");
	snd_soc_dapm_ignore_suspend(snd_soc_component_get_dapm(codec), "AIF Capture");

	/* Jack probe */
	if (s5m3500x->jack_type  == JACK)
		s5m3500x_jack_probe(codec, s5m3500x);

	dev_info(codec->dev, "%s done\n", __func__);

	return 0;
}

static void s5m3500x_codec_remove(struct snd_soc_component *codec)
{
	struct s5m3500x_priv *s5m3500x = snd_soc_component_get_drvdata(codec);
	dev_info(s5m3500x->dev, "(*) %s called\n", __func__);
	s5m3500x_codec_disable(s5m3500x->dev, false);
	if (s5m3500x->jack_type  == JACK)
		s5m3500x_jack_remove(codec);
}

static const struct snd_soc_component_driver soc_codec_dev_s5m3500x = {
	.probe = s5m3500x_codec_probe,
	.remove = s5m3500x_codec_remove,
	.controls = s5m3500x_snd_controls,
	.num_controls = ARRAY_SIZE(s5m3500x_snd_controls),
	.dapm_widgets = s5m3500x_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(s5m3500x_dapm_widgets),
	.dapm_routes = s5m3500x_dapm_routes,
	.num_dapm_routes = ARRAY_SIZE(s5m3500x_dapm_routes),
	.use_pmdown_time = false,
	.idle_bias_on = false,
};

static int s5m3500x_i2c_probe(struct i2c_client *i2c)
{
	struct s5m3500x_priv *s5m3500x;
	int ret = 0;

	dev_info(&i2c->dev, "%s : i2c addr: 0x%02x\n",__func__,(int)i2c->addr);

	s5m3500x = kzalloc(sizeof(*s5m3500x), GFP_KERNEL);
	if (s5m3500x == NULL)
		return -ENOMEM;

	i2c->addr = S5M3500X_SLAVE_ADDR;
	s5m3500x->dev = &i2c->dev;
	
	/* initialize codec_priv variable */
	s5m3500x->codec_power_ref_cnt= 0;
	s5m3500x->cache_only = 0;
	s5m3500x->cache_bypass = 0;
	s5m3500x->power_always_on = true;
	s5m3500x->status = DEVICE_NONE;
	s5m3500x->amic_delay = 100;
	s5m3500x->playback_params.aifrate = SAMPLE_RATE_48KHZ;
	s5m3500x->playback_params.width = BIT_RATE_16;
	s5m3500x->playback_params.channels = CHANNEL_2;
	s5m3500x->capture_params.aifrate = SAMPLE_RATE_48KHZ;
	s5m3500x->capture_params.width = BIT_RATE_16;
	s5m3500x->capture_params.channels = CHANNEL_2;
	s5m3500x->jack_type = JACK;

	/* Parsing to dt */
	s5m3500x_i2c_parse_dt(s5m3500x);
	
	s5m3500x->i2c_priv = i2c;
	i2c_set_clientdata(s5m3500x->i2c_priv, s5m3500x);

	s5m3500x->regmap = devm_regmap_init_i2c(i2c, &s5m3500x_regmap);
	if (IS_ERR(s5m3500x->regmap)) {
		dev_err(&i2c->dev, "Failed to allocate regmap: %li\n",
				PTR_ERR(s5m3500x->regmap));
		ret = -ENOMEM;
		goto err;
	}

	/* initialize workqueue */
	INIT_DELAYED_WORK(&s5m3500x->adc_mute_work, s5m3500x_adc_mute_work);
	s5m3500x->adc_mute_wq = create_singlethread_workqueue("adc_mute_wq");
	if (s5m3500x->adc_mute_wq == NULL) {
		dev_err(s5m3500x->dev, "Failed to create adc_mute_wq\n");
		return -ENOMEM;
	}

	/* initialize mutex lock */
	mutex_init(&s5m3500x->reg_lock);
	mutex_init(&s5m3500x->regcache_lock);
	mutex_init(&s5m3500x->regsync_lock);
	mutex_init(&s5m3500x->mute_lock);

	/* register alsa component */
	ret = snd_soc_register_component(&i2c->dev, &soc_codec_dev_s5m3500x,
			s5m3500x_dai, 1);
	if (ret < 0) {
		dev_err(&i2c->dev, "Failed to register digital codec: %d\n", ret);
		goto err;
	}

#if IS_ENABLED(CONFIG_PM)
	if (s5m3500x->power_always_on)
		pm_runtime_disable(s5m3500x->dev);
	else
		pm_runtime_enable(s5m3500x->dev);
#endif
#ifdef CONFIG_DEBUG_FS
	s5m3500x_debug_init(s5m3500x);
#endif
	dev_info(&i2c->dev, "%s done\n",__func__);
	
	return ret;
err:
	kfree(s5m3500x);
	return ret;
}

static void s5m3500x_i2c_remove(struct i2c_client *i2c)
{
	struct s5m3500x_priv *s5m3500x = dev_get_drvdata(&i2c->dev);

	dev_info(s5m3500x->dev, "(*) %s called\n", __func__);

	destroy_workqueue(s5m3500x->adc_mute_wq);
#if IS_ENABLED(CONFIG_PM)
	if (pm_runtime_enabled(s5m3500x->dev))
		pm_runtime_disable(s5m3500x->dev);
#endif
#ifdef CONFIG_DEBUG_FS
	s5m3500x_debug_remove(s5m3500x);
#endif
	snd_soc_unregister_component(&i2c->dev);
	kfree(s5m3500x);
}

int s5m3500x_codec_enable(struct device *dev, bool force)
{
	struct s5m3500x_priv *s5m3500x = dev_get_drvdata(dev);

	dev_info(dev, "%s : enter\n", __func__);
	mutex_lock(&s5m3500x->mute_lock);

	if (s5m3500x->codec_power_ref_cnt == 0 || force) {
		/* Disable Cache Only Mode */
		s5m3500x_regcache_cache_only_switch(s5m3500x, false);
		/* Codec Power On */
		s5m3500x_codec_power_enable(s5m3500x, true);
		/* HW Register sync with cahce */
		s5m3500x_hardware_register_sync(s5m3500x,S5M3500X_REGISTER_START,S5M3500X_REGISTER_END);
	}

	s5m3500x->codec_power_ref_cnt++;

	mutex_unlock(&s5m3500x->mute_lock);

	/* ADC/DAC Mute */
	s5m3500x_adc_digital_mute(s5m3500x, ADC_MUTE_ALL, true);
	s5m3500x_dac_soft_mute(s5m3500x, DAC_MUTE_ALL, true);

	dev_info(dev, "%s : exit\n", __func__);

	return 0;
}

int s5m3500x_codec_disable(struct device *dev, bool force)
{
	struct s5m3500x_priv *s5m3500x = dev_get_drvdata(dev);

	dev_info(dev, "%s : enter\n", __func__);
	
	mutex_lock(&s5m3500x->mute_lock);
	s5m3500x->codec_power_ref_cnt--;

	if (s5m3500x->codec_power_ref_cnt == 0 || force) {
		if (s5m3500x->cache_only >= 1) {
			s5m3500x->cache_only = 1;
			s5m3500x_regcache_cache_only_switch(s5m3500x, false);
			dev_info(dev, "%s : force s5m3500x_regcache_cache_only_switch set false by codec disable\n", __func__);
		}
		/* Codec Power Off */
		s5m3500x_codec_power_enable(s5m3500x, false);
		/* Cache fall back to reg_defaults */
		s5m3500x_cache_register_sync_default(s5m3500x);
		/* Enable Cache Only Mode */
		s5m3500x_regcache_cache_only_switch(s5m3500x, true);
		/* set device status none */
		s5m3500x->status = DEVICE_NONE;
	}

	mutex_unlock(&s5m3500x->mute_lock);
	dev_info(dev, "%s : exit\n", __func__);

	return 0;
}

#if IS_ENABLED(CONFIG_PM) || IS_ENABLED(CONFIG_PM_SLEEP)
static int s5m3500x_resume(struct device *dev)
{
	struct s5m3500x_priv *s5m3500x = dev_get_drvdata(dev);

	dev_info(dev, "(*) %s\n", __func__);
	if (!s5m3500x->power_always_on)
		s5m3500x_codec_enable(dev, false);

	return 0;
}

static int s5m3500x_suspend(struct device *dev)
{
	struct s5m3500x_priv *s5m3500x = dev_get_drvdata(dev);

	dev_info(dev, "(*) %s\n", __func__);
	if (!s5m3500x->power_always_on)
		s5m3500x_codec_disable(dev, false);

	return 0;
}
#endif

static const struct dev_pm_ops s5m3500x_pm = {
#if IS_ENABLED(CONFIG_PM_SLEEP)
	SET_SYSTEM_SLEEP_PM_OPS(
			s5m3500x_suspend,
			s5m3500x_resume)
#endif
#if IS_ENABLED(CONFIG_PM)
	SET_RUNTIME_PM_OPS(
			s5m3500x_suspend,
			s5m3500x_resume,
			NULL)
#endif
};

static const struct i2c_device_id s5m3500x_i2c_id[] = {
	{ "s5m3500x", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, s5m3500x_i2c_id);

const struct of_device_id s5m3500x_of_match[] = {
	{ .compatible = "samsung,s5m3500x", },
	{ },
};
MODULE_DEVICE_TABLE(of, s5m3500x_of_match);

static struct i2c_driver s5m3500x_i2c_driver = {
	.driver = {
		.name = "s5m3500x",
		.owner = THIS_MODULE,
		.pm = &s5m3500x_pm,
		.of_match_table = of_match_ptr(s5m3500x_of_match),
	},
	.probe = s5m3500x_i2c_probe,
	.remove = s5m3500x_i2c_remove,
	.id_table = s5m3500x_i2c_id,
};

module_i2c_driver(s5m3500x_i2c_driver);

#ifdef CONFIG_DEBUG_FS

#define ADDR_SEPERATOR_STRING	"========================================================\n"
#define ADDR_INDEX_STRING		"      00 01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f\n"

static char *s5m3500x_get_register_block(unsigned int j)
{
	switch (j) {
	case 0:
		return "Digital Register Block\n";
	case 1:
		return "Analog Register Block\n";
	case 2:
		return "OTP Register Block\n";
	}

	return "";
}

static ssize_t s5m3500x_debugfs_cache_write_file(struct file *file,
									const char __user *user_buf,
									size_t count, loff_t *ppos)
{
	int ret = 0;
	struct s5m3500x_priv *s5m3500x = file->private_data;
	struct device *dev = s5m3500x->dev;

	char buf[32];
	size_t buf_size;
	char *start = buf;
	unsigned long reg, value;

	dev_info(dev, "%s : enter\n", __func__);

	buf_size = min(count, (sizeof(buf)-1));

	if (simple_write_to_buffer(buf, buf_size, ppos, user_buf, count) < 0)
		return -EFAULT;

	/* add zero at the end of data buf from user */
	buf[buf_size] = 0;

	/* extract register address */
	while (*start == ' ')
		start++;
	reg = simple_strtoul(start, &start, 16);

	/* extract register value */
	while (*start == ' ')
		start++;
	if (kstrtoul(start, 16, &value))
		return -EINVAL;

	ret = s5m3500x_write_only_cache(s5m3500x, reg, value);
	if (ret < 0)
		return ret;
	return buf_size;
}

static ssize_t s5m3500x_debugfs_cache_read_file(struct file *file,
									char __user *user_buf,
									size_t count, loff_t *ppos)
{
	int ret = 0;
	struct s5m3500x_priv *s5m3500x = file->private_data;
	struct device *dev = s5m3500x->dev;
	char *reg_dump;
	unsigned int value = 0, i = 0, j = 0, reg_dump_pos = 0, len = 0;

	dev_info(dev, "%s : enter\n", __func__);

	if (*ppos < 0 || !count)
		return -EINVAL;

	if (count > (PAGE_SIZE << (MAX_ORDER - 1)))
		count = PAGE_SIZE << (MAX_ORDER - 1);

	reg_dump = kmalloc(count, GFP_KERNEL);
	if (!reg_dump)
		return -ENOMEM;

	memset(reg_dump, 0, count);

	/* Register Block */
	for (j = 0; j < S5M3500X_REGISTER_BLOCKS; j++) {
		/* copy address index at the top of stdout */
		len = snprintf(reg_dump + reg_dump_pos, count - reg_dump_pos, "               %s", s5m3500x_get_register_block(j));
		if (len >= 0)
			reg_dump_pos += len;

		len = snprintf(reg_dump + reg_dump_pos, count - reg_dump_pos, ADDR_SEPERATOR_STRING);
		if (len >= 0)
			reg_dump_pos += len;

		/* copy address index at the top of stdout */
		len = snprintf(reg_dump + reg_dump_pos, count - reg_dump_pos, ADDR_INDEX_STRING);
		if (len >= 0)
			reg_dump_pos += len;

		/* register address 0x00 to 0xFF of each block */
		for (i = 0; i <= 255 ; i++) {
			/* line numbers like 0x10, 0x20, 0x30 ... 0xff
			 * in the first colume
			 */
			if (i % 16 == 0) {
				len = snprintf(reg_dump + reg_dump_pos, count - reg_dump_pos, "%04x:", j<<8|(i/16*16));
				if (len >= 0)
					reg_dump_pos += len;
			}

			/* read cache register value */
			ret = s5m3500x_read_only_cache(s5m3500x, j<<8|i, &value);
			if (ret == 0) {
				len = snprintf(reg_dump + reg_dump_pos, count - reg_dump_pos, " %02x", value);
				if (len >= 0)
					reg_dump_pos += len;
			} else {
				len = snprintf(reg_dump + reg_dump_pos, count - reg_dump_pos, " 00");
				if (len >= 0)
					reg_dump_pos += len;
			}

			/* change line */
			if (i % 16 == 15) {
				len = snprintf(reg_dump + reg_dump_pos, count - reg_dump_pos, "\n");
				if (len >= 0)
					reg_dump_pos += len;
			}
		}
		len = snprintf(reg_dump + reg_dump_pos, count - reg_dump_pos, "\n");
		if (len >= 0)
			reg_dump_pos += len;
	}

	/*get string length */
	ret = reg_dump_pos;

	/*show stdout */
	ret = simple_read_from_buffer(user_buf, count, ppos, reg_dump, reg_dump_pos);

	kfree(reg_dump);
	return ret;
}

static const struct file_operations s5m3500x_debugfs_cache_register_fops = {
	.open = simple_open,
	.read = s5m3500x_debugfs_cache_read_file,
	.write = s5m3500x_debugfs_cache_write_file,
	.llseek = default_llseek,
};

static ssize_t s5m3500x_debugfs_hw_write_file(struct file *file,
									const char __user *user_buf,
									size_t count, loff_t *ppos)
{
	int ret = 0;
	struct s5m3500x_priv *s5m3500x = file->private_data;
	struct device *dev = s5m3500x->dev;

	char buf[32];
	size_t buf_size;
	char *start = buf;
	unsigned long reg, value;

	dev_info(dev, "%s : enter\n", __func__);

	buf_size = min(count, (sizeof(buf)-1));

	if (simple_write_to_buffer(buf, buf_size, ppos, user_buf, count) < 0)
		return -EFAULT;

	/* add zero at the end of data buf from user */
	buf[buf_size] = 0;

	/* extract register address */
	while (*start == ' ')
		start++;
	reg = simple_strtoul(start, &start, 16);

	/* extract register value */
	while (*start == ' ')
		start++;
	if (kstrtoul(start, 16, &value))
		return -EINVAL;

	ret = s5m3500x_write_only_hardware(s5m3500x, reg, value);
	if (ret < 0)
		return ret;
	return buf_size;
}

static ssize_t s5m3500x_debugfs_hw_read_file(struct file *file,
									char __user *user_buf,
									size_t count, loff_t *ppos)
{
	int ret = 0;
	struct s5m3500x_priv *s5m3500x = file->private_data;
	struct device *dev = s5m3500x->dev;
	char *reg_dump;
	unsigned int value = 0, i = 0, j = 0, reg_dump_pos = 0, len = 0;

	dev_info(dev, "%s : enter\n", __func__);

	if (*ppos < 0 || !count)
		return -EINVAL;

	if (count > (PAGE_SIZE << (MAX_ORDER - 1)))
		count = PAGE_SIZE << (MAX_ORDER - 1);

	reg_dump = kmalloc(count, GFP_KERNEL);
	if (!reg_dump)
		return -ENOMEM;

	memset(reg_dump, 0, count);

	/* Register Block */
	for (j = 0; j < S5M3500X_REGISTER_BLOCKS; j++) {
		/* copy address index at the top of stdout */
		len = snprintf(reg_dump + reg_dump_pos, count - reg_dump_pos, "                  %s", s5m3500x_get_register_block(j));
		if (len >= 0)
			reg_dump_pos += len;

		len = snprintf(reg_dump + reg_dump_pos, count - reg_dump_pos, ADDR_SEPERATOR_STRING);
		if (len >= 0)
			reg_dump_pos += len;

		len = snprintf(reg_dump + reg_dump_pos, count - reg_dump_pos, ADDR_INDEX_STRING);
		if (len >= 0)
			reg_dump_pos += len;

		/* register address 0x00 to 0xFF of each block */
		for (i = 0; i <= 255 ; i++) {
			/* line numbers like 0x10, 0x20, 0x30 ... 0xff
			 * in the first colume
			 */
			if (i % 16 == 0) {
				len = snprintf(reg_dump + reg_dump_pos, count - reg_dump_pos, "%04x:", j<<8|(i/16*16));
				if (len >= 0)
					reg_dump_pos += len;
			}

			/* read HW register value */
			ret = s5m3500x_read_only_hardware(s5m3500x, j<<8|i, &value);
			if (ret == 0) {
				len = snprintf(reg_dump + reg_dump_pos, count - reg_dump_pos, " %02x", value);
				if (len >= 0)
					reg_dump_pos += len;
			} else {
				len = snprintf(reg_dump + reg_dump_pos, count - reg_dump_pos, " 00");
				if (len >= 0)
					reg_dump_pos += len;
			}

			/* change line */
			if (i % 16 == 15) {
				len = snprintf(reg_dump + reg_dump_pos, count - reg_dump_pos, "\n");
				if (len >= 0)
					reg_dump_pos += len;
			}
		}
		/* change line */
		len = snprintf(reg_dump + reg_dump_pos, count - reg_dump_pos, "\n");
		if (len >= 0)
			reg_dump_pos += len;
	}

	/*get string length */
	ret = reg_dump_pos;

	/*show stdout */
	ret = simple_read_from_buffer(user_buf, count, ppos, reg_dump, reg_dump_pos);

	kfree(reg_dump);
	return ret;
}

static const struct file_operations s5m3500x_debugfs_hw_register_fops = {
	.open = simple_open,
	.read = s5m3500x_debugfs_hw_read_file,
	.write = s5m3500x_debugfs_hw_write_file,
	.llseek = default_llseek,
};

#define REGDUMP_ALL	0xffff
unsigned long read_reg;
int parsing_buf(struct s5m3500x_priv *s5m3500x, char *start)
{
	int ret = 0;
	unsigned long reg, value;
	struct device *dev = s5m3500x->dev;

	dev_info(dev, "%s : %s\n", __func__,start);

	//check read / write command
	if(*start == 'r')
	{
		//move to next parameter
		start++;start++;

		/* extract register address */
		while (*start == ' ')
			start++;
		read_reg = simple_strtoul(start, &start, 16);
		dev_info(dev, "%s : 0x%lx\n", __func__,read_reg);
	}
	else if(*start == 'w')
	{
		//move to next parameter
		start++;start++;

		/* extract register address */
		while (*start == ' ')
			start++;
		reg = simple_strtoul(start, &start, 16);

		/* extract register value */
		while (*start == ' ')
			start++;
		if (kstrtoul(start, 16, &value))
			return -EINVAL;
		dev_info(dev, "%s : 0x%lx 0x%lx\n", __func__,reg,value);

		ret = s5m3500x_write(s5m3500x, reg, value);
		if (ret < 0)
			return ret;
	}
	else
	{
		dev_err(dev, "%s : parsing error %s\n", __func__,start);
		return -1;
	}

	return 0;
}

static ssize_t s5m3500x_debugfs_write_file(struct file *file,
									const char __user *user_buf,
									size_t count, loff_t *ppos)
{
	int ret = 0;
	struct s5m3500x_priv *s5m3500x = file->private_data;
	struct device *dev = s5m3500x->dev;

	char buf[32];
	size_t buf_size;
	char *start = buf;
	//unsigned long reg, value;

	dev_info(dev, "%s : enter %zu\n", __func__,count);

	buf_size = min(count, (sizeof(buf)-1));

	if (simple_write_to_buffer(buf, buf_size, ppos, user_buf, count) < 0)
		return -EFAULT;

	/* add zero at the end of data buf from user */
	buf[buf_size] = 0;

	//parsing buffer and run command
	ret = parsing_buf(s5m3500x, start);
	if(ret < 0)
		return ret;
	return buf_size;
}

static ssize_t s5m3500x_debugfs_read_file(struct file *file,
									char __user *user_buf,
									size_t count, loff_t *ppos)
{
	int ret = 0, len = 0;
	struct s5m3500x_priv *s5m3500x = file->private_data;
	struct device *dev = s5m3500x->dev;
	char reg_dump[6];
	char *reg_dump_all;
	unsigned int value = 0, i = 0, reg_dump_pos = 0;

	dev_info(dev, "%s : enter %zu 0x%lx\n", __func__,count, read_reg);

	if (*ppos < 0 || !count)
		return -EINVAL;

	//reg_dump init
	if(read_reg != REGDUMP_ALL) {
		memset(reg_dump, 0, 6);
		s5m3500x_read(s5m3500x, read_reg, &value);
		len = snprintf(reg_dump, 6, "0x%02x\n", value);
		dev_info(dev, "%s : len %d reg_dump %s", __func__,len, reg_dump);
		/*show stdout */
		ret = simple_read_from_buffer(user_buf, count, ppos, reg_dump, len);
	}
	else
	{
		if (count > (PAGE_SIZE << (MAX_ORDER - 1)))
			count = PAGE_SIZE << (MAX_ORDER - 1);
		reg_dump_all = kmalloc(count, GFP_KERNEL);
		if (!reg_dump_all)
			return -ENOMEM;
		memset(reg_dump_all, 0, count);

		/* register address 0x00 to 0x2FF of each block */
		for (i = 0; i <= S5M3500X_REGISTER_BLOCKS * S5M3500X_BLOCK_SIZE; i++) {
			/* read HW register value */
			ret = s5m3500x_read(s5m3500x, i, &value);
			if (ret == 0) {
				len = snprintf(reg_dump_all + reg_dump_pos, count - reg_dump_pos, "%02x ", value);
				if (len >= 0)
					reg_dump_pos += len;
			} else {
				len = snprintf(reg_dump_all + reg_dump_pos, count - reg_dump_pos, "00 ");
				if (len >= 0)
					reg_dump_pos += len;
			}
		}

		/*get string length */
		ret = reg_dump_pos;

		/*show stdout */
		ret = simple_read_from_buffer(user_buf, count, ppos, reg_dump_all, reg_dump_pos);

		kfree(reg_dump_all);
	}
	return ret;
}

static const struct file_operations s5m3500x_debugfs_register_fops = {
	.open = simple_open,
	.read = s5m3500x_debugfs_read_file,
	.write = s5m3500x_debugfs_write_file,
	.llseek = default_llseek,
};

static ssize_t s5m3500x_debugfs_cache_only_file(struct file *file,
									char __user *user_buf,
									size_t count, loff_t *ppos)
{
	int ret = 0;
	struct s5m3500x_priv *s5m3500x = file->private_data;
	struct device *dev = s5m3500x->dev;
	char *reg_dump;
	unsigned int reg_dump_pos = 0, len = 0;

	dev_info(dev, "%s : enter\n", __func__);

	if (*ppos < 0 || !count)
		return -EINVAL;

	if (count > (PAGE_SIZE << (MAX_ORDER - 1)))
		count = PAGE_SIZE << (MAX_ORDER - 1);

	reg_dump = kmalloc(count, GFP_KERNEL);

	if (!reg_dump)
		return -ENOMEM;

	memset(reg_dump, 0, count);

	len = snprintf(reg_dump + reg_dump_pos, count - reg_dump_pos, "\nCache only is %s.\n\n",
					is_cache_only_enabled(s5m3500x) > 0 ? "Enabled" : "Disabled");
	if (len >= 0)
		reg_dump_pos += len;

	/*get string length */
	ret = reg_dump_pos;

	/*show stdout */
	ret = simple_read_from_buffer(user_buf, count, ppos, reg_dump, reg_dump_pos);

	kfree(reg_dump);
	return ret;
}

static const struct file_operations s5m3500x_debugfs_cache_only_fops = {
	.open = simple_open,
	.read = s5m3500x_debugfs_cache_only_file,
	.llseek = default_llseek,
};

static ssize_t s5m3500x_debugfs_cache_bypass_file(struct file *file,
									char __user *user_buf,
									size_t count, loff_t *ppos)
{
	int ret = 0;
	struct s5m3500x_priv *s5m3500x = file->private_data;
	struct device *dev = s5m3500x->dev;
	char *reg_dump;
	unsigned int reg_dump_pos = 0, len = 0;

	dev_info(dev, "%s : enter\n", __func__);

	if (*ppos < 0 || !count)
		return -EINVAL;

	if (count > (PAGE_SIZE << (MAX_ORDER - 1)))
		count = PAGE_SIZE << (MAX_ORDER - 1);

	reg_dump = kmalloc(count, GFP_KERNEL);

	if (!reg_dump)
		return -ENOMEM;

	memset(reg_dump, 0, count);

	len = snprintf(reg_dump + reg_dump_pos, count - reg_dump_pos, "\nCache bypass is %s.\n\n",
					is_cache_bypass_enabled(s5m3500x) > 0 ? "Enabled" : "Disabled");
	if (len >= 0)
		reg_dump_pos += len;

	/*get string length */
	ret = reg_dump_pos;

	/*show stdout */
	ret = simple_read_from_buffer(user_buf, count, ppos, reg_dump, reg_dump_pos);

	kfree(reg_dump);
	return ret;
}

static const struct file_operations s5m3500x_debugfs_cache_bypass_fops = {
	.open = simple_open,
	.read = s5m3500x_debugfs_cache_bypass_file,
	.llseek = default_llseek,
};

static void s5m3500x_debug_init(struct s5m3500x_priv *s5m3500x)
{
	struct device *dev = s5m3500x->dev;

	if (s5m3500x->dbg_root) {
		dev_err(dev, "%s : debugfs is already exist\n", __func__);
		return;
	}

	s5m3500x->dbg_root = debugfs_create_dir("s5m3500x", NULL);
	if (!s5m3500x->dbg_root) {
		dev_err(dev, "%s : cannot creat debugfs dir\n", __func__);
		return;
	}
	debugfs_create_file("cache_registers", S_IRUGO|S_IWUGO, s5m3500x->dbg_root,
			s5m3500x, &s5m3500x_debugfs_cache_register_fops);
	debugfs_create_file("hw_registers", S_IRUGO|S_IWUGO, s5m3500x->dbg_root,
			s5m3500x, &s5m3500x_debugfs_hw_register_fops);
	debugfs_create_file("registers", S_IRUGO|S_IWUGO, s5m3500x->dbg_root,
			s5m3500x, &s5m3500x_debugfs_register_fops);
	debugfs_create_file("cache_only", S_IRUGO|S_IWUGO, s5m3500x->dbg_root,
			s5m3500x, &s5m3500x_debugfs_cache_only_fops);
	debugfs_create_file("cache_bypass", S_IRUGO|S_IWUGO, s5m3500x->dbg_root,
			s5m3500x, &s5m3500x_debugfs_cache_bypass_fops);
}

static void s5m3500x_debug_remove(struct s5m3500x_priv *s5m3500x)
{
	if (s5m3500x->dbg_root)
		debugfs_remove_recursive(s5m3500x->dbg_root);
}
#endif //CONFIG_DEBUG_FS


MODULE_DESCRIPTION("ASoC S5M3500X driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:S5M3500X-codec");

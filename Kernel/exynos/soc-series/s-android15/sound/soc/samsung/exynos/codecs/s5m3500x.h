/*
 * /sound/soc/samsung/exynos/codecs/s5m3500x.h
 *
 * ALSA SoC Audio Layer - Samsung Codec Driver
 *
 * Copyright (C) 2024 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _S5M3500X_H
#define _S5M3500X_H

#include "s5m3500x-register.h"

#define S5M3500X_REGISTER_START                        0x0000
#define S5M3500X_REGISTER_END                 			0x023F

#define S5M3500X_REGISTER_BLOCKS              			3
#define S5M3500X_BLOCK_SIZE                            0x100
#define S5M3500X_DIGITAL_BLOCK                 			0
#define S5M3500X_ANALOG_BLOCK                  			1
#define S5M3500X_OTP_BLOCK                             2

#define ADC_MUTE_CH_L				0
#define ADC_MUTE_CH_R				1
#define ADC_MUTE_ALL					2

#define DAC_MUTE_CH_L				0
#define DAC_MUTE_CH_R				1
#define DAC_MUTE_ALL					2

/* Device Status */
#define DEVICE_NONE					0x0000
#define DEVICE_AMIC1					0x0001
#define DEVICE_AMIC2					0x0002
#define DEVICE_AMIC3					0x0004
#define DEVICE_AMIC_ON				(DEVICE_AMIC1|DEVICE_AMIC2|DEVICE_AMIC3)
#define DEVICE_ADC_ON				(DEVICE_AMIC_ON)

#define DEVICE_EP						0x0100
#define DEVICE_HP						0x0200
#define DEVICE_DAC_ON				(DEVICE_EP|DEVICE_HP)

#define DEVICE_PLAYBACK_ON			(DEVICE_DAC_ON)
#define DEVICE_CAPTURE_ON			(DEVICE_ADC_ON)

/* HW Params */
#define SAMPLE_RATE_48KHZ					48000
#define SAMPLE_RATE_192KHZ					192000

#define BIT_RATE_16							16
#define BIT_RATE_24							24
#define BIT_RATE_32							32

#define CHANNEL_2								2

/* Jack type */
enum {
	JACK = 0,
};

struct hw_params {
	unsigned int aifrate;
	unsigned int width;
	unsigned int channels;
};

struct s5m3500x_priv {
	/* codec driver default */
	struct device *dev;
	struct i2c_client *i2c_priv;
	struct regmap *regmap;

	/* Alsa codec driver default */
	struct snd_soc_component *codec;

	/* codec mutex */
	struct mutex reg_lock;
	struct mutex regcache_lock;
	struct mutex regsync_lock;
	struct mutex mute_lock;

	/* codec workqueue */
	struct delayed_work adc_mute_work;
	struct workqueue_struct *adc_mute_wq;

	/* Power & regulator (Depends on HW)*/
	int resetb_gpio;
	struct regulator *vdd_1_8v;

	/* saved information */
	int codec_power_ref_cnt;
	int cache_bypass;
	int cache_only;
	bool power_always_on;
	int amic_delay;
	struct hw_params playback_params;
	struct hw_params capture_params;
	
	/* device_status */
	unsigned int status;

	/* Jack */
	unsigned int jack_type;
	struct s5m3500x_jack *p_jackdet;

	/*debugfs*/
#ifdef CONFIG_DEBUG_FS
	struct dentry *dbg_root;
#endif
};

/* Forward Declarations */
/* Main Functions */
/* Regmap Function */
int s5m3500x_write(struct s5m3500x_priv *s5m3500x,
	unsigned int addr, unsigned int value);
int s5m3500x_read(struct s5m3500x_priv *s5m3500x,
	unsigned int addr, unsigned int *value);
int s5m3500x_update_bits(struct s5m3500x_priv *s5m3500x,
	unsigned int addr, unsigned int mask, unsigned int value);

int s5m3500x_read_only_cache(struct s5m3500x_priv *s5m3500x,
	unsigned int addr, unsigned int *value);
int s5m3500x_write_only_cache(struct s5m3500x_priv *s5m3500x,
	unsigned int addr, unsigned int value);
int s5m3500x_update_bits_only_cache(struct s5m3500x_priv *s5m3500x,
	 int addr, unsigned int mask, unsigned int value);

int s5m3500x_read_only_hardware(struct s5m3500x_priv *s5m3500x,
	unsigned int addr, unsigned int *value);
int s5m3500x_write_only_hardware(struct s5m3500x_priv *s5m3500x,
	unsigned int addr, unsigned int value);
int s5m3500x_update_bits_only_hardware(struct s5m3500x_priv *s5m3500x,
	unsigned int addr, unsigned int mask, unsigned int value);

void s5m3500x_regcache_cache_only_switch(struct s5m3500x_priv *s5m3500x, bool on);
void s5m3500x_regcache_cache_bypass_switch(struct s5m3500x_priv *s5m3500x, bool on);
int s5m3500x_regmap_register_sync(struct s5m3500x_priv *s5m3500x);
int s5m3500x_hardware_register_sync(struct s5m3500x_priv *s5m3500x, int start, int end);
int s5m3500x_cache_register_sync_default(struct s5m3500x_priv *s5m3500x);
int s5m3500x_regmap_reinit_cache(struct s5m3500x_priv *s5m3500x);
void s5m3500x_update_reg_defaults(const struct reg_sequence *patch, const int array_size);

void s5m3500x_usleep(unsigned int u_sec);
bool s5m3500x_check_device_status(struct s5m3500x_priv *s5m3500x, unsigned int status);
void s5m3500x_adc_digital_mute(struct s5m3500x_priv *s5m3500x, unsigned int channel, bool on);

#endif /* _S5M3500X_H */

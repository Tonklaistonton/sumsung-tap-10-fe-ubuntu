/*
 * /sound/soc/samsung/exynos/codecs/s5m3500x-jack-common.c
 *
 * ALSA SoC Audio Layer - Samsung Codec Driver
 *
 * Copyright (C) 2020 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/regmap.h>
#include <linux/of_gpio.h>
#include <sound/soc.h>
#include <sound/jack.h>
#include <linux/input-event-codes.h>

#include "s5m3500x.h"
#include "s5m3500x-jack.h"
#include "s5m3500x-register.h"

#define MICBIAS_MIN				0
#define MICBIAS_MAX				15
#define MICBIAS_DEFAULT			MICBIAS_MAX

/* Jack Detection Notifier Head */
static BLOCKING_NOTIFIER_HEAD(s5m3500x_notifier);

/* JACK IRQ */
unsigned int s5m3500x_jack_irq_addr[] = {
	S5M3500X_001_IRQ1,
	S5M3500X_002_IRQ2,
	S5M3500X_003_IRQ3,
	S5M3500X_005_IRQ5,
	S5M3500X_006_IRQ6,
	S5M3500X_0F0_STATUS1,
	S5M3500X_0F1_STATUS2,
	S5M3500X_0F2_STATUS3,
};

void jack_wake_lock(struct wakeup_source *ws)
{
	__pm_stay_awake(ws);
}

void jack_wake_unlock(struct wakeup_source *ws)
{
	__pm_relax(ws);
}

int jack_set_wake_lock(struct s5m3500x_jack *jackdet)
{
	struct wakeup_source *ws = NULL;

	ws = wakeup_source_register(NULL, "s5m3500x-jack");

	if (ws == NULL)
		goto err;

	jackdet->jack_wakeup = ws;

	return 0;
err:
	return -1;
}

void jack_unregister_wake_lock(struct s5m3500x_jack *jackdet)
{
	wakeup_source_unregister(jackdet->jack_wakeup);
}

int s5m3500x_get_irq_type(struct s5m3500x_priv *s5m3500x, unsigned int event)
{
	struct s5m3500x_jack *jackdet = s5m3500x->p_jackdet;
	int ret = IRQ_ST_ERR;

	unsigned int v1 = jackdet->irq_val[0] & 0xf0,v2 = jackdet->irq_val[1] & 0xe0, v3 = jackdet->irq_val[7] & CUR_BTN_STATE_MASK;
	unsigned int irq_val = v1 + (v2 >> 4);

	//Register 0x01
	if (irq_val & ST_JO_R)
		ret = IRQ_ST_JACKOUT;
	else if (irq_val & ST_C_JI_R)
		ret = IRQ_ST_CMP_JACK_IN;
	else if (irq_val & ST_S_JI_R)
		ret = IRQ_ST_SLIGHT_JACK_IN;
	else if (irq_val & ST_WTP_R)
		ret = IRQ_ST_WTJACK_POLLING;
	//Register 0x02
	else if (irq_val & (INT_WT_JACK_R>>4))
		ret = IRQ_ST_WTJACK_POLLING_DONE;
	else if (irq_val & (POLE_CHK_R>>4))
		ret = IRQ_ST_POLE_DEC;
	else if (irq_val & (GP_AVG_DONE_R>>4) )
		ret = IRQ_ST_ADC_READ_DONE;
	//Register 0xf2
	else if (v3 & (CUR_BTN_PRESS|CUR_BTN_RELEASE) )
		ret = IRQ_ST_BTN_DET;
	return ret;
}

void s5m3500x_auto_boost_reset(struct s5m3500x_priv *s5m3500x)
{
	s5m3500x_update_bits(s5m3500x, S5M3500X_0AD_AMU_CTRL14, 0x03, 0x03);
	s5m3500x_usleep(100);
	s5m3500x_update_bits(s5m3500x, S5M3500X_0AD_AMU_CTRL14, 0x03, 0x00);
	s5m3500x_usleep(50000); //50ms delay
}

/* Jack Control */
static const struct snd_kcontrol_new s5m3500x_snd_jack_controls[] = {
	SOC_SINGLE("Jack DBNC In", S5M3500X_0D8_DCTR_DBNC1,
			A2D_JACK_DBNC_IN_SHIFT, 15, 0),

	SOC_SINGLE("Jack DBNC Out", S5M3500X_0D8_DCTR_DBNC1,
			A2D_JACK_DBNC_OUT_SHIFT, 15, 0),

	SOC_SINGLE("MDET Threshold", S5M3500X_0C4_ACTR_JD5,
			CTRV_REF_ANT_MDET_SHIFT, 7, 0),

	SOC_SINGLE("MDET DBNC In", S5M3500X_0DB_DCTR_DBNC4,
			ANT_MDET_DBNC_IN_SHIFT, 15, 0),

	SOC_SINGLE("MDET DBNC Out", S5M3500X_0DB_DCTR_DBNC4,
			ANT_MDET_DBNC_OUT_SHIFT, 15, 0),

	SOC_SINGLE("MDET2 DBNC In", S5M3500X_0DC_DCTR_DBNC5,
			ANT_MDET2_DBNC_IN_SHIFT, 15, 0),

	SOC_SINGLE("MDET2 DBNC Out", S5M3500X_0DC_DCTR_DBNC5,
			ANT_MDET2_DBNC_OUT_SHIFT, 15, 0),

	SOC_SINGLE("Jack BTN DBNC", S5M3500X_0DD_DCTR_DBNC6,
			CTMD_BTN_DBNC_SHIFT, 15, 0),

};

void s5m3500x_register_jack_controls(struct snd_soc_component *codec, struct s5m3500x_priv *s5m3500x)
{
	int ret = 0;

	dev_info(s5m3500x->dev, "%s enter\n", __func__);

	ret = snd_soc_add_component_controls(codec, s5m3500x_snd_jack_controls,
			ARRAY_SIZE(s5m3500x_snd_jack_controls));
	if (ret != 0)
		dev_err(s5m3500x->dev, "Failed to add Jack controls: %d\n", ret);
}

/*
 * s5m3500x_jack_variable_initialize()
 *
 * @s5m3500x: codec information struct
 *
 * Desc: Initialize jack_det struct variable as 0.
 */

void s5m3500x_jack_variable_initialize(struct s5m3500x_priv *s5m3500x, struct s5m3500x_jack *jackdet)
{
	/* initialize struct s5m3500x_jack */
	memset(jackdet, 0, sizeof(struct s5m3500x_jack));

	/* allocate pointer variable */
	s5m3500x->p_jackdet = jackdet;
	jackdet->codec = s5m3500x->codec;
	jackdet->p_s5m3500x = s5m3500x;

	/* initialize variable for local value */
	jackdet->prv_jack_state = JACK_OUT;
	jackdet->cur_jack_state = JACK_OUT;
	jackdet->gpadc_done = false;
	jackdet->btn_state = BUTTON_RELEASE;

	/* Initialize mutex lock */
	mutex_init(&jackdet->key_lock);
	mutex_init(&jackdet->adc_lock);
	mutex_init(&jackdet->jack_det_lock);

	if (jack_set_wake_lock(jackdet) < 0) {
		pr_err("%s: jack_set_wake_lock fail\n", __func__);
		jack_unregister_wake_lock(jackdet);
	}
}


/*
 * s5m3500x_common_parse_dt() - Parsing device tree options about jack
 *
 * @s5m3500x: codec information struct
 *
 * Desc: Initialize jack_det struct variable as parsing device tree options.
 * Customer can tune this options. If not set by customer,
 * it use default value below defined.
 */
void s5m3500x_common_parse_dt(struct s5m3500x_priv *s5m3500x)
{
	struct s5m3500x_jack *jackdet = s5m3500x->p_jackdet;
	struct device *dev = s5m3500x->dev;
	struct of_phandle_args args;
	int ret = 0, i = 0;
	unsigned int bias_v_conf;

	/* parsing IRQ */
	jackdet->irqb_gpio = of_get_named_gpio(dev->of_node, "s5m3500x-codec-int", 0);
	if (jackdet->irqb_gpio < 0)
		dev_err(dev, "%s cannot find irqb gpio in the dt\n", __func__);
	else
		dev_info(dev, "%s: irqb gpio = %d\n",
				__func__, jackdet->irqb_gpio);

	/*
	 * Set mic bias 1/2 voltages
	 */
	/* Mic Bias 1 */
	ret = of_property_read_u32(dev->of_node, "mic-bias1-voltage", &bias_v_conf);
	if (!ret &&
			((bias_v_conf >= MICBIAS_MIN) &&
			 (bias_v_conf <= MICBIAS_MAX))) {
		jackdet->mic_bias1_voltage = bias_v_conf;
	} else {
		jackdet->mic_bias1_voltage = MICBIAS_DEFAULT;
		dev_warn(dev, "Property 'mic-bias1-voltage' %s",
				ret ? "not found, default set 2.8V" : "used invalid value");
	}

	/* Mic Bias 2 */
	ret = of_property_read_u32(dev->of_node, "mic-bias2-voltage", &bias_v_conf);
	if (!ret &&
			((bias_v_conf >= MICBIAS_MIN) &&
			 (bias_v_conf <= MICBIAS_MAX))) {
		jackdet->mic_bias2_voltage = bias_v_conf;
	} else {
		jackdet->mic_bias2_voltage = MICBIAS_DEFAULT;
		dev_warn(dev, "Property 'mic-bias2-voltage' %s",
				ret ? "not found, default set 2.8V" : "used invalid value");
	}

	dev_info(dev, "Bias voltage values: bias1=%d, bias2=%d\n", jackdet->mic_bias1_voltage, jackdet->mic_bias2_voltage);

	/* Mic det adc range */
	ret = of_parse_phandle_with_args(dev->of_node, "mic-adc-range", "#list-imp-cells", 0, &args);
	if (!ret) {
		for (i = 0; i < 2; i++)
			jackdet->mic_adc_range[i] = args.args[i];
	} else {
		jackdet->mic_adc_range[0] = S5M3500X_MIC_ADC_RANGE_0;
		jackdet->mic_adc_range[1] = S5M3500X_MIC_ADC_RANGE_1;
	}

	dev_info(dev, "Mic ADC values: Range 0=%d, Range 1=%d\n", jackdet->mic_adc_range[0], jackdet->mic_adc_range[1]);

	/* Button press adc value, a maximum of 4 buttons are supported */
	for (i = 0; i < 4; i++) {
		if (of_parse_phandle_with_args(dev->of_node,
					"but-zones-list", "#list-but-cells", i, &args))
			break;
		jackdet->btn_zones[i].adc_low = args.args[0];
		jackdet->btn_zones[i].adc_high = args.args[1];
	}

	dev_info(dev, "btn_zones: btn_zones[1]=(%d,%d), btn_zones[2]=(%d,%d), btn_zones[3]=(%d,%d), btn_zones[4]=(%d,%d)\n",
		jackdet->btn_zones[0].adc_low, jackdet->btn_zones[0].adc_high,
		jackdet->btn_zones[1].adc_low, jackdet->btn_zones[1].adc_high,
		jackdet->btn_zones[2].adc_low, jackdet->btn_zones[2].adc_high,
		jackdet->btn_zones[3].adc_low, jackdet->btn_zones[3].adc_high);

	/* WTP LDET threshold value */
	ret = of_parse_phandle_with_args(dev->of_node, "wtp-ldet-range", "#list-ldet-cells", 0, &args);
	if (!ret) {
		for (i = 0; i < 3; i++)
			jackdet->wtp_ldet_thd[i] = args.args[i];
	} else {
		for (i = 0; i < 3; i++)
			jackdet->wtp_ldet_thd[i] = S5M3500X_WTP_THD_DEFAULT;
	}
}

/* Request thread IRQ */
int s5m3500x_request_threaded_irq(struct s5m3500x_priv *s5m3500x,
					irqreturn_t (*s5m3500x_irq_thread)(int, void *))
{
	struct s5m3500x_jack *jackdet = s5m3500x->p_jackdet;
	int ret = 0;

	if (jackdet->irqb_gpio > 0) {
		jackdet->codec_irq = gpio_to_irq(jackdet->irqb_gpio);
		dev_info(s5m3500x->dev, "%s: codec_irq = %d\n", __func__, jackdet->codec_irq);

		if (jackdet->codec_irq > 0) {
			ret = request_threaded_irq(jackdet->codec_irq,
					NULL, s5m3500x_irq_thread,
					IRQF_TRIGGER_LOW | IRQF_ONESHOT,
					"s5m3500x-irq", jackdet);
			if (ret)
				dev_err(s5m3500x->dev, "%s Failed to Request IRQ. ret: %d\n", __func__, ret);
			else {
				dev_info(s5m3500x->dev, "%s Request IRQ. ret: %d\n", __func__, ret);

				ret = enable_irq_wake(jackdet->codec_irq);
				if (ret < 0) {
					dev_err(s5m3500x->dev, "%s Failed to Enable Wakeup Source(%d)\n", __func__, ret);
					disable_irq_wake(jackdet->codec_irq);
				}
			}
		} else
			dev_err(s5m3500x->dev, "%s Failed gpio_to_irq. ret: %d\n", __func__, jackdet->codec_irq);
	} else {
		dev_err(s5m3500x->dev, "%s Failed get irqb_gpio %d\n", __func__, jackdet->irqb_gpio);
	}

	return ret;
}

/*Register Jack Detection Notifier */
int s5m3500x_register_notifier(struct s5m3500x_priv *s5m3500x,
		int (*s5m3500x_notifier_handler)(struct notifier_block *, unsigned long, void *))
{
	int ret = 0;
	struct s5m3500x_jack *jackdet = s5m3500x->p_jackdet;

	/* Allocate Notifier handler */
	jackdet->s5m3500x_jack_det_nb.notifier_call = s5m3500x_notifier_handler;

	/* Register Notifier */
	ret = blocking_notifier_chain_register(&s5m3500x_notifier, &(jackdet->s5m3500x_jack_det_nb));
	if (ret < 0)
		dev_err(s5m3500x->dev, "%s Failed to register notifier. ret: %d\n", __func__, ret);

	return ret;
}

/*
 * s5m3500x_register_jack
 */
int s5m3500x_register_jack(struct snd_soc_component *codec,
		struct s5m3500x_priv *s5m3500x, int type)
{
	struct s5m3500x_jack *jackdet = s5m3500x->p_jackdet;
	int ret = 0;

	if (jackdet->headset_jack.jack == NULL) {
#if 0
		ret = snd_soc_card_jack_new(codec->card,
					    "S5M3500X Headset Input", type,
					    &jackdet->headset_jack, NULL, 0);
#else
		ret = snd_soc_card_jack_new(codec->card, "S5M3500X Headset Input", type, &jackdet->headset_jack);
#endif
		if (ret) {
			dev_info(s5m3500x->dev, "%s: failed to create new jack %d\n", __func__, ret);
			return ret;
		}
		dev_info(s5m3500x->dev, "%s: jack is registered\n", __func__);
	} else {
		dev_info(s5m3500x->dev, "%s: jack is already registered\n", __func__);
	}
	return ret;
}

int s5m3500x_register_button(struct snd_soc_component *codec,
		struct s5m3500x_priv *s5m3500x, int type)
{
	struct s5m3500x_jack *jackdet = s5m3500x->p_jackdet;
	int ret = 0;

	if (jackdet->headset_button.jack == NULL) {
#if 0
		ret = snd_soc_card_jack_new(codec->card,
					    "S5M3500X Headset Button", type,
					    &jackdet->headset_button, NULL, 0);
#else
		ret = snd_soc_card_jack_new(codec->card, "S5M3500X Headset Button", type, &jackdet->headset_button);
#endif
		if (ret) {
			dev_info(s5m3500x->dev, "%s: failed to create new button jack %d\n", __func__, ret);
			return ret;
		}
		/* keycode */
		snd_jack_set_key(jackdet->headset_button.jack, SND_JACK_BTN_0, KEY_MEDIA); //226
		snd_jack_set_key(jackdet->headset_button.jack, SND_JACK_BTN_1, KEY_VOICECOMMAND); //0x246
		snd_jack_set_key(jackdet->headset_button.jack, SND_JACK_BTN_2, KEY_VOLUMEUP); //115
		snd_jack_set_key(jackdet->headset_button.jack, SND_JACK_BTN_3, KEY_VOLUMEDOWN); //114
		dev_info(s5m3500x->dev, "%s: button jack is registered\n", __func__);
	} else {
		dev_info(s5m3500x->dev, "%s: button jack is already registered\n", __func__);
	}
	return ret;
}

/*
 * report jack type
 */
void s5m3500x_report_jack_type(struct s5m3500x_priv *s5m3500x, int type, int insert)
{
	struct s5m3500x_jack *jackdet = s5m3500x->p_jackdet;

	if (insert) {
		switch (jackdet->cur_jack_state) {
		case JACK_3POLE:
			snd_soc_jack_report(&jackdet->headset_jack, SND_JACK_HEADPHONE, type);
			dev_info(s5m3500x->dev, "%s: Report SND_JACK_HEADPHONE\n", __func__);
			break;
		case JACK_4POLE:
			snd_soc_jack_report(&jackdet->headset_jack, SND_JACK_HEADSET, type);
			dev_info(s5m3500x->dev, "%s: Report SND_JACK_HEADSET\n", __func__);
			break;
		case JACK_AUX:
#if 0 // temp fix (not recognized on framework)
			snd_soc_jack_report(&jackdet->headset_jack, SND_JACK_LINEOUT, type);
			dev_info(s5m3500x->dev, "%s: Report SND_JACK_LINEOUT\n", __func__);
#else
			snd_soc_jack_report(&jackdet->headset_jack, SND_JACK_HEADPHONE, type);
			dev_info(s5m3500x->dev, "%s: Report SND_JACK_HEADPHONE\n", __func__);
#endif
			break;
		default:
			dev_info(s5m3500x->dev, "%s Unsupported jack state\n", __func__);
			break;
		}
	} else {
		snd_soc_jack_report(&jackdet->headset_jack, 0, type);
		dev_info(s5m3500x->dev, "%s: Report Jack Out\n", __func__);
	}
}

/*
 * report button press / release
 */
static int s5m3500x_get_btn_mask(struct s5m3500x_priv *s5m3500x, int btn_adc)
{
	struct s5m3500x_jack *jackdet = s5m3500x->p_jackdet;
	struct jack_btn_zone *btn_zones = jackdet->btn_zones;
	int num_btn_zones = ARRAY_SIZE(jackdet->btn_zones), i = 0, mask = SND_JACK_BTN_0;

	for (i = 0; i < num_btn_zones; i++) {
		if ((btn_adc >= btn_zones[i].adc_low)
			&& (btn_adc <= btn_zones[i].adc_high))
			break;
	}

	switch (i) {
	case 0:
		mask = SND_JACK_BTN_0;
		break;
	case 1:
		mask = SND_JACK_BTN_1;
		break;
	case 2:
		mask = SND_JACK_BTN_2;
		break;
	case 3:
		mask = SND_JACK_BTN_3;
		break;
	default:
		dev_err(s5m3500x->dev, "%s : Unsupported button press (adc value %d)\n", __func__, btn_adc);
		mask = -1;
		break;
	}
	return mask;
}

void s5m3500x_report_button_type(struct s5m3500x_priv *s5m3500x,
		int type, int state, int btn_adc)
{
	int btn_mask = 0;
	struct s5m3500x_jack *jackdet = s5m3500x->p_jackdet;

	if (state == BUTTON_RELEASE) {
		/* Mic unmute for remove click noise */
		if(s5m3500x_check_device_status(s5m3500x, DEVICE_DAC_ON))
			s5m3500x_adc_digital_mute(s5m3500x, ADC_MUTE_ALL, false);

		snd_soc_jack_report(&jackdet->headset_button, 0, type);
		jackdet->btn_state = BUTTON_RELEASE;
		dev_info(s5m3500x->dev, "%s : Release Button\n", __func__);
	} else {
		/* Mic mute for remove click noise */
		if(s5m3500x_check_device_status(s5m3500x, DEVICE_DAC_ON))
			s5m3500x_adc_digital_mute(s5m3500x, ADC_MUTE_ALL, true);

		btn_mask = s5m3500x_get_btn_mask(s5m3500x, btn_adc);
		if(btn_mask > 0) {
			snd_soc_jack_report(&jackdet->headset_button, btn_mask, type);
			jackdet->btn_state = BUTTON_PRESS;
			dev_info(s5m3500x->dev, "%s : Press Button (0x%x)\n", __func__, btn_mask);
		}
	}
}

bool s5m3500x_button_error_check(struct s5m3500x_priv *s5m3500x, int btn_irq_state)
{
	struct s5m3500x_jack *jackdet = s5m3500x->p_jackdet;

	/* Terminate workqueue Cond1: jack is not detected */
	if (jackdet->cur_jack_state & JACK_OUT) {
		dev_err(s5m3500x->dev, "Skip button events because jack is not detected.\n");
		if (jackdet->btn_state == BUTTON_PRESS) {
			jackdet->btn_state = BUTTON_RELEASE;
			s5m3500x_report_button_type(s5m3500x, S5M3500X_BUTTON_MASK, BUTTON_RELEASE, 0);
			dev_err(s5m3500x->dev, "force report BUTTON_RELEASE because button state is pressed.\n");
		}
		return false;
	}

	/* Terminate workqueue Cond2: 3 pole earjack */
	if (!(jackdet->cur_jack_state & JACK_4POLE)) {
		dev_err(s5m3500x->dev, "Skip button events for 3 pole earjack.\n");
		return false;
	}

	/* Terminate workqueue Cond3: state is not changed */
	if (jackdet->btn_state == btn_irq_state) {
		dev_err(s5m3500x->dev, "button state is not changed.\n");
		if (btn_irq_state == BUTTON_RELEASE) {
			s5m3500x_auto_boost_reset(s5m3500x);
			s5m3500x_adc_digital_mute(s5m3500x, ADC_MUTE_ALL, false);
		}
		return false;
	}

	return true;
}

/* call registered notifier handler */
void s5m3500x_call_notifier(struct s5m3500x_priv *s5m3500x, int insert)
{
	blocking_notifier_call_chain(&s5m3500x_notifier, insert, s5m3500x);
}

void s5m3500x_read_irq(struct s5m3500x_priv *s5m3500x)
{
	struct s5m3500x_jack *jackdet = s5m3500x->p_jackdet;
	unsigned int temp = 0, i = 0;

	for(i = 0; i < S5M3500X_NUM_JACK_IRQ; i++)
	{
		s5m3500x_read_only_hardware(s5m3500x, s5m3500x_jack_irq_addr[i], &temp);
		jackdet->irq_val[i] = (u8) temp;
	}

	dev_info(s5m3500x->dev,
			"[IRQ] %s(%d) 0x1:%02x 0x2:%02x 0x3:%02x 0x5:%02x 0x6:%02x 0xf0:%02x 0xf1:%02x 0xf2:%02x\n",
			__func__, __LINE__, jackdet->irq_val[0] & 0xf0, jackdet->irq_val[1] & 0xe7, jackdet->irq_val[2] & 0x3f,
			jackdet->irq_val[3] & 0x1f, jackdet->irq_val[4] & 0x1f, jackdet->irq_val[5] & 0x7f,
			jackdet->irq_val[6] & 0xf0, jackdet->irq_val[7] & 0x77);
}

void s5m3500x_wtp_threshold_initialize(struct s5m3500x_priv *s5m3500x)
{
	struct s5m3500x_jack *jackdet = s5m3500x->p_jackdet;
	struct device *dev = s5m3500x->dev;
	unsigned int v1, v2;
	int index = S5M3500X_WTP_THD_LOW_RANGE;

	/* LOWR_WTP_LDET */
	index = S5M3500X_WTP_THD_LOW_RANGE;
	if(jackdet->wtp_ldet_thd[index] == S5M3500X_WTP_THD_DEFAULT)
	{
		jackdet->wtp_ldet_thd[index] = S5M3500X_WTP_THD_LOW_DEFAULT;
	}

	/* MIDR_WTP_LDET */
	index = S5M3500X_WTP_THD_MID_RANGE;
	v1 = 0;
	v2 = 0;
	if(jackdet->wtp_ldet_thd[index] == S5M3500X_WTP_THD_DEFAULT)
	{
		s5m3500x_read(s5m3500x, S5M3500X_0D2_RESERVED, &v1);
		s5m3500x_read(s5m3500x, S5M3500X_0D1_RESERVED, &v2);

		v2 = (v2 & 0xC0) >> 6 << 8;
		jackdet->wtp_ldet_thd[index] = v2 + v1;
	} else {
		v1 = jackdet->wtp_ldet_thd[index] & 0xFF;
		v2 = (jackdet->wtp_ldet_thd[index] >> 8) & 0x03;

		s5m3500x_write(s5m3500x, S5M3500X_0D2_RESERVED, v1);
		s5m3500x_update_bits(s5m3500x, S5M3500X_0D1_RESERVED, 0xC0, v2 << 6);
	}

	/* HIGHHR_WTP_LDET */
	index = S5M3500X_WTP_THD_HIGH_RANGE;
	v1 = 0;
	v2 = 0;
	if(jackdet->wtp_ldet_thd[index] == S5M3500X_WTP_THD_DEFAULT)
	{
		s5m3500x_read(s5m3500x, S5M3500X_0D3_RESERVED, &v1);
		s5m3500x_read(s5m3500x, S5M3500X_0D1_RESERVED, &v2);

		v2 = (v2 & 0x30) >> 4 << 8;
		jackdet->wtp_ldet_thd[index] = v2 + v1;
	} else {
		v1 = jackdet->wtp_ldet_thd[index] & 0xFF;
		v2 = (jackdet->wtp_ldet_thd[index] >> 8) & 0x03;

		s5m3500x_write(s5m3500x, S5M3500X_0D3_RESERVED, v1);
		s5m3500x_update_bits(s5m3500x, S5M3500X_0D1_RESERVED,	0x30, v2 << 4);
	}

	dev_info(dev, "%s : wtp ldet range %d %d %d\n", __func__
					,jackdet->wtp_ldet_thd[S5M3500X_WTP_THD_LOW_RANGE]
					,jackdet->wtp_ldet_thd[S5M3500X_WTP_THD_MID_RANGE]
					,jackdet->wtp_ldet_thd[S5M3500X_WTP_THD_HIGH_RANGE]);
}


/*
 * s5m3500x_configure_mic_bias() - Configure mic bias voltage
 *
 * @jackdet: jack information struct
 *
 * Desc: Configure the mic1 and mic2 bias voltages with default value
 * or the value received from the device tree.
 * Also configure the internal LDO voltage.
 */
unsigned int get_micbias1_value(int mic_bias1_voltage)
{
	switch(mic_bias1_voltage)
	{
	case 0: return CTRV_MCB1_1P3; break;
	case 1: return CTRV_MCB1_1P4; break;
	case 2: return CTRV_MCB1_1P5; break;
	case 3: return CTRV_MCB1_1P6; break;
	case 4: return CTRV_MCB1_1P7; break;
	case 5: return CTRV_MCB1_1P8; break;
	case 6: return CTRV_MCB1_1P9; break;
	case 7: return CTRV_MCB1_2P0; break;
	case 8: return CTRV_MCB1_2P1; break;
	case 9: return CTRV_MCB1_2P2; break;
	case 10: return CTRV_MCB1_2P3; break;
	case 11: return CTRV_MCB1_2P4; break;
	case 12: return CTRV_MCB1_2P5; break;
	case 13: return CTRV_MCB1_2P6; break;
	case 14: return CTRV_MCB1_2P7; break;
	case 15: return CTRV_MCB1_2P8; break;
	}
	return CTRV_MCB1_2P8;
}

unsigned int get_micbias2_value(int mic_bias2_voltage)
{
	switch(mic_bias2_voltage)
	{
	case 0: return CTRV_MCB2_1P3; break;
	case 1: return CTRV_MCB2_1P4; break;
	case 2: return CTRV_MCB2_1P5; break;
	case 3: return CTRV_MCB2_1P6; break;
	case 4: return CTRV_MCB2_1P7; break;
	case 5: return CTRV_MCB2_1P8; break;
	case 6: return CTRV_MCB2_1P9; break;
	case 7: return CTRV_MCB2_2P0; break;
	case 8: return CTRV_MCB2_2P1; break;
	case 9: return CTRV_MCB2_2P2; break;
	case 10: return CTRV_MCB2_2P3; break;
	case 11: return CTRV_MCB2_2P4; break;
	case 12: return CTRV_MCB2_2P5; break;
	case 13: return CTRV_MCB2_2P6; break;
	case 14: return CTRV_MCB2_2P7; break;
	case 15: return CTRV_MCB2_2P8; break;
	}
	return CTRV_MCB2_2P8;
}

void s5m3500x_configure_mic_bias(struct s5m3500x_priv *s5m3500x)
{
	struct s5m3500x_jack *jackdet = s5m3500x->p_jackdet;

	/* Configure Mic1 Bias Voltage */
	s5m3500x_update_bits(s5m3500x, S5M3500X_0C7_ACTR_MCB3,
			CTRV_MCB1_MASK, get_micbias1_value(jackdet->mic_bias1_voltage));

	/* Configure Mic2 Bias Voltage */
	s5m3500x_update_bits(s5m3500x, S5M3500X_0C7_ACTR_MCB3,
			CTRV_MCB2_MASK, get_micbias2_value(jackdet->mic_bias2_voltage));
}

void s5m3500x_jack_register_exit(struct snd_soc_component *codec)
{
	struct s5m3500x_priv *s5m3500x = snd_soc_component_get_drvdata(codec);

	/* IRQ Masking */
	s5m3500x_write(s5m3500x, S5M3500X_008_IRQ1M, 0xFF);
	s5m3500x_write(s5m3500x, S5M3500X_009_IRQ2M, 0xFF);
	s5m3500x_write(s5m3500x, S5M3500X_00A_IRQ3M, 0xFF);
	s5m3500x_write(s5m3500x, S5M3500X_00B_IRQ4M, 0xFF);
}


bool s5m3500x_check_jack_state_sequence(struct s5m3500x_priv *s5m3500x)
{
	struct s5m3500x_jack *jackdet = s5m3500x->p_jackdet;
	bool ret = false;
	unsigned int cur_jack, prv_jack;

	cur_jack = jackdet->cur_jack_state;
	prv_jack = jackdet->prv_jack_state;

	switch (cur_jack) {
	case JACK_OUT:
		if (prv_jack & (JACK_IN | JACK_WTP_ST | JACK_POLE_DEC | JACK_CMP | JACK_SJI))
			ret = true;
		break;
	case JACK_POLE_DEC:
		if (prv_jack & JACK_CMP)
			ret = true;
		break;
	case JACK_3POLE:
		if (prv_jack & JACK_POLE_DEC)
			ret = true;
		break;
	case JACK_4POLE:
		if (prv_jack & JACK_POLE_DEC)
			ret = true;
		break;
	case JACK_AUX:
		if (prv_jack & JACK_POLE_DEC)
			ret = true;
		break;
	case JACK_OMTP:
		if (prv_jack & JACK_POLE_DEC)
			ret = true;
		break;
	case JACK_CMP:
		if (prv_jack & (JACK_OUT | JACK_WTP_IN | JACK_WTP_POLL_DONE))
			ret = true;
		break;
	case JACK_SJI:
		if (prv_jack & (JACK_OUT | JACK_WTP_DEC | JACK_WTP_POLL_DONE) )
			ret = true;
		break;
	case JACK_WTP_DEC:
		if (prv_jack & (JACK_SJI | JACK_WTP_POLL_DONE))
			ret = true;
		break;
	case JACK_WTP_IN:
		if (prv_jack & JACK_WTP_DEC)
			ret = true;
		break;
	case JACK_WTP_OUT:
		if (prv_jack & JACK_WTP_DEC)
			ret = true;
		break;
	case JACK_WTP_AGAIN:
		if (prv_jack & (JACK_WTP_DEC | JACK_WTP_AGAIN))
			ret = true;
		break;
	case JACK_WTP_POLL:
		if (prv_jack & (JACK_WTP_DEC | JACK_WTP_POLL_DONE))
			ret = true;
		break;
	case JACK_WTP_POLL_DONE:
		if (prv_jack & JACK_WTP_POLL)
			ret = true;
		break;
	}

	if (!ret) {
		dev_err(s5m3500x->dev, "%s Jack state machine error! prv: %s, cur: %s\n", __func__,
				s5m3500x_return_status_name(prv_jack),
				s5m3500x_return_status_name(cur_jack));
	}
	return ret;
}

char *s5m3500x_return_status_name(unsigned int status)
{
	switch (status) {
	case JACK_OUT:
		return "JACK_OUT";
	case JACK_POLE_DEC:
		return "JACK_POLE_DEC";
	case JACK_3POLE:
		return "JACK_3POLE";
	case JACK_4POLE:
		return "JACK_4POLE";
	case JACK_AUX:
		return "JACK_AUX";
	case JACK_OMTP:
		return "JACK_OMTP";
	case JACK_CMP:
		return "JACK_CMP";
	case JACK_SJI:
		return "JACK_SJI";
	case JACK_WTP_DEC:
		return "JACK_WTP_DEC";
	case JACK_WTP_IN:
		return "JACK_WTP_IN";
	case JACK_WTP_OUT:
		return "JACK_WTP_OUT";
	case JACK_WTP_AGAIN:
		return "JACK_WTP_AGAIN";
	case JACK_WTP_POLL:
		return "JACK_WTP_POLL";
	case JACK_WTP_POLL_DONE:
		return "JACK_WTP_POLL_DONE";
	case JACK_IN:
		return "JACK_IN";
	case JACK_WTP_ST:
		return "JACK_WTP_ST";
	}
	return "No status";
}

char *s5m3500x_print_jack_type_state(int cur_jack_state)
{

	if (cur_jack_state & JACK_IN) {
		if (cur_jack_state & JACK_4POLE)
			return "4POLE";
		else if (cur_jack_state & JACK_AUX)
			return "AUX";
		else
			return "3POLE";
	}
	return "";
}

void s5m3500x_print_status_change(struct device *dev, unsigned int prv_jack, unsigned int cur_jack)
{
	dev_info(dev, "Priv: %s -> Cur: %s\n"
		, s5m3500x_return_status_name(prv_jack), s5m3500x_return_status_name(cur_jack));
}
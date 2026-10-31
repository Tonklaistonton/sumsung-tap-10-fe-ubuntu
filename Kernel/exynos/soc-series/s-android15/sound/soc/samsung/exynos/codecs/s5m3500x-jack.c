/*
 * /sound/soc/samsung/exynos/codecs/s5m3500x-jack.c
 *
 * ALSA SoC Audio Layer - Samsung Codec Driver
 *
 * Copyright (C) 2024 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/of_gpio.h>
#include <sound/soc.h>
#include <sound/jack.h>
 #include <linux/wait.h> 

#include "s5m3500x.h"
#include "s5m3500x-jack.h"
#include "s5m3500x-register.h"
#include "s5m3500x-regmap.h"

#define MAX_CNT 			30
#define WAIT_TIMEOUT 		5000 //5sec
#define POLLING_CNT			400
#define POLLING_WAIT_TIME	50000

static DECLARE_WAIT_QUEUE_HEAD(wq);

extern void abox_enable_mclk(unsigned int on);
extern bool abox_is_on(void);
static void s5m3500x_jack_parse_dt(struct s5m3500x_priv *s5m3500x);
static void s5m3500x_pre_init_register(struct s5m3500x_priv *s5m3500x);
static void s5m3500x_jack_register_initialize(struct s5m3500x_priv *s5m3500x);
static int s5m3500x_jack_notifier_handler(struct notifier_block *nb, unsigned long event, void *data);
static void s5m3500x_jackstate_set(struct s5m3500x_priv *s5m3500x, unsigned int change_state);

static char *s5m3500x_gpadc_name(int type)
{
	if(type == JACK_MDET_ADC)
		return "MIC_DET";
	else if(type == JACK_LDET_ADC)
		return "HPL_DET";
	return

 "";
}

static int s5m3500x_read_adc(struct s5m3500x_priv *s5m3500x, int type)
{
	struct s5m3500x_jack *jackdet = s5m3500x->p_jackdet;
	unsigned int value = 0, v1 = 0, v2 = 0;
	int ret = 0;

	mutex_lock(&jackdet->adc_lock);
	jackdet->gpadc_done = false;
	
	switch (type) {
	case JACK_MDET_ADC:
			s5m3500x_update_bits(s5m3500x, S5M3500X_0FD_ACTR_GP,
				T_EN_AVG_START_MASK|EN_AVG_START_MASK|T_CTMP_GPADCIN_MASK|CTMP_GPADCIN_MASK,
				T_EN_AVG_START_TESTMODE|EN_AVG_START_ENABLE|T_CTMP_GPADCIN_TESTMODE|CTMP_GPADCIN_MIC_DET);
		break;
	case JACK_LDET_ADC:
	default:
			s5m3500x_update_bits(s5m3500x, S5M3500X_0FD_ACTR_GP,
				T_EN_AVG_START_MASK|EN_AVG_START_MASK|T_CTMP_GPADCIN_MASK|CTMP_GPADCIN_MASK,
				T_EN_AVG_START_TESTMODE|EN_AVG_START_ENABLE|T_CTMP_GPADCIN_TESTMODE|CTMP_GPADCIN_HPL_DET);
		break;
	}

	//Wait until GPADC Read done
	ret = wait_event_timeout(wq, jackdet->gpadc_done == true, msecs_to_jiffies(WAIT_TIMEOUT));
	if(!ret)
		dev_err(s5m3500x->dev, "%s called. GPADC (%s) Read Timeout (%d)\n", __func__, s5m3500x_gpadc_name(type), WAIT_TIMEOUT);

	s5m3500x_read_only_hardware(s5m3500x,S5M3500X_0FA_STATUS11,&v2);
	s5m3500x_read_only_hardware(s5m3500x,S5M3500X_0FB_STATUS12,&v1);

	s5m3500x_update_bits(s5m3500x, S5M3500X_0FD_ACTR_GP,
		EN_AVG_START_MASK|T_CTMP_GPADCIN_MASK|CTMP_GPADCIN_MASK,
		EN_AVG_START_DISABLE|T_CTMP_GPADCIN_NORMAL|CTMP_GPADCIN_NA);
	s5m3500x_update_bits(s5m3500x, S5M3500X_0FD_ACTR_GP, T_EN_AVG_START_MASK, T_EN_AVG_START_NORMAL);

	v2 = (v2 & 0xc0) >> 6 << 8;
	value = v1 + v2;
	mutex_unlock(&jackdet->adc_lock);
	dev_info(s5m3500x->dev, "%s called. ADC Pin: %s, Value: %d\n", __func__, s5m3500x_gpadc_name(type), value);
	return value;
}

/*
 * Workqueue handle
 */

//left impedance check for water decision
static void s5m3500x_ldet_chk_work(struct work_struct *work)
{
	struct s5m3500x_jack *jackdet =
		container_of(work, struct s5m3500x_jack, ldet_chk_work.work);
	struct s5m3500x_priv *s5m3500x = jackdet->p_s5m3500x;
	struct device *dev = s5m3500x->dev;

	unsigned int decision_state = JACK_WTP_OUT;
	int ldet_adc = 0, retry_cnt = 0;

	dev_info(dev, "%s called, cur_jack_state: %s.\n",
			__func__, s5m3500x_return_status_name(jackdet->cur_jack_state));

	/* Pole value decision */
	if (jackdet->cur_jack_state & JACK_WTP_DEC) {
		//Retry Max 3 Seconds
		while(retry_cnt != MAX_CNT)
		{
			/* Read ldet adc value for Water Protection */
			ldet_adc = s5m3500x_read_adc(s5m3500x, JACK_LDET_ADC);
			dev_info(dev, "%s called, ldet_adc %d\n", __func__,ldet_adc);

			if (ldet_adc < jackdet->wtp_ldet_thd[S5M3500X_WTP_THD_LOW_RANGE]) {
				decision_state = JACK_WTP_IN;
				break;
			} else if ((ldet_adc >= jackdet->wtp_ldet_thd[S5M3500X_WTP_THD_LOW_RANGE])
						&& (ldet_adc < jackdet->wtp_ldet_thd[S5M3500X_WTP_THD_MID_RANGE])) {
				decision_state = JACK_WTP_AGAIN;
				retry_cnt++;
				if(retry_cnt == MAX_CNT)
				{
					dev_info(dev, "%s called, Go to Jack Out State (retry cnt %d)\n", __func__,retry_cnt);
					decision_state = JACK_WTP_OUT;
					break;
				}
				dev_info(dev, "%s called, need to measure ldet_adc again\n", __func__);
				s5m3500x_usleep(100000); //100ms wait
				continue;
			} else if ((ldet_adc >= jackdet->wtp_ldet_thd[S5M3500X_WTP_THD_MID_RANGE])
						&& (ldet_adc < jackdet->wtp_ldet_thd[S5M3500X_WTP_THD_HIGH_RANGE])) {
				decision_state = JACK_WTP_POLL;
				break;
			} else {
				decision_state = JACK_WTP_OUT;
				break;
			}
		}
		s5m3500x_jackstate_set(s5m3500x, decision_state);
	} else {
		dev_info(dev, "%s called, Jack state is not JACK_WTP_DEC\n", __func__);
	}

	jack_wake_unlock(jackdet->jack_wakeup);
}

static void s5m3500x_jack_det_work(struct work_struct *work)
{
	struct s5m3500x_jack *jackdet =
		container_of(work, struct s5m3500x_jack, jack_det_work.work);
	struct s5m3500x_priv *s5m3500x = jackdet->p_s5m3500x;
	struct device *dev = s5m3500x->dev;
	int jack_state = JACK_4POLE;
	int mdet_adc = 0;

	mutex_lock(&jackdet->jack_det_lock);

	dev_info(dev, "%s called, cur_jack_state: %s.\n",
			__func__, s5m3500x_return_status_name(jackdet->cur_jack_state));

	/* Pole value decision */
	if (jackdet->cur_jack_state & JACK_POLE_DEC) {
		mdet_adc = s5m3500x_read_adc(s5m3500x,JACK_MDET_ADC);
		if (mdet_adc < jackdet->mic_adc_range[0])
			jack_state = JACK_3POLE;
		else if ((mdet_adc >= jackdet->mic_adc_range[0]) &&
			(mdet_adc < jackdet->mic_adc_range[1]))
			jack_state = JACK_4POLE;
		else
			jack_state = JACK_AUX;

		/* Report jack type */
		s5m3500x_jackstate_set(s5m3500x, jack_state);
		s5m3500x_report_jack_type(s5m3500x, S5M3500X_JACK_MASK, 1);

		dev_info(dev, "%s mdet_adc: %d, Jack: %s, Pole: %s\n", __func__,
				mdet_adc, (jackdet->cur_jack_state & JACK_OMTP) ? "OMTP" : "CTIA",
				s5m3500x_print_jack_type_state(jackdet->cur_jack_state));

	} else if (jackdet->cur_jack_state & JACK_IN) {
		dev_info(dev, "%s called, JACK_IN State again.\n", __func__);
		s5m3500x_report_jack_type(s5m3500x, S5M3500X_JACK_MASK, 1);
	} else {
		if (jackdet->cur_jack_state & JACK_OUT)
			dev_info(dev, "%s called, JACK_OUT State.\n", __func__);
		else
			dev_info(dev, "%s called, Unsupported state\n", __func__);
		s5m3500x_report_jack_type(s5m3500x, S5M3500X_JACK_MASK, 0);
	}	

	dev_info(dev, "%s called, Jack %s, Mic %s\n", __func__,
			(jackdet->cur_jack_state & JACK_IN) ?	"inserted" : "removed",
			(jackdet->cur_jack_state & JACK_4POLE) ? "inserted" : "removed");

	mutex_unlock(&jackdet->jack_det_lock);
	jack_wake_unlock(jackdet->jack_wakeup);
	
}

static void s5m3500x_button_release_work(struct work_struct *work)
{
	struct s5m3500x_jack *jackdet =
		container_of(work, struct s5m3500x_jack, btn_release_work.work);
	struct s5m3500x_priv *s5m3500x = jackdet->p_s5m3500x;
	struct device *dev = s5m3500x->dev;

	dev_info(dev, "%s called\n", __func__);

	/* Check error status */
	if (!s5m3500x_button_error_check(s5m3500x, BUTTON_RELEASE)) {
		jack_wake_unlock(jackdet->jack_wakeup);
		return;
	}

	s5m3500x_auto_boost_reset(s5m3500x);
	s5m3500x_report_button_type(s5m3500x, S5M3500X_BUTTON_MASK, BUTTON_RELEASE, 0);

	jack_wake_unlock(jackdet->jack_wakeup);
}

static void s5m3500x_button_press_work(struct work_struct *work)
{
	struct s5m3500x_jack *jackdet =
		container_of(work, struct s5m3500x_jack, btn_press_work.work);
	struct s5m3500x_priv *s5m3500x = jackdet->p_s5m3500x;
	struct device *dev = s5m3500x->dev;
	int btn_adc = 0;

	dev_info(dev, "%s called\n", __func__);

	/* Check error status */
	if (!s5m3500x_button_error_check(s5m3500x, BUTTON_PRESS)) {
		jack_wake_unlock(jackdet->jack_wakeup);
		return;
	}

	btn_adc = s5m3500x_read_adc(s5m3500x, JACK_MDET_ADC);

	s5m3500x_report_button_type(s5m3500x, S5M3500X_BUTTON_MASK, BUTTON_PRESS, btn_adc);

	jack_wake_unlock(jackdet->jack_wakeup);
}

static bool s5m3500x_polling_abox_is_on(struct s5m3500x_priv *s5m3500x)
{
	struct device *dev = s5m3500x->dev;
	int retry_cnt = 0;
	while(!abox_is_on())
	{
		if(retry_cnt++ > POLLING_CNT) {
			dev_info(dev, "%s over retry_cnt.%d %d\n", __func__,abox_is_on(),retry_cnt);
			return false;
		}
		s5m3500x_usleep(POLLING_WAIT_TIME); //50ms delay
	}
	dev_info(dev, "%s abox is ready(%d)\n", __func__,retry_cnt);
	return true;
}

static void s5m3500x_jack_init(struct s5m3500x_jack *jackdet)
{
	struct s5m3500x_priv *s5m3500x = jackdet->p_s5m3500x;
	struct device *dev = s5m3500x->dev;

	dev_info(dev, "%s enter\n", __func__);

	/* s5m3500x_resume */
#if IS_ENABLED(CONFIG_PM)
	if (pm_runtime_enabled(s5m3500x->dev))
		pm_runtime_get_sync(s5m3500x->dev);
#endif

	/* register initialize */
	s5m3500x_jack_register_initialize(s5m3500x);

	/* Sync Up WTP LDET threshold with HMU */
	s5m3500x_wtp_threshold_initialize(s5m3500x);

	/* Configure mic bias voltage */
	s5m3500x_configure_mic_bias(s5m3500x);

	/* Register jack detection notifier */
	s5m3500x_register_notifier(s5m3500x, s5m3500x_jack_notifier_handler);

	/* Request Jack IRQ */
	s5m3500x_request_threaded_irq(s5m3500x, s5m3500x_jack_irq_thread);

	/* s5m3500x_suspend */
#if IS_ENABLED(CONFIG_PM)
	if (pm_runtime_enabled(s5m3500x->dev))
		pm_runtime_put_sync(s5m3500x->dev);
#endif
}

static void s5m3500x_set_init_hmu_register(struct s5m3500x_jack *jackdet)
{
	struct s5m3500x_priv *s5m3500x = jackdet->p_s5m3500x;
	struct device *dev = s5m3500x->dev;

	dev_info(dev, "%s enter\n", __func__);

	//enable abox mclk
	abox_enable_mclk(true);

	//HW delay
	s5m3500x_usleep(100); //100usec

	//Start init register
	s5m3500x_jack_init(jackdet);

	//enable abox mclk
	abox_enable_mclk(false);
}

static void s5m3500x_init_hmu_work(struct work_struct *work)
{
	struct s5m3500x_jack *jackdet =
		container_of(work, struct s5m3500x_jack, init_hmu_work.work);
	struct s5m3500x_priv *s5m3500x = jackdet->p_s5m3500x;
	struct device *dev = s5m3500x->dev;

	dev_info(dev, "%s called\n", __func__);

	s5m3500x_pre_init_register(s5m3500x);

	if(!s5m3500x_polling_abox_is_on(s5m3500x))
	{
		dev_err(dev, "%s : need to enable mclk\n", __func__);
		return;
	}

	dev_info(dev, "%s : abox is %s\n", __func__,abox_is_on() ? "enabled" : "disabled");
	s5m3500x_set_init_hmu_register(jackdet);
	dev_info(dev, "%s is done : abox is %s\n", __func__,abox_is_on() ? "enabled" : "disabled");
}

/*
 * Jack Detection Process Routine Start
 * handler -> jackstate_set -> jackstate_register -> workqueue (if neeeded)
 */
//jackstate_register
static void s5m3500x_jackout_register(struct s5m3500x_priv *s5m3500x)
{
	s5m3500x_update_bits(s5m3500x, S5M3500X_0E1_DCTR_FSM2, POLE_VALUE_MASK, POLE_VALUE_NA);
}

static void s5m3500x_jack_pole_dec_register(struct s5m3500x_priv *s5m3500x)
{
}

static void s5m3500x_jack_3pole_register(struct s5m3500x_priv *s5m3500x)
{
	s5m3500x_update_bits(s5m3500x, S5M3500X_0E1_DCTR_FSM2, POLE_VALUE_MASK, POLE_VALUE_3POLE);
}

static void s5m3500x_jack_4pole_register(struct s5m3500x_priv *s5m3500x)
{
	s5m3500x_update_bits(s5m3500x, S5M3500X_0E1_DCTR_FSM2, POLE_VALUE_MASK, POLE_VALUE_4POLE);
}

static void s5m3500x_jack_aux_register(struct s5m3500x_priv *s5m3500x)
{
	s5m3500x_update_bits(s5m3500x, S5M3500X_0E1_DCTR_FSM2, POLE_VALUE_MASK, POLE_VALUE_ETC);
}

static void s5m3500x_jack_omtp_register(struct s5m3500x_priv *s5m3500x)
{
}

static void s5m3500x_jackcmp_register(struct s5m3500x_priv *s5m3500x)
{
}

static void s5m3500x_jack_sji_register(struct s5m3500x_priv *s5m3500x)
{
}

static void s5m3500x_jack_wtp_dec_register(struct s5m3500x_priv *s5m3500x)
{
	s5m3500x_update_bits(s5m3500x, S5M3500X_0E0_DCTR_FSM1, AP_POLLING_MASK, 0x00);
	s5m3500x_usleep(100);
}

static void s5m3500x_jack_wtp_cji_register(struct s5m3500x_priv *s5m3500x)
{
	//AP Jack In
	s5m3500x_update_bits(s5m3500x, S5M3500X_0E0_DCTR_FSM1, AP_JI_MASK, AP_JI_SET);
	s5m3500x_usleep(100);
	//Set Init value
	s5m3500x_update_bits(s5m3500x, S5M3500X_0E0_DCTR_FSM1,
		AP_JI_MASK|AP_JO_MASK|AP_POLLING_MASK, 0x00);
}

static void s5m3500x_jack_wtp_jo_register(struct s5m3500x_priv *s5m3500x)
{
	//AP Jack Out
	s5m3500x_update_bits(s5m3500x, S5M3500X_0E0_DCTR_FSM1, AP_JO_MASK, AP_JO_SET);
	s5m3500x_usleep(100);
	//Set Init value
	s5m3500x_update_bits(s5m3500x, S5M3500X_0E0_DCTR_FSM1,
		AP_JI_MASK|AP_JO_MASK|AP_POLLING_MASK, 0x00);
}

static void s5m3500x_jack_wtp_again_register(struct s5m3500x_priv *s5m3500x)
{
}

static void s5m3500x_jack_wtp_poll_register(struct s5m3500x_priv *s5m3500x)
{
	//AP Polling
	s5m3500x_update_bits(s5m3500x, S5M3500X_0E0_DCTR_FSM1, AP_POLLING_MASK, AP_POLLING_SET);
	s5m3500x_usleep(100);
}

static void s5m3500x_jack_wtp_poll_done_register(struct s5m3500x_priv *s5m3500x)
{
	//Set Init value
	s5m3500x_update_bits(s5m3500x, S5M3500X_0E0_DCTR_FSM1,
		AP_JI_MASK|AP_JO_MASK|AP_POLLING_MASK, 0x00);
}

static bool s5m3500x_jackstate_register(struct s5m3500x_priv *s5m3500x)
{
	struct s5m3500x_jack *jackdet = s5m3500x->p_jackdet;
	struct device *dev = s5m3500x->dev;
	unsigned int cur_jack, prv_jack;

	cur_jack = jackdet->cur_jack_state;
	prv_jack = jackdet->prv_jack_state;

	if (!s5m3500x_check_jack_state_sequence(s5m3500x))
		return false;

	s5m3500x_print_status_change(dev, prv_jack, cur_jack);
	switch (cur_jack) {
	case JACK_OUT:
		s5m3500x_jackout_register(s5m3500x);
		break;
	case JACK_POLE_DEC:
		s5m3500x_jack_pole_dec_register(s5m3500x);
		break;
	case JACK_3POLE:
		s5m3500x_jack_3pole_register(s5m3500x);
		break;
	case JACK_4POLE:
		s5m3500x_jack_4pole_register(s5m3500x);
		break;
	case JACK_AUX:
		s5m3500x_jack_aux_register(s5m3500x);
		break;
	case JACK_OMTP:
		s5m3500x_jack_omtp_register(s5m3500x);
		break;
	case JACK_CMP:
		s5m3500x_jackcmp_register(s5m3500x);
		break;
	case JACK_SJI:
		s5m3500x_jack_sji_register(s5m3500x);
		break;
	case JACK_WTP_DEC:
		s5m3500x_jack_wtp_dec_register(s5m3500x);
		break;
	case JACK_WTP_IN:
		s5m3500x_jack_wtp_cji_register(s5m3500x);
		break;
	case JACK_WTP_OUT:
		s5m3500x_jack_wtp_jo_register(s5m3500x);
		break;
	case JACK_WTP_AGAIN:
		s5m3500x_jack_wtp_again_register(s5m3500x);
		break;
	case JACK_WTP_POLL:
		s5m3500x_jack_wtp_poll_register(s5m3500x);
		break;
	case JACK_WTP_POLL_DONE:
		s5m3500x_jack_wtp_poll_done_register(s5m3500x);
		break;
	}
	return true;
}

//jackstate_set
static void s5m3500x_jackstate_set(struct s5m3500x_priv *s5m3500x,
		unsigned int change_state)
{
	struct s5m3500x_jack *jackdet = s5m3500x->p_jackdet;
	struct device *dev = s5m3500x->dev;
	int ret = 0;

	/* Save privious jack state */
	jackdet->prv_jack_state = jackdet->cur_jack_state;

	/* Update current jack state */
	jackdet->cur_jack_state = change_state;

	if (jackdet->prv_jack_state != jackdet->cur_jack_state) {
		dev_info(dev, "%s called, Prv: %s, Cur: %s\n",  __func__,
				s5m3500x_return_status_name(jackdet->prv_jack_state),
				s5m3500x_return_status_name(jackdet->cur_jack_state));

		/* Set jack register */
		ret = s5m3500x_jackstate_register(s5m3500x);
		dev_info(dev, "Jack register write %s.\n", ret ? "complete" : "incomplete");
	} else {
		dev_info(dev, "Prv_jack_state and Cur_jack_state are same (%s).\n",
					s5m3500x_return_status_name(jackdet->prv_jack_state));
	}
}

static void s5m3500x_st_jackout_handler(struct s5m3500x_priv *s5m3500x)
{
	struct s5m3500x_jack *jackdet = s5m3500x->p_jackdet;
	s5m3500x_jackstate_set(s5m3500x, JACK_OUT);

	cancel_delayed_work(&jackdet->ldet_chk_work);
	cancel_delayed_work(&jackdet->jack_det_work);
	queue_delayed_work(jackdet->jack_det_wq, &jackdet->jack_det_work,
			msecs_to_jiffies(0));
	queue_delayed_work(jackdet->btn_release_wq, &jackdet->btn_release_work,
			msecs_to_jiffies(BTN_ADC_DELAY));
}

static void s5m3500x_st_cmp_jack_in_handler(struct s5m3500x_priv *s5m3500x)
{
	s5m3500x_jackstate_set(s5m3500x, JACK_CMP);
}

static void s5m3500x_st_slightly_jack_in_handler(struct s5m3500x_priv *s5m3500x)
{
	struct s5m3500x_jack *jackdet = s5m3500x->p_jackdet;
	s5m3500x_jackstate_set(s5m3500x, JACK_SJI);
	s5m3500x_jackstate_set(s5m3500x, JACK_WTP_DEC);

	/* lock for jack and button irq */
	jack_wake_lock(jackdet->jack_wakeup);

	/* run ldet adc workqueue */
	cancel_delayed_work(&jackdet->ldet_chk_work);
	queue_delayed_work(jackdet->ldet_chk_wq, &jackdet->ldet_chk_work,
			msecs_to_jiffies(LDET_CHK_DELAY));
}

static void s5m3500x_st_pole_dec_handler(struct s5m3500x_priv *s5m3500x)
{
	struct s5m3500x_jack *jackdet = s5m3500x->p_jackdet;
	s5m3500x_jackstate_set(s5m3500x, JACK_POLE_DEC);

	//lock for jack and button irq
	jack_wake_lock(jackdet->jack_wakeup);
	cancel_delayed_work(&jackdet->jack_det_work);
	queue_delayed_work(jackdet->jack_det_wq, &jackdet->jack_det_work,
			msecs_to_jiffies(JACK_MDET_DELAY));	
}

static void s5m3500x_st_wt_jack_poll_done_handler(struct s5m3500x_priv *s5m3500x)
{
	//struct s5m3500x_jack *jackdet = s5m3500x->p_jackdet;
	
	s5m3500x_jackstate_set(s5m3500x, JACK_WTP_POLL_DONE);
#if 0
	s5m3500x_jackstate_set(s5m3500x, JACK_WTP_DEC);

	/* lock for jack and button irq */
	jack_wake_lock(jackdet->jack_wakeup);

	/* run ldet adc workqueue */
	cancel_delayed_work(&jackdet->ldet_chk_work);
	queue_delayed_work(jackdet->ldet_chk_wq, &jackdet->ldet_chk_work,
			msecs_to_jiffies(LDET_CHK_DELAY));
#endif
}

static void s5m3500x_st_adc_read_done_handler(struct s5m3500x_priv *s5m3500x)
{
	struct s5m3500x_jack *jackdet = s5m3500x->p_jackdet;
	if(waitqueue_active(&wq)) {
		jackdet->gpadc_done = true;
		wake_up(&wq);
	}
}

static void s5m3500x_st_btn_det_handler(struct s5m3500x_priv *s5m3500x)
{
	struct s5m3500x_jack *jackdet = s5m3500x->p_jackdet;
	bool isPress = false;

	/* lock for jack and button irq */
	jack_wake_lock(jackdet->jack_wakeup);
	
	if( (jackdet->irq_val[7] & CUR_BTN_STATE_MASK) & CUR_BTN_PRESS)
		isPress = true;

	if(isPress)
		queue_delayed_work(jackdet->btn_press_wq, &jackdet->btn_press_work, msecs_to_jiffies(BTN_ADC_DELAY));
	else
		queue_delayed_work(jackdet->btn_release_wq, &jackdet->btn_release_work, msecs_to_jiffies(BTN_ADC_DELAY));
}

static int s5m3500x_earjack_handler(struct s5m3500x_priv *s5m3500x, unsigned int event)
{
	struct device *dev = s5m3500x->dev;
	struct s5m3500x_jack *jackdet = s5m3500x->p_jackdet;
	int irq_type = s5m3500x_get_irq_type(s5m3500x, event);

	mutex_lock(&jackdet->key_lock);

	switch (irq_type) {
	case IRQ_ST_JACKOUT:
		dev_info(dev, "[IRQ] %s Jack out interrupt, line: %d\n", __func__, __LINE__);
		s5m3500x_st_jackout_handler(s5m3500x);
		break;
	case IRQ_ST_CMP_JACK_IN:
		dev_info(dev, "[IRQ] %s Completely Jack in interrupt, line: %d\n", __func__, __LINE__);
		s5m3500x_st_cmp_jack_in_handler(s5m3500x);
		break;
	case IRQ_ST_SLIGHT_JACK_IN:
		dev_info(dev, "[IRQ] %s Slightly Jack in interrupt, interrupt, line: %d\n", __func__, __LINE__);
		s5m3500x_st_slightly_jack_in_handler(s5m3500x);
		break;
	case IRQ_ST_POLE_DEC:
		dev_info(dev, "[IRQ] %s Pole Decision Interrupt, line: %d\n", __func__, __LINE__);
		s5m3500x_st_pole_dec_handler(s5m3500x);
		break;
	case IRQ_ST_WTJACK_POLLING:
		dev_info(dev, "[IRQ] %s Water Polling Interrupt, line: %d\n", __func__, __LINE__);
		break;
	case IRQ_ST_WTJACK_POLLING_DONE:
		dev_info(dev, "[IRQ] %s Water Polling Done interrupt, line: %d\n", __func__, __LINE__);
		s5m3500x_st_wt_jack_poll_done_handler(s5m3500x);
		break;
	case IRQ_ST_ADC_READ_DONE:
		dev_info(dev, "[IRQ] %s GPADC Read Done interrupt, line: %d\n", __func__, __LINE__);
		s5m3500x_st_adc_read_done_handler(s5m3500x);
		break;
	case IRQ_ST_BTN_DET:
		dev_info(dev, "[IRQ] %s Button Detection interrupt, line: %d\n", __func__, __LINE__);
		s5m3500x_st_btn_det_handler(s5m3500x);
		break;
	default:
		dev_info(dev, "[IRQ] %s IRQ return type skip, line %d\n", __func__, __LINE__);
		break;
	}

	mutex_unlock(&jackdet->key_lock);

	return IRQ_HANDLED;
}

/*
 * Jack Detection Process Routine End
 */

static void s5m3500x_jack_parse_dt(struct s5m3500x_priv *s5m3500x)
{
	s5m3500x_common_parse_dt(s5m3500x);
}

/* Register workqueue */
static int s5m3500x_register_jack_workqueue(struct s5m3500x_priv *s5m3500x)
{
	struct s5m3500x_jack *jackdet = s5m3500x->p_jackdet;

	/* Initiallize workqueue for jack detect handling */
	INIT_DELAYED_WORK(&jackdet->jack_det_work, s5m3500x_jack_det_work);
	jackdet->jack_det_wq = create_singlethread_workqueue("jack_det_wq");
	if (jackdet->jack_det_wq == NULL) {
		dev_err(s5m3500x->dev, "Failed to create jack_det_wq\n");
		return -ENOMEM;
	}

	/* Initialize workqueue for ldet detect handling */
	INIT_DELAYED_WORK(&jackdet->ldet_chk_work, s5m3500x_ldet_chk_work);
	jackdet->ldet_chk_wq = create_singlethread_workqueue("ldet_chk_wq");
	if (jackdet->ldet_chk_wq == NULL) {
		dev_err(s5m3500x->dev, "Failed to create ldet_chk_wq\n");
		return -ENOMEM;
	}

	/* Initialize workqueue for button press, release */
	INIT_DELAYED_WORK(&jackdet->btn_release_work, s5m3500x_button_release_work);
	jackdet->btn_release_wq = create_singlethread_workqueue("btn_release_wq");
	if (jackdet->btn_release_wq == NULL) {
		dev_err(s5m3500x->dev, "Failed to create btn_release_wq\n");
		return -ENOMEM;
	}

	INIT_DELAYED_WORK(&jackdet->btn_press_work, s5m3500x_button_press_work);
	jackdet->btn_press_wq = create_singlethread_workqueue("btn_press_wq");
	if (jackdet->btn_press_wq == NULL) {
		dev_err(s5m3500x->dev, "Failed to create btn_press_wq\n");
		return -ENOMEM;
	}

	/* codec mclk control workqueue */
	INIT_DELAYED_WORK(&jackdet->init_hmu_work, s5m3500x_init_hmu_work);
	jackdet->init_hmu_wq = create_singlethread_workqueue("init_hmu_wq");
	if (jackdet->init_hmu_wq == NULL) {
		dev_err(s5m3500x->dev, "Failed to create init_hmu_wq\n");
		return -ENOMEM;
	}
	return 0;
}

/* apply register initialize by patch
 * patch is only updated hw register, so cache need to be updated.
 */
static int s5m3500x_regmap_jack_register_patch(struct s5m3500x_priv *s5m3500x)
{
	struct device *dev = s5m3500x->dev;
	int ret = 0;

	/* jack register patch */
	ret = regmap_register_patch(s5m3500x->regmap, s5m3500x_jack_patch,
				ARRAY_SIZE(s5m3500x_jack_patch));
	if (ret < 0) {
		dev_err(dev, "Failed to apply s5m3500x_jack_patch %d\n", ret);
		return ret;
	}

	/* update reg_defaults with registered patch */
	s5m3500x_update_reg_defaults(s5m3500x_jack_patch, ARRAY_SIZE(s5m3500x_jack_patch));
	return ret;
}

/*
 * Registers which are not related mclk enable initialized.
 */
static void s5m3500x_pre_init_register(struct s5m3500x_priv *s5m3500x)
{
	int size = ARRAY_SIZE(s5m3500x_jack_patch);
	for(int i = 0; i < size; i++)
	{
		if(s5m3500x_jack_patch[i].reg != S5M3500X_015_RESETB1) {
			s5m3500x_write(s5m3500x, s5m3500x_jack_patch[i].reg, s5m3500x_jack_patch[i].def);
			s5m3500x_usleep(s5m3500x_jack_patch[i].delay_us * 1000);
		}
	}
}

/* 
 * don't need to call s5m3500x_regmap_reinit_cache api.
 * because cache is already updated by s5m3500x_write api is called.
 */

static void s5m3500x_jack_register_initialize(struct s5m3500x_priv *s5m3500x)
{
	int size = ARRAY_SIZE(s5m3500x_jack_patch);
	/* ResetB register setting after mclk is enabled */
	for(int i = 0; i < size; i++)
	{
		if(s5m3500x_jack_patch[i].reg == S5M3500X_015_RESETB1) {
			s5m3500x_write(s5m3500x, s5m3500x_jack_patch[i].reg, s5m3500x_jack_patch[i].def);
			s5m3500x_usleep(s5m3500x_jack_patch[i].delay_us * 1000);
		}
	}
	
	/* update initial registers on cache and hw registers */
	s5m3500x_regmap_jack_register_patch(s5m3500x);
	dev_info(s5m3500x->dev, "%s : s5m3500x_jack_register_initialize exit.\n", __func__);
}

/*
 * s5m3500x_jack_notifier_handler() - Codec IRQ Handler
 *
 * Desc: Set codec register according to codec IRQ.
 */
static int s5m3500x_jack_notifier_handler(struct notifier_block *nb,
		unsigned long event, void *data)
{
	struct s5m3500x_priv *s5m3500x = data;

	dev_info(s5m3500x->dev, "%s called.\n", __func__);

	/* read irq registgers */
	s5m3500x_read_irq(s5m3500x);

	s5m3500x_earjack_handler(s5m3500x, event);

	return IRQ_HANDLED;
}

/* IRQ Process Thread Routine */
irqreturn_t s5m3500x_jack_irq_thread(int irq, void *irq_data)
{
	struct s5m3500x_jack *jackdet = irq_data;
	struct s5m3500x_priv *s5m3500x = jackdet->p_s5m3500x;

	dev_info(s5m3500x->dev, "%s called.\n", __func__);

	/* call notifier handler */
	s5m3500x_call_notifier(s5m3500x, JACK_HANDLER_EVENT_NONE);

	return IRQ_HANDLED;
}

/*
 * s5m3500x_jack_probe() - Initialize variable related jack
 *
 * @codec: SoC audio codec device
 *
 * Desc: This function is called by s5m3500x_component_probe. For separate codec
 * and jack code, this function called from codec driver. This function initialize
 * jack variable, workqueue, mutex, and wakelock.
 */
int s5m3500x_jack_probe(struct snd_soc_component *codec, struct s5m3500x_priv *s5m3500x)
{
	struct s5m3500x_jack *jackdet;
	int ret = 0;

	dev_info(s5m3500x->dev, "Codec Jack Probe: (%s)\n", __func__);

	jackdet = kzalloc(sizeof(struct s5m3500x_jack), GFP_KERNEL);
	if (jackdet == NULL)
		return -ENOMEM;

	/* initialize variable */
	s5m3500x_jack_variable_initialize(s5m3500x, jackdet);

	/* Device Tree for jack */
	s5m3500x_jack_parse_dt(s5m3500x);

	/* Register workqueue */
	s5m3500x_register_jack_workqueue(s5m3500x);

	/* Register jack */
	s5m3500x_register_jack(codec, s5m3500x, S5M3500X_JACK_MASK);

	/* Register Button */
	s5m3500x_register_button(codec, s5m3500x, S5M3500X_BUTTON_MASK);

	/* Register Jack controls */
	s5m3500x_register_jack_controls(codec, s5m3500x);

	/* codec mclk control workqueue */
	if(jackdet->init_hmu_wq != NULL) {
		cancel_delayed_work(&jackdet->init_hmu_work);
		queue_delayed_work(jackdet->init_hmu_wq, &jackdet->init_hmu_work,
				msecs_to_jiffies(HMU_INIT_DELAY));
	}
	return ret;
}

int s5m3500x_jack_remove(struct snd_soc_component *codec)
{
	struct s5m3500x_priv *s5m3500x = snd_soc_component_get_drvdata(codec);
	struct s5m3500x_jack *jackdet = s5m3500x->p_jackdet;

	dev_info(codec->dev, "(*) %s called\n", __func__);

	destroy_workqueue(jackdet->ldet_chk_wq);
	destroy_workqueue(jackdet->jack_det_wq);
	destroy_workqueue(jackdet->btn_press_wq);
	destroy_workqueue(jackdet->btn_release_wq);
	destroy_workqueue(jackdet->init_hmu_wq);

	s5m3500x_jack_register_exit(codec);

	/* Unregister ADC pin */
	kfree(jackdet);

	return 0;
}

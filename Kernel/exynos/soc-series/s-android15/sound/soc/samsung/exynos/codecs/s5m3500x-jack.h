/*
 * /sound/soc/samsung/exynos/codecs/s5m3500x-jack.h
 *
 * ALSA SoC Audio Layer - Samsung Codec Driver
 *
 * Copyright (C) 2024 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef _S5M3500X_JACK_H
#define _S5M3500X_JACK_H

/* Jack state flag */
#define JACK_OUT				BIT(0) // JACK Out
#define JACK_POLE_DEC		BIT(1) // JACK Pole Decision
#define JACK_3POLE			BIT(2) // 3 Pole Inserted
#define JACK_4POLE			BIT(3) // 4 Pole Inserted
#define JACK_AUX				BIT(4) // Aux Inserted
#define JACK_OMTP				BIT(5) // OMTP Inserted
#define JACK_CMP				BIT(6) // Completely Jack In
#define JACK_SJI				BIT(7) // Slightly Jack In
#define JACK_WTP_DEC			BIT(8) // Water Decision
#define JACK_WTP_IN			BIT(9) // Water Jack In
#define JACK_WTP_OUT			BIT(10) // Water Jack Out
#define JACK_WTP_AGAIN		BIT(11) // Water Read Again
#define JACK_WTP_POLL		BIT(12) // Water Polling
#define JACK_WTP_POLL_DONE	BIT(13) // Water Polling Done
#define JACK_IN				(BIT(2) | BIT(3) | BIT(4) | BIT(5))
#define JACK_WTP_ST			(BIT(8) | BIT(9) | BIT(10) | BIT(11) | BIT(12) | BIT(13))

/* NUM_JACK_IRQ */
#define S5M3500X_NUM_JACK_IRQ				8

/* Workqueue Delay */
#define LDET_CHK_DELAY						0
#define JACK_MDET_DELAY						50
#define BTN_ADC_DELAY						0
#define HMU_INIT_DELAY						100

/* Threshold for Water Decision */
#define S5M3500X_WTP_THD_DEFAULT			-1
#define S5M3500X_WTP_THD_LOW_RANGE			0
#define S5M3500X_WTP_THD_MID_RANGE			1
#define S5M3500X_WTP_THD_HIGH_RANGE		2

#define S5M3500X_WTP_THD_LOW_DEFAULT		66

/* default range for mic adc */
#define S5M3500X_MIC_ADC_RANGE_0			25
#define S5M3500X_MIC_ADC_RANGE_1			659

/* JACK MASK */
#define S5M3500X_JACK_MASK					(SND_JACK_HEADPHONE|SND_JACK_HEADSET|SND_JACK_LINEOUT)

/* BUTTON MASK */
#define S5M3500X_BUTTON_MASK		(SND_JACK_BTN_0 | SND_JACK_BTN_1 | SND_JACK_BTN_2 | SND_JACK_BTN_3)

enum {
	JACK_HANDLER_EVENT_NONE = 0,
};

/* IRQ return type */
enum {
	IRQ_ST_JACKOUT = 0,
	IRQ_ST_CMP_JACK_IN,
	IRQ_ST_SLIGHT_JACK_IN,
	IRQ_ST_POLE_DEC,
	IRQ_ST_WTJACK_POLLING,
	IRQ_ST_WTJACK_POLLING_DONE,
	IRQ_ST_ADC_READ_DONE,
	IRQ_ST_BTN_DET,
	IRQ_ST_ERR,
};

/* ADC PIN Selection */
enum {
	JACK_LDET_ADC = 0,
	JACK_MDET_ADC,
};

/* Button state */
enum {
	BUTTON_RELEASE = 0,
	BUTTON_PRESS,
};

/* Button range */
struct jack_btn_zone {
	int adc_low;
	int adc_high;
};

struct s5m3500x_priv;

/* Jack information struct */
struct s5m3500x_jack {
	/* Codec Driver default */
	struct snd_soc_component *codec;
	struct s5m3500x_priv *p_s5m3500x;

	/* IRQ */
	int irqb_gpio;
	int codec_irq;
	u8 irq_val[S5M3500X_NUM_JACK_IRQ];

	/* jack device */
	struct snd_soc_jack headset_jack;

	/* jack device */
	struct snd_soc_jack headset_button;

	/* jack detection notifier */
	struct notifier_block s5m3500x_jack_det_nb;

	/* mutex */
	struct mutex key_lock;
	struct mutex adc_lock;
	struct mutex jack_det_lock;
	struct wakeup_source *jack_wakeup;

	/* Workqueue */
	struct delayed_work ldet_chk_work;
	struct workqueue_struct *ldet_chk_wq;

	struct delayed_work jack_det_work;
	struct workqueue_struct *jack_det_wq;

	struct delayed_work init_hmu_work;
	struct workqueue_struct *init_hmu_wq;

	struct delayed_work btn_release_work;
	struct workqueue_struct *btn_release_wq;

	struct delayed_work btn_press_work;
	struct workqueue_struct *btn_press_wq;

	/* wtp ldet thd */
	int wtp_ldet_thd[3];

	/* buttons zone */
	struct jack_btn_zone btn_zones[4];

	/* MicBias */
	int mic_bias1_voltage;
	int mic_bias2_voltage;

	/* status */
	int prv_jack_state;
	int cur_jack_state;
	int btn_state;
	int mic_adc_range[2];
	bool gpadc_done;
};

/* probe/remove */
int s5m3500x_jack_probe(struct snd_soc_component *codec, struct s5m3500x_priv *s5m3500x);
int s5m3500x_jack_remove(struct snd_soc_component *codec);

/* common function */
irqreturn_t s5m3500x_jack_irq_thread(int irq, void *irq_data);
void jack_wake_lock(struct wakeup_source *ws);
void jack_wake_unlock(struct wakeup_source *ws);
int jack_set_wake_lock(struct s5m3500x_jack *jackdet);
void jack_unregister_wake_lock(struct s5m3500x_jack *jackdet);
int s5m3500x_get_irq_type(struct s5m3500x_priv *s5m3500x, unsigned int event);
void s5m3500x_register_jack_controls(struct snd_soc_component *codec, struct s5m3500x_priv *s5m3500x);
void s5m3500x_jack_variable_initialize(struct s5m3500x_priv *s5m3500x, struct s5m3500x_jack *jackdet);
void s5m3500x_common_parse_dt(struct s5m3500x_priv *s5m3500x);
int s5m3500x_request_threaded_irq(struct s5m3500x_priv *s5m3500x, irqreturn_t (*s5m3500x_irq_thread)(int, void *));
int s5m3500x_register_notifier(struct s5m3500x_priv *s5m3500x, int (*s5m3500x_notifier_handler)(struct notifier_block *, unsigned long, void *));
int s5m3500x_register_jack(struct snd_soc_component *codec, struct s5m3500x_priv *s5m3500x, int type);
int s5m3500x_register_button(struct snd_soc_component *codec, struct s5m3500x_priv *s5m3500x, int type);
void s5m3500x_report_jack_type(struct s5m3500x_priv *s5m3500x, int type, int insert);
void s5m3500x_report_button_type(struct s5m3500x_priv *s5m3500x, int type, int state, int btn_adc);
bool s5m3500x_button_error_check(struct s5m3500x_priv *s5m3500x, int btn_irq_state);
void s5m3500x_call_notifier(struct s5m3500x_priv *s5m3500x, int insert);
void s5m3500x_read_irq(struct s5m3500x_priv *s5m3500x);
void s5m3500x_wtp_threshold_initialize(struct s5m3500x_priv *s5m3500x);
void s5m3500x_configure_mic_bias(struct s5m3500x_priv *s5m3500x);
void s5m3500x_jack_register_exit(struct snd_soc_component *codec);
bool s5m3500x_check_jack_state_sequence(struct s5m3500x_priv *s5m3500x);
char *s5m3500x_return_status_name(unsigned int status);
char *s5m3500x_print_jack_type_state(int cur_jack_state);
void s5m3500x_print_status_change(struct device *dev, unsigned int prv_jack, unsigned int cur_jack);
void s5m3500x_auto_boost_reset(struct s5m3500x_priv *s5m3500x);

#endif
#ifndef _SEC_AUDIO_EXYNOS_H
#define _SEC_AUDIO_EXYNOS_H

enum rmem_size_type {
	TYPE_ABOX_DBG_SIZE,
	TYPE_ABOX_SLOG_SIZE,
	TYPE_SIZE_MAX,
};

enum abox_debug_err_type {
	TYPE_ABOX_NOERROR,
	TYPE_ABOX_DATAABORT = 1,
	TYPE_ABOX_PREFETCHABORT,
	TYPE_ABOX_OSERROR,
	TYPE_ABOX_VSSERROR,
	TYPE_ABOX_UNDEFEXCEPTION,
	TYPE_ABOX_DEBUGASSERT,
	TYPE_ABOX_DEBUG_MAX,
};

struct sec_audio_exynos_data {
	struct device *dev;

	/* sec_audio_debug callback */
	void (*abox_log_extra_copy)(char *, unsigned int, unsigned int, unsigned int);
	void (*set_modem_event)(int);
	void (*abox_debug_string_update)(enum abox_debug_err_type, int, int, int, int, unsigned long long);

	/* sec_audio_sysfs callback */
	void (*send_adsp_silent_reset_ev)(void);
};

#if IS_ENABLED(CONFIG_SND_SOC_SAMSUNG_AUDIO)
/* sec_audio_debug callback */
int register_abox_log_extra_copy(void (*abox_log_extra_copy) (char *, unsigned int, unsigned int, unsigned int));
int register_set_modem_event(void (*set_modem_event) (int));
int register_abox_debug_string_update(void (*abox_debug_string_update) (enum abox_debug_err_type, int, int, int, int, unsigned long long));
void abox_log_extra_copy(char *, unsigned int, unsigned int, unsigned int);
void set_modem_event(int);
void abox_debug_string_update(enum abox_debug_err_type, int, int, int, int, unsigned long long);
int check_upload_mode_disabled(void);
int check_debug_level_low(void);
size_t get_rmem_size_min(enum rmem_size_type);

/* sec_audio_sysfs callback */
int register_send_adsp_silent_reset_ev(void (*send_adsp_silent_reset_ev) (void));
void send_adsp_silent_reset_ev(void);
#else
/* sec_audio_debug callback */
inline int register_abox_log_extra_copy(void (*abox_log_extra_copy) (char *, unsigned int, unsigned int, unsigned int))
{
	return -EACCES;
}

inline int register_set_modem_event(void (*set_modem_event) (int))
{
	return -EACCES;
}

inline int register_abox_debug_string_update(void (*abox_debug_string_update) (int, int, int, int, int, unsigned long long))
{
	return -EACCES;
}

inline void abox_log_extra_copy(char *src_base, unsigned int index_reader,
		unsigned int index_writer, unsigned int src_buff_size)
{

}

inline void set_modem_event(int event)
{

}

inline void abox_debug_string_update(int type, int p1, int p2, int p3,
				int audio_mode, unsigned long long audio_mode_time)
{

}

inline int check_upload_mode_disabled(void)
{
	return 1;
}

inline int check_debug_level_low(void)
{
	return 0;
}

inline size_t get_rmem_size_min(int id)
{
	return 0xab0cab0c;
}

/* sec_audio_sysfs callback */
inline int register_send_adsp_silent_reset_ev(void (*send_adsp_silent_reset_ev) (void))
{
	return -EACCES;
}

inline void send_adsp_silent_reset_ev(void)
{

}
#endif

#endif /* _SEC_AUDIO_EXYNOS_H */

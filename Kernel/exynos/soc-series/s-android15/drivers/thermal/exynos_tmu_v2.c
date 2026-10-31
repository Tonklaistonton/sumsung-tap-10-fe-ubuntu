/*
 * exynos_tmu.c - Samsung EXYNOS TMU (Thermal Management Unit)
 *
 *  Copyright (C) 2014 Samsung Electronics
 *  Bartlomiej Zolnierkiewicz <b.zolnierkie@samsung.com>
 *  Lukasz Majewski <l.majewski@samsung.com>
 *
 *  Copyright (C) 2011 Samsung Electronics
 *  Donggeun Kim <dg77.kim@samsung.com>
 *  Amit Daniel Kachhap <amit.kachhap@linaro.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <linux/delay.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/cpufreq.h>
#include <linux/suspend.h>
#include <linux/threads.h>
#include <linux/thermal.h>
#include <soc/samsung/gpu_cooling.h>
#include <soc/samsung/isp_cooling.h>
#include <linux/slab.h>
#include <linux/debugfs.h>
#include <uapi/linux/sched/types.h>
#include <soc/samsung/exynos/debug-snapshot.h>
#if IS_ENABLED(CONFIG_DEBUG_SNAPSHOT)
#include <soc/samsung/exynos/debug-snapshot-log.h>
#endif
#include <soc/samsung/tmu.h>
#include <soc/samsung/ect_parser.h>
#include <soc/samsung/exynos-mcinfo.h>
#include <soc/samsung/cal-if.h>
#include <linux/pm_qos.h>
#include <soc/samsung/exynos-cpupm.h>
#include <linux/pm_opp.h>

#include "exynos_tmu.h"
#include "../../../../../common/drivers/thermal/thermal_core.h"
#include "exynos_acpm_tmu.h"
#include <soc/samsung/exynos-pmu-if.h>
#if IS_ENABLED(CONFIG_EXYNOS_CPUHP)
#include <soc/samsung/exynos-cpuhp.h>
#endif
#if IS_ENABLED(CONFIG_SCHED_EMS)
#include <linux/ems.h>
#endif

#include <kunit/visibility.h>

#define CREATE_TRACE_POINTS
#include <trace/events/thermal_exynos.h>

#if IS_ENABLED(CONFIG_EXYNOS_ESCA_THERMAL)
#include <linux/workqueue.h>
#include <linux/jiffies.h>
#endif

#define EXYNOS_GPU_THERMAL_ZONE_ID		(3)

#define FRAC_BITS 10
#define int_to_frac(x) ((x) << FRAC_BITS)
#define frac_to_int(x) ((x) >> FRAC_BITS)

#define INVALID_TRIP -1

/**
 * struct freq_table - frequency table along with power entries
 * @frequency:	frequency in KHz
 * @power:	power in mW
 *
 * This structure is built when the cooling device registers and helps
 * in translating frequency to power and vice versa.
 */
struct freq_table {
	u32 frequency;
	u32 power;
};

/**
 * mul_frac() - multiply two fixed-point numbers
 * @x:	first multiplicand
 * @y:	second multiplicand
 *
 * Return: the result of multiplying two fixed-point numbers.  The
 * result is also a fixed-point number.
 */
static inline s64 mul_frac(s64 x, s64 y)
{
	return (x * y) >> FRAC_BITS;
}

/**
 * div_frac() - divide two fixed-point numbers
 * @x:	the dividend
 * @y:	the divisor
 *
 * Return: the result of dividing two fixed-point numbers.  The
 * result is also a fixed-point number.
 */
static inline s64 div_frac(s64 x, s64 y)
{
	return div_s64(x << FRAC_BITS, y);
}
/* list of multiple instance for each thermal sensor */
static LIST_HEAD(dtm_dev_list);

#if IS_ENABLED(CONFIG_EXYNOS_ESCA_THERMAL)
static u32 callback_state[NOTI_STATUS_DATA_NUM];

struct exynos_esca_tmu_callback {
	int id;
	struct kthread_worker worker;
	struct kthread_work work;
	unsigned int cmd[4];
};
struct exynos_esca_tmu_callback esca_tmu_cb;
static int exynos_tmu_idle_ip_index;
#else
struct kthread_worker hotplug_worker;
#endif

static struct acpm_tmu_cap cap;
static unsigned int num_of_devices;
static atomic_t suspended_count;
static bool cp_call_mode;
static bool is_aud_on(void)
{
	unsigned int val;

	val = abox_is_on();

	pr_info("%s AUD_STATUS %d\n", __func__, val);

	return val;
}

#if IS_ENABLED(CONFIG_EXYNOS_ESCA_THERMAL)
static int exynos_thermal_zone_trip_id(struct thermal_zone_device *tz,
			 const struct thermal_trip *trip)
{
	int i;

	for (i = 0; i < tz->num_trips; i++) {
		if (&tz->trips[i] == trip)
			return i;
	}

	return -ENODATA;
}

void exynos_thermal_instance_update(struct thermal_instance *instance)
{
	int trip_id;
	struct exynos_tmu_data *data = (struct exynos_tmu_data *)
		instance->tz->devdata;

	trip_id = exynos_thermal_zone_trip_id(instance->tz, instance->trip);
	exynos_acpm_tmu_instance_update(instance->cdev->id, trip_id,
			INSTANCE_CDEV_ID, instance->cdev->id);

	trip_id = exynos_thermal_zone_trip_id(instance->tz, instance->trip);
	exynos_acpm_tmu_instance_update(instance->cdev->id, trip_id,
		INSTANCE_TZ_ID, data->id);

	trip_id = exynos_thermal_zone_trip_id(instance->tz, instance->trip);
	exynos_acpm_tmu_instance_update(instance->cdev->id, trip_id,
			INSTANCE_UPPER, instance->upper);

	trip_id = exynos_thermal_zone_trip_id(instance->tz, instance->trip);
	exynos_acpm_tmu_instance_update(instance->cdev->id, trip_id,
			INSTANCE_LOWER, instance->lower);
}
EXPORT_SYMBOL_GPL(exynos_thermal_instance_update);
#endif

#if IS_ENABLED(CONFIG_EXYNOS_ESCA_THERMAL)
static struct exynos_tmu_data *exynos_tmu_get_data_from_id(int id)
{
	struct exynos_tmu_data *data = NULL;

	list_for_each_entry(data, &dtm_dev_list, node)
		if (data->id == id)
			break;

	return data;
}

#if IS_ENABLED(CONFIG_SCHED_EMS)
void exynos_tmu_callback_hotplug(char cpuhp_name[], bool hotplug)
{
	struct exynos_tmu_data *data;
	struct cpumask mask;

	list_for_each_entry(data, &dtm_dev_list, node) {
		if (!strncmp(data->tmu_name, cpuhp_name, 4))
			break;
	}
	if (hotplug) {
		cpumask_andnot(&mask, cpu_possible_mask, &data->cpu_domain);
		if (data->hotplug_completely_off)
			exynos_cpuhp_update_request(data->cpuhp_name, &mask);
		else
			ecs_request(data->cpuhp_name, &mask, ECS_MAX);
		pr_info("[esca_acpm_tmu] %s hotplug_out: 0x%02x\n",
				data->cpuhp_name, *(unsigned int *)cpumask_bits(&mask));
	} else {
		if (data->hotplug_completely_off)
			exynos_cpuhp_update_request(data->cpuhp_name,
						    cpu_possible_mask);
		else
			ecs_request(data->cpuhp_name, cpu_possible_mask, ECS_MAX);
		pr_info("[esca_acpm_tmu] %s hotplug_in: 0x%02x\n",
				data->cpuhp_name, *(unsigned int *)cpumask_bits(cpu_possible_mask));
	}
}
#else
void exynos_tmu_callback_hotplug(char cpuhp_name[], bool hotplug)
{
	return ;
}
#endif

#if IS_ENABLED(CONFIG_ISP_THERMAL)
static void exynos_tmu_callback_isp_throttle(u32 cooling_state)
{
	struct exynos_tmu_data *data;
	struct thermal_zone_device *tzd;
	struct thermal_instance *instance;
	struct thermal_cooling_device *cdev;

	list_for_each_entry(data, &dtm_dev_list, node) {
		if (!strncmp(data->tmu_name, "ISP", 3))
			break;
	}

	tzd = data->tzd;
	list_for_each_entry(instance, &tzd->thermal_instances, tz_node) {
		if (instance->cdev) {
			instance->target = cooling_state;
			cdev = instance->cdev;
			mutex_lock(&cdev->lock);
			cdev->updated = false;
			mutex_unlock(&cdev->lock);
			thermal_cdev_update(cdev);
		}
	}
	pr_info("[esca_acpm_tmu] isp throttle to state %d\n", cooling_state);
}
#endif

static void exynos_tmu_callback_pi_polling(u32 polling_state)
{
	if (polling_state)
		exynos_update_ip_idle_status(exynos_tmu_idle_ip_index, 0);
	else
		exynos_update_ip_idle_status(exynos_tmu_idle_ip_index, 1);
}

#if IS_ENABLED(CONFIG_SOC_S5E9945)
#else
static volatile u32 extern_cur_state_phy;
static void __iomem *extern_cur_state_base;
static u32 extern_cur_state_size;

static int get_extern_cur_state(struct thermal_cooling_device *cdev,
				unsigned long *state)
{
	u32 offset = sizeof(u32) * cdev->id;

	if (!extern_cur_state_base
		|| offset + sizeof(u32) >= extern_cur_state_size) {
		dev_err(&cdev->device,
			"failed to get extern_cur_state: %u, %u\n",
			extern_cur_state_size, offset);
		return -ENODEV;
	}

	*state = (unsigned long)readl(extern_cur_state_base + offset);

	return 0;
}
#endif

static void exynos_tmu_callback_cdev_update(u32 cdev_list)
{
	struct exynos_tmu_data *data;
	struct thermal_zone_device *tzd;
	struct thermal_instance *instance;
	int i;

	for (i = 0; i < NOTI_STATUS_CDEV_UPDATE_NUM; i++) {
		if (!(0x1 << i & cdev_list))
			continue;
		data = exynos_tmu_get_data_from_id(i);
		tzd = data->tzd;

		list_for_each_entry(instance, &tzd->thermal_instances, tz_node) {
			struct thermal_cooling_device *cdev =
				instance->cdev;
			unsigned long cdev_state = 0;
			int ret = 0;

			if (!cdev)
				continue;

#if IS_ENABLED(CONFIG_SOC_S5E9945)
			ret = cdev->ops->get_cur_state(cdev, &cdev_state);
#else
			ret = get_extern_cur_state(cdev, &cdev_state);
#endif
			if (ret)
				continue;

			instance->target = cdev_state;
			mutex_lock(&cdev->lock);
			cdev->updated = false;
			mutex_unlock(&cdev->lock);
			thermal_cdev_update(cdev);
		}
#if IS_ENABLED(CONFIG_SOC_S5E9945)
		exynos_acpm_tmu_clear_cdev_update(data->id);
#endif
	}
}

static void exynos_tmu_esca_callback_work(struct kthread_work *work)
{
	u32 state[3];
	u32 cur_state_check = 0, state_check = 0;
	u32 partial_state = 0;

	state[0] = esca_tmu_cb.cmd[1];
	state[1] = esca_tmu_cb.cmd[2];
	state[2] = esca_tmu_cb.cmd[3];

	cur_state_check = callback_state[NOTI_STATUS_DATA_POS2] &
			  NOTI_STATUS_BIG_HOTPLUG_MASK;
	state_check = state[NOTI_STATUS_DATA_POS2] &
		      NOTI_STATUS_BIG_HOTPLUG_MASK;

	if (cur_state_check != state_check) {
		exynos_tmu_callback_hotplug("BIG", state_check);
		if (state_check)
			callback_state[NOTI_STATUS_DATA_POS2] |= NOTI_STATUS_BIG_HOTPLUG_MASK;
		else
			callback_state[NOTI_STATUS_DATA_POS2] &= ~NOTI_STATUS_BIG_HOTPLUG_MASK;
	}

	cur_state_check = callback_state[NOTI_STATUS_DATA_POS2] &
			  NOTI_STATUS_MID_HOTPLUG_MASK;;
	state_check = state[NOTI_STATUS_DATA_POS2] &
		      NOTI_STATUS_MID_HOTPLUG_MASK;

	if (cur_state_check != state_check) {
		exynos_tmu_callback_hotplug("MID", state_check);
		if (state_check)
			callback_state[NOTI_STATUS_DATA_POS2] |= NOTI_STATUS_MID_HOTPLUG_MASK;
		else
			callback_state[NOTI_STATUS_DATA_POS2] &= ~NOTI_STATUS_MID_HOTPLUG_MASK;
	}

	cur_state_check = callback_state[NOTI_STATUS_DATA_POS2] &
			  NOTI_STATUS_MIDH_HOTPLUG_MASK;;
	state_check = state[NOTI_STATUS_DATA_POS2] &
		      NOTI_STATUS_MIDH_HOTPLUG_MASK;

	if (cur_state_check != state_check) {
		exynos_tmu_callback_hotplug("MIDH", state_check);
		if (state_check)
			callback_state[NOTI_STATUS_DATA_POS2] |= NOTI_STATUS_MIDH_HOTPLUG_MASK;
		else
			callback_state[NOTI_STATUS_DATA_POS2] &= ~NOTI_STATUS_MIDH_HOTPLUG_MASK;
	}

	cur_state_check = callback_state[NOTI_STATUS_DATA_POS2] &
			  NOTI_STATUS_MIDL_HOTPLUG_MASK;;
	state_check = state[NOTI_STATUS_DATA_POS2] &
		      NOTI_STATUS_MIDL_HOTPLUG_MASK;

	if (cur_state_check != state_check) {
		exynos_tmu_callback_hotplug("MIDL", state_check);
		if (state_check)
			callback_state[NOTI_STATUS_DATA_POS2] |= NOTI_STATUS_MIDL_HOTPLUG_MASK;
		else
			callback_state[NOTI_STATUS_DATA_POS2] &= ~NOTI_STATUS_MIDL_HOTPLUG_MASK;
	}

#if IS_ENABLED(CONFIG_ISP_THERMAL)
	cur_state_check = callback_state[NOTI_STATUS_DATA_POS2] &
			  NOTI_STATUS_ISP_THROTTLE_MASK;
	state_check = state[NOTI_STATUS_DATA_POS2] &
		      NOTI_STATUS_ISP_THROTTLE_MASK;

	if (cur_state_check != state_check) {
		exynos_tmu_callback_isp_throttle(state_check >>
				NOTI_STATUS_ISP_THROTTLE_SHIFT);
		callback_state[NOTI_STATUS_DATA_POS2] &= ~NOTI_STATUS_ISP_THROTTLE_MASK;
		callback_state[NOTI_STATUS_DATA_POS2] |= state_check;
	}
#endif
	partial_state = state[NOTI_STATUS_DATA_POS2] >>
			NOTI_STATUS_CDEV_UPDATE_SHIFT;
	exynos_tmu_callback_cdev_update(partial_state);

#if IS_ENABLED(CONFIG_SOC_S5E9955) // DSU throttle WA start
#include <linux/exynos-dsufreq.h>
	if (state[NOTI_STATUS_DATA_POS1]) {
		static u32 old_throttle, new_throttle;
		static struct dev_pm_qos_request max_req;
		static int qos_added = -1;

		if (qos_added < 0)
			qos_added = dsufreq_qos_add_request("thermal", &max_req,
					DEV_PM_QOS_MAX_FREQUENCY,
					new_throttle);

		if (qos_added < 0)
			return;

		new_throttle = state[NOTI_STATUS_DATA_POS1];
		if (old_throttle != new_throttle) {
			dev_pm_qos_update_request(&max_req, new_throttle);
			old_throttle = new_throttle;
		}
	}
#endif // DSU throttle WA end
}

static void exynos_tmu_callback_func(unsigned int *cmd, unsigned int size)
{
	u32 cur_state_check, state_check;
	u32 state[3];

	esca_tmu_cb.cmd[0] = cmd[0];
	esca_tmu_cb.cmd[1] = cmd[1];
	esca_tmu_cb.cmd[2] = cmd[2];
	esca_tmu_cb.cmd[3] = cmd[3];

	state[0] = esca_tmu_cb.cmd[1];
	state[1] = esca_tmu_cb.cmd[2];
	state[2] = esca_tmu_cb.cmd[3];

	cur_state_check = callback_state[NOTI_STATUS_DATA_POS2] &
			  NOTI_STATUS_POLLING_MASK;
	state_check = state[NOTI_STATUS_DATA_POS2] &
		      NOTI_STATUS_POLLING_MASK;

	if (cur_state_check != state_check) {
		exynos_tmu_callback_pi_polling(state_check >>
				NOTI_STATUS_POLLING_SHIFT);
		if (state_check)
			callback_state[NOTI_STATUS_DATA_POS2] |= NOTI_STATUS_POLLING_MASK;
		else
			callback_state[NOTI_STATUS_DATA_POS2] &= ~NOTI_STATUS_POLLING_MASK;
	}

	kthread_queue_work(&esca_tmu_cb.worker, &esca_tmu_cb.work);
}
#else

#if IS_ENABLED(CONFIG_SCHED_EMS)
static void exynos_throttle_cpu_hotplug(struct kthread_work *work)
{
	struct exynos_tmu_data *data = container_of(work,
			struct exynos_tmu_data, hotplug_work);
	struct cpumask mask;

	mutex_lock(&data->lock);

	if (data->is_cpu_hotplugged_out) {
		if (data->temperature < data->hotplug_in_threshold) {
			/*
			 * If current temperature is lower than low threshold,
			 * call cluster1_cores_hotplug(false) for hotplugged out cpus.
			 */
			ecs_request(data->cpuhp_name, cpu_possible_mask, ECS_MAX);
			data->is_cpu_hotplugged_out = false;
		}
	} else {
		if (data->temperature >= data->hotplug_out_threshold) {
			/*
			 * If current temperature is higher than high threshold,
			 * call cluster1_cores_hotplug(true) to hold temperature down.
			 */
			data->is_cpu_hotplugged_out = true;
			cpumask_andnot(&mask, cpu_possible_mask, &data->cpu_domain);
			ecs_request(data->cpuhp_name, &mask, ECS_MAX);
		}
	}

	mutex_unlock(&data->lock);
}
#endif
#endif
static int thermal_status_level[3];
static bool update_thermal_status;

int exynos_build_static_power_table(struct device_node *np, int **var_table,
		unsigned int *var_volt_size, unsigned int *var_temp_size, char *tz_name)
{
	int i, j, ret = -EINVAL;
	int ratio = 0, asv_group, cal_id;
	struct ect_gen_param_table *pwr_coeff;
	u32 index;

	void *gen_block;
	struct ect_gen_param_table *volt_temp_param = NULL, *asv_param = NULL;
	int cpu_ratio_table[16] = { 0, 18, 22, 27, 33, 40, 49, 60, 73, 89, 108, 131, 159, 194, 232, 250};
	int g3d_ratio_table[16] = { 0, 25, 29, 35, 41, 48, 57, 67, 79, 94, 110, 130, 151, 162, 162, 162};
	int *ratio_table, *var_coeff_table, *asv_coeff_table;

	if (of_property_read_u32(np, "cal-id", &cal_id)) {
		if (of_property_read_u32(np, "g3d_cmu_cal_id", &cal_id)) {
			pr_err("%s: Failed to get cal-id\n", __func__);
			return -EINVAL;
		}
	}

	gen_block = ect_get_block("GEN");
	if (gen_block == NULL) {
		pr_err("%s: Failed to get gen block from ECT\n", __func__);
		return ret;
	}

	if (!of_property_read_u32(np, "ect-coeff-index", &index)) {
		pwr_coeff = ect_gen_param_get_table(gen_block, "DTM_PWR_Coeff_2");
		if (pwr_coeff)
			ratio = pwr_coeff->parameter[index];
	}
	asv_group = cal_asv_get_grp(cal_id);

	if (asv_group < 0 || asv_group > 15)
		asv_group = 0;

	if (!strcmp(tz_name, "MID")) {
		volt_temp_param = ect_gen_param_get_table(gen_block, "DTM_MID_VOLT_TEMP");
		asv_param = ect_gen_param_get_table(gen_block, "DTM_MID_ASV");
		ratio_table = cpu_ratio_table;
	}
	else if (!strcmp(tz_name, "BIG")) {
		volt_temp_param = ect_gen_param_get_table(gen_block, "DTM_BIG_VOLT_TEMP");
		asv_param = ect_gen_param_get_table(gen_block, "DTM_BIG_ASV");
		ratio_table = cpu_ratio_table;
	}
	else if (!strcmp(tz_name, "G3D")) {
		volt_temp_param = ect_gen_param_get_table(gen_block, "DTM_G3D_VOLT_TEMP");
		asv_param = ect_gen_param_get_table(gen_block, "DTM_G3D_ASV");
		ratio_table = g3d_ratio_table;
	}
	else if (!strcmp(tz_name, "LITTLE")) {
		volt_temp_param = ect_gen_param_get_table(gen_block, "DTM_LIT_VOLT_TEMP");
		asv_param = ect_gen_param_get_table(gen_block, "DTM_LIT_ASV");
		ratio_table = cpu_ratio_table;
	}
	else if (!strcmp(tz_name, "MIDH")) {
		volt_temp_param = ect_gen_param_get_table(gen_block, "DTM_MIDH_VOLT_TEMP");
		asv_param = ect_gen_param_get_table(gen_block, "DTM_MIDH_ASV");
		ratio_table = cpu_ratio_table;
	}
	else if (!strcmp(tz_name, "MIDL")) {
		volt_temp_param = ect_gen_param_get_table(gen_block, "DTM_MIDL_VOLT_TEMP");
		asv_param = ect_gen_param_get_table(gen_block, "DTM_MIDL_ASV");
		ratio_table = cpu_ratio_table;
	}
	else {
		pr_err("%s: Thermal zone %s does not use PIDTM\n", __func__, tz_name);
		return -EINVAL;
	}

	if (asv_group == 0)
		asv_group = 8;

	if (!ratio)
		ratio = ratio_table[asv_group];

	if (volt_temp_param && asv_param) {
		*var_volt_size = volt_temp_param->num_of_row - 1;
		*var_temp_size = volt_temp_param->num_of_col - 1;

		var_coeff_table = kzalloc(sizeof(int) *
							volt_temp_param->num_of_row *
							volt_temp_param->num_of_col,
							GFP_KERNEL);
		if (!var_coeff_table)
			goto err_mem;

		asv_coeff_table = kzalloc(sizeof(int) *
							asv_param->num_of_row *
							asv_param->num_of_col,
							GFP_KERNEL);
		if (!asv_coeff_table)
			goto free_var_coeff;

		*var_table = kzalloc(sizeof(int) *
							volt_temp_param->num_of_row *
							volt_temp_param->num_of_col,
							GFP_KERNEL);
		if (!*var_table)
			goto free_asv_coeff;

		memcpy(var_coeff_table, volt_temp_param->parameter,
			sizeof(int) * volt_temp_param->num_of_row * volt_temp_param->num_of_col);
		memcpy(asv_coeff_table, asv_param->parameter,
			sizeof(int) * asv_param->num_of_row * asv_param->num_of_col);
		memcpy(*var_table, volt_temp_param->parameter,
			sizeof(int) * volt_temp_param->num_of_row * volt_temp_param->num_of_col);
	} else {
		pr_err("%s: Failed to get param table from ECT\n", __func__);
		return -EINVAL;
	}

	for (i = 1; i <= *var_volt_size; i++) {
		long asv_coeff = (long)asv_coeff_table[3 * i + 0] * asv_group * asv_group
				+ (long)asv_coeff_table[3 * i + 1] * asv_group
				+ (long)asv_coeff_table[3 * i + 2];
		asv_coeff = asv_coeff / 100;

		for (j = 1; j <= *var_temp_size; j++) {
			long var_coeff = (long)var_coeff_table[i * (*var_temp_size + 1) + j];
			var_coeff =  ratio * var_coeff * asv_coeff;
			var_coeff = var_coeff / 100000;
			(*var_table)[i * (*var_temp_size + 1) + j] = (int)var_coeff;
		}
	}

	ret = 0;

free_asv_coeff:
	kfree(asv_coeff_table);
free_var_coeff:
	kfree(var_coeff_table);
err_mem:
	return ret;
}
EXPORT_SYMBOL_GPL(exynos_build_static_power_table);


#if IS_ENABLED(CONFIG_EXYNOS_ESCA_THERMAL)
#else
static void exynos_report_trigger(struct exynos_tmu_data *p)
{
	struct thermal_zone_device *tz = p->tzd;

	if (!tz) {
		pr_err("No thermal zone device defined\n");
		return;
	}

	thermal_zone_device_update(tz, THERMAL_EVENT_UNSPECIFIED);
}
#endif

static int exynos_tmu_initialize(struct platform_device *pdev)
{
	struct exynos_tmu_data *data = platform_get_drvdata(pdev);
	struct thermal_zone_device *tz = data->tzd;
	enum thermal_trip_type type;
	int i, temp;
	unsigned char threshold[8] = {0, };
	unsigned char inten = 0;
	struct thermal_trip _trip;

	mutex_lock(&data->lock);

	for (i = (tz->num_trips - 1); i >= 0; i--) {
		__thermal_zone_get_trip(tz, i, &_trip);
		type = _trip.type;

		if (type == THERMAL_TRIP_PASSIVE)
			continue;

		temp = _trip.temperature;

		threshold[i] = (unsigned char)(temp / MCELSIUS);
		inten |= (1 << i);
	}
	exynos_acpm_tmu_set_threshold(data->id, threshold);
	exynos_acpm_tmu_set_interrupt_enable(data->id, inten);

	mutex_unlock(&data->lock);

	return 0;
}

static void exynos_tmu_control(struct platform_device *pdev, bool on)
{
	struct exynos_tmu_data *data = platform_get_drvdata(pdev);

	mutex_lock(&data->lock);
	exynos_acpm_tmu_tz_control(data->id, on);
	data->enabled = on;
	mutex_unlock(&data->lock);
}

static int exynos_tmu_set_trip_temp(struct thermal_zone_device *tz, int trip,
		int temp)
{
	struct exynos_tmu_data *data = tz->devdata;

	if (!data)
		return -ENODEV;

	tz->trips[trip].temperature = temp;
	exynos_acpm_tmu_set_tmu_data(data->id, TMU_DATA_TRIP_TEMP, trip, temp / MCELSIUS);

	return 0;
}

#if IS_ENABLED(CONFIG_EXYNOS_ESCA_THERMAL)
static void __iomem *exynos_tmu_remap(unsigned long addr, unsigned int size)
{
	int i;
	unsigned int num_pages = ((addr + size) >> PAGE_SHIFT)
					- (addr >> PAGE_SHIFT)+ 1;
	pgprot_t prot = pgprot_writecombine(PAGE_KERNEL);
	struct page **pages = NULL;
	void __iomem *v_addr = NULL;
	unsigned long offset;

	if (!addr)
		return 0;

	offset = addr & (PAGE_SIZE - 1);
	addr = (addr >> PAGE_SHIFT) << PAGE_SHIFT;

	pages = kmalloc_array(num_pages, sizeof(struct page *), GFP_ATOMIC);
	if (!pages)
		return 0;

	for (i = 0; i < num_pages; i++) {
		pages[i] = phys_to_page(addr);
		addr += PAGE_SIZE;
	}

	v_addr = vmap(pages, num_pages, VM_MAP, prot);
	kfree(pages);

	return v_addr + offset;
}

struct tmu_dbginfo {
	u64 entry[14];
} *tmu_dbginfo;

struct entry_info {
	u32 info0;
	u8 info1;
	u8 info2;
	u8 info3;
	u8 info4;
};
static atomic_t tmu_dbginfo_idx;
static int log_num;
static void __iomem *tmu_dump_base;
static volatile u32 tmu_dump_phy;
#endif

static int exynos_get_temp(struct thermal_zone_device *tz, int *temp)
{
	struct exynos_tmu_data *data = tz->devdata;
	int acpm_temp = 0, stat = 0;
	u32 acpm_data[3];
	unsigned long long dbginfo = 0;

	if (!data || !data->enabled)
		return -EINVAL;

	mutex_lock(&data->lock);

	exynos_acpm_tmu_set_read_temp(data->id, &acpm_temp, &stat, acpm_data);

	*temp = acpm_temp * MCELSIUS;

	// Update thermal status
	if (update_thermal_status) {
		ktime_t diff;
		ktime_t cur_time = ktime_get() / NSEC_PER_MSEC;

		diff = cur_time - data->last_thermal_status_updated;

		if (data->last_thermal_status_updated == 0 && *temp >= thermal_status_level[0]) {
			data->last_thermal_status_updated = cur_time;
		} else if (*temp >= thermal_status_level[2]) {
			data->thermal_status[2] += diff;
			data->last_thermal_status_updated = cur_time;
		} else if (*temp >= thermal_status_level[1]) {
			data->thermal_status[1] += diff;
			data->last_thermal_status_updated = cur_time;
		} else if (*temp >= thermal_status_level[0]) {
			data->thermal_status[0] += diff;
			data->last_thermal_status_updated = cur_time;
		} else {
			data->last_thermal_status_updated = 0;
		}
	}

	data->temperature = *temp / 1000;

#if IS_ENABLED(CONFIG_EXYNOS_ESCA_THERMAL)
#else
	if (data->hotplug_enable &&
			((data->is_cpu_hotplugged_out && data->temperature < data->hotplug_in_threshold) ||
			(!data->is_cpu_hotplugged_out && data->temperature >= data->hotplug_out_threshold)))
		kthread_queue_work(&hotplug_worker, &data->hotplug_work);
#endif

#if IS_ENABLED(CONFIG_EXYNOS_ESCA_THERMAL)
	if (tmu_dump_base) {
		int k;
		struct entry_info *entry_info;
		dbginfo = atomic_fetch_inc(&tmu_dbginfo_idx) % log_num;
		for (k = 0; k < acpm_data[1]; k++) {
			if (!tmu_dbginfo)
				break;
			tmu_dbginfo[dbginfo].entry[k] = __raw_readq(tmu_dump_base +
							  acpm_data[0]
							  + sizeof(u64) * k);
			entry_info = (struct entry_info *)&tmu_dbginfo[dbginfo].entry[k];
			data->dfs_triggered |= ((entry_info->info0 >> 6) & 0x1);
		}
		dbginfo |= (((u64)acpm_data[1]) << 32);
		dbginfo |= (((u64)acpm_data[2]) << 40);
	}
#else
	dbginfo = (((unsigned long long)acpm_data[0])
		   | (((unsigned long long)acpm_data[1]) << 32));
#endif
	dbg_snapshot_thermal(data, *temp / 1000, data->tmu_name, dbginfo);
	mutex_unlock(&data->lock);

	return 0;
}

static int exynos_get_trend(struct thermal_zone_device *tz, const struct thermal_trip *trip, enum thermal_trend *trend)
{
	*trend = THERMAL_TREND_STABLE;

	return 0;
}

#if defined(CONFIG_THERMAL_EMULATION)
static int exynos_tmu_set_emulation(struct thermal_zone_device *tz, int temp)
{
	struct exynos_tmu_data *data = tz->devdata;
	int ret = -EINVAL;
	unsigned char emul_temp;

	if (temp && temp < MCELSIUS)
		goto out;

	mutex_lock(&data->lock);
	emul_temp = (unsigned char)(temp / MCELSIUS);
	exynos_acpm_tmu_set_emul_temp(data->id, emul_temp);
	mutex_unlock(&data->lock);
	return 0;
out:
	return ret;
}
#else
static int exynos_tmu_set_emulation(void *drv_data, int temp)
{ return -EINVAL; }
#endif /* CONFIG_THERMAL_EMULATION */

static void start_pi_polling(struct exynos_tmu_data *data, int delay)
{
#if IS_ENABLED(CONFIG_EXYNOS_ESCA_THERMAL)
	exynos_acpm_tmu_start_pi_polling(data->id);
#else
	kthread_mod_delayed_work(&data->thermal_worker, &data->pi_work,
			msecs_to_jiffies(delay));
#endif
}

static void reset_pi_trips(struct exynos_tmu_data *data)
{
	struct thermal_zone_device *tz = data->tzd;
	struct exynos_pi_param *params = data->pi_param;
	int i, last_active;
	bool found_first_passive;
	struct thermal_trip _trip;

	found_first_passive = false;
	last_active = INVALID_TRIP;

	for (i = 0; i < tz->num_trips; i++) {
		enum thermal_trip_type type;
		int ret;

		ret = thermal_zone_get_trip(tz, i, &_trip);
		type = _trip.type;
		if (ret) {
			dev_warn(&tz->device,
				 "Failed to get trip point %d type: %d\n", i,
				 ret);
			continue;
		}

		if (type == THERMAL_TRIP_PASSIVE) {
			if (!found_first_passive) {
				params->trip_switch_on = i;
				found_first_passive = true;
				break;
			}
		} else if (type == THERMAL_TRIP_ACTIVE) {
			last_active = i;
		} else {
			break;
		}
	}

	if (found_first_passive) {
		params->trip_control_temp = params->trip_switch_on;
		params->trip_switch_on = last_active;
	} else {
		params->trip_switch_on = INVALID_TRIP;
		params->trip_control_temp = last_active;
	}
}

#if IS_ENABLED(CONFIG_EXYNOS_ESCA_THERMAL)
#else
static void reset_pi_params(struct exynos_tmu_data *data)
{
	s64 i = int_to_frac(data->pi_param->i_max);

	data->pi_param->err_integral = div_frac(i, data->pi_param->k_i);
	data->pi_param->prev_err = -1;
}

static void allow_maximum_power(struct exynos_tmu_data *data)
{
	struct thermal_instance *instance;
	struct thermal_zone_device *tz = data->tzd;
	struct exynos_pi_param *params = data->pi_param;

	list_for_each_entry(instance, &tz->thermal_instances, tz_node) {
		if ((instance->trip != params->trip_control_temp) ||
		    (!__cdev_is_power_actor(instance->cdev)))
			continue;

		instance->target = 0;
		mutex_lock(&instance->cdev->lock);
		instance->cdev->updated = false;
		mutex_unlock(&instance->cdev->lock);
		thermal_cdev_update(instance->cdev);
	}
}

static u32 pi_calculate(struct exynos_tmu_data *data,
			 int control_temp,
			 u32 max_allocatable_power)
{
	struct thermal_zone_device *tz = data->tzd;
	struct exynos_pi_param *params = data->pi_param;
	s64 p, i, power_range, d = 0;
	s32 err, max_power_frac;

	max_power_frac = int_to_frac(max_allocatable_power);

	err = (control_temp - tz->temperature) / 1000;
	err = int_to_frac(err);

	/* Calculate the proportional term */
	p = mul_frac(err < 0 ? params->k_po : params->k_pu, err);

	/*
	 * Calculate the integral term
	 *
	 * if the error is less than cut off allow integration (but
	 * the integral is limited to max power)
	 */
	i = mul_frac(params->k_i, params->err_integral);

	if (err < int_to_frac(params->integral_cutoff)) {
		s64 i_next = i + mul_frac(params->k_i, err);
		s64 i_windup = int_to_frac(-1 * (s64)params->sustainable_power);

		if (i_next > int_to_frac((s64)params->i_max)) {
			i = int_to_frac((s64)params->i_max);
			params->err_integral = div_frac(i, params->k_i);
		} else if (i_next <= i_windup) {
			i = i_windup;
			params->err_integral = div_frac(i, params->k_i);
		} else {
			i = i_next;
			params->err_integral += err;
		}
	}

	if (params->k_d && params->prev_err != -1)
		d = mul_frac(params->k_d, err - params->prev_err);
	else
		d = 0;

	params->prev_err = err;

	power_range = p + i + d;

	power_range = params->sustainable_power + frac_to_int(power_range);

	power_range = clamp(power_range, (s64)0, (s64)max_allocatable_power);

	trace_thermal_exynos_power_allocator_pid(tz, frac_to_int(err),
						 frac_to_int(params->err_integral),
						 frac_to_int(p), frac_to_int(i),
						 frac_to_int(d), power_range);

	return power_range;
}

static int exynos_pi_controller(struct exynos_tmu_data *data, int control_temp)
{
	struct thermal_zone_device *tz = data->tzd;
	struct exynos_pi_param *params = data->pi_param;
	struct thermal_instance *instance, *main_instance, *sub_instance;
	struct thermal_cooling_device *cdev;
	int ret = 0;
	bool found_actor = false;
	u32 max_power, power_range;
	unsigned long state;
	struct thermal_cooling_device *sub_cdev = NULL;

	list_for_each_entry(instance, &tz->thermal_instances, tz_node) {
		if ((instance->trip == params->trip_control_temp) &&
		    __cdev_is_power_actor(instance->cdev)) {
			if (data->use_sync_pi_thermal) {
				struct exynos_cpufreq_cooling_device *cpufreq_cdev = instance->cdev->devdata;
				struct thermal_zone_device *cdev_tz = cpufreq_cdev ? cpufreq_cdev->tz : NULL;

				if (cdev_tz && (tz->id == cdev_tz->id)) {
					found_actor = true;
					main_instance = instance;
					cdev = instance->cdev;
				} else {
					sub_instance = instance;
					sub_cdev = instance->cdev;
				}
			} else {
				found_actor = true;
				main_instance = instance;
				cdev = instance->cdev;
			}
		}
	}

	if (!found_actor)
		return -ENODEV;

	cdev->ops->state2power(cdev, 0, &max_power);

	power_range = pi_calculate(data, control_temp, max_power);

	ret = cdev->ops->power2state(cdev, power_range, &state);
	if (ret)
		return ret;

	if (data->use_sync_pi_thermal && sub_cdev) {
		unsigned long freq;
		u32 sub_state, cpu, sub_cpu;
		struct dev_pm_opp *opp, *sub_opp;

		cpu = ((struct exynos_cpufreq_cooling_device *)(cdev->devdata))->policy->cpu;
		sub_cpu = ((struct exynos_cpufreq_cooling_device *)(sub_cdev->devdata))->policy->cpu;

		freq = ((struct exynos_cpufreq_cooling_device *)(cdev->devdata))->freq_table[state].frequency * 1000;
		opp = dev_pm_opp_find_freq_ceil(get_cpu_device(cpu), &freq);
		sub_opp = dev_pm_opp_find_freq_ceil_by_volt(get_cpu_device(sub_cpu), dev_pm_opp_get_voltage(opp));

		if (!IS_ERR(sub_opp) && dev_pm_opp_get_freq(sub_opp) < freq)
			sub_opp = dev_pm_opp_find_freq_floor(get_cpu_device(sub_cpu), &freq);

		if (IS_ERR(sub_opp)) {
			unsigned long min_freq = 0;

			sub_opp = dev_pm_opp_find_freq_ceil(get_cpu_device(sub_cpu), &min_freq);
		}

		sub_state = cpufreq_cooling_get_level(sub_cpu, dev_pm_opp_get_freq(sub_opp) / 1000);

		if (sub_cdev != NULL) {
			sub_instance->target = sub_state;
			mutex_lock(&sub_cdev->lock);
			sub_cdev->updated = false;
			mutex_unlock(&sub_cdev->lock);
			thermal_cdev_update(sub_cdev);
		}
	}

	main_instance->target = state;
	mutex_lock(&cdev->lock);
	cdev->updated = false;
	mutex_unlock(&cdev->lock);
	thermal_cdev_update(cdev);

	trace_thermal_exynos_power_allocator(tz, power_range,
					     max_power, tz->temperature,
					     control_temp - tz->temperature);

	return ret;
}
#endif
struct exynos_tmu_data *exynos_tmu_get_data_from_tz(struct thermal_zone_device *tz)
{
	struct exynos_tmu_data *data = NULL;

	list_for_each_entry(data, &dtm_dev_list, node)
		if (data->tzd == tz)
			break;

	return data;
}
EXPORT_SYMBOL_GPL(exynos_tmu_get_data_from_tz);

#if IS_ENABLED(CONFIG_EXYNOS_ESCA_THERMAL)
#else
static unsigned long get_target_state(struct thermal_instance *instance,
				enum thermal_trend trend, bool throttle)
{
	struct thermal_cooling_device *cdev = instance->cdev;
	unsigned long cur_state;
	unsigned long next_target;

	/*
	 * We keep this instance the way it is by default.
	 * Otherwise, we use the current state of the
	 * cdev in use to determine the next_target.
	 */
	cdev->ops->get_cur_state(cdev, &cur_state);
	next_target = instance->target;
	dev_dbg(&cdev->device, "cur_state=%ld\n", cur_state);

	if (!instance->initialized) {
		if (throttle) {
			next_target = (cur_state + 1) >= instance->upper ?
					instance->upper :
					((cur_state + 1) < instance->lower ?
					instance->lower : (cur_state + 1));
		} else {
			next_target = THERMAL_NO_TARGET;
		}

		return next_target;
	}

	switch (trend) {
	case THERMAL_TREND_RAISE_FULL:
		if (throttle)
			next_target = instance->upper;
		break;
	case THERMAL_TREND_DROP_FULL:
		if (cur_state == instance->lower) {
			if (!throttle)
				next_target = THERMAL_NO_TARGET;
		} else
			next_target = instance->lower;
		break;
	default:
		break;
	}

	return next_target;
}

static void update_passive_instance(struct thermal_zone_device *tz,
				enum thermal_trip_type type, int value)
{
	/*
	 * If value is +1, activate a passive instance.
	 * If value is -1, deactivate a passive instance.
	 */
	if (type == THERMAL_TRIP_PASSIVE)
		tz->passive += value;
}

static void thermal_zone_trip_update(struct thermal_zone_device *tz, int trip)
{
	struct thermal_trip _trip;
	int trip_temp;
	enum thermal_trip_type trip_type;
	enum thermal_trend trend;
	struct thermal_instance *instance;
	bool throttle = false;
	int old_target;

	thermal_zone_get_trip(tz, trip, &_trip);
	trip_temp = _trip.temperature;
	trip_type = _trip.type

	if (tz->temperature >= trip_temp)
		trend = THERMAL_TREND_RAISE_FULL;
	else
		trend = THERMAL_TREND_DROP_FULL;

	if (tz->temperature >= trip_temp) {
		throttle = true;
	}

	dev_dbg(&tz->device, "Trip%d[type=%d,temp=%d]:trend=%d,throttle=%d\n",
				trip, trip_type, trip_temp, trend, throttle);

	mutex_lock(&tz->lock);

	list_for_each_entry(instance, &tz->thermal_instances, tz_node) {
		if (instance->trip != trip)
			continue;

		old_target = instance->target;
		instance->target = get_target_state(instance, trend, throttle);
		dev_dbg(&instance->cdev->device, "old_target=%d, target=%d\n",
					old_target, (int)instance->target);

		if (instance->initialized && old_target == instance->target)
			continue;

		/* Activate a passive thermal instance */
		if (old_target == THERMAL_NO_TARGET &&
			instance->target != THERMAL_NO_TARGET)
			update_passive_instance(tz, trip_type, 1);
		/* Deactivate a passive thermal instance */
		else if (old_target != THERMAL_NO_TARGET &&
			instance->target == THERMAL_NO_TARGET)
			update_passive_instance(tz, trip_type, -1);

		instance->initialized = true;
		mutex_lock(&instance->cdev->lock);
		instance->cdev->updated = false; /* cdev needs update */
		mutex_unlock(&instance->cdev->lock);
	}

	mutex_unlock(&tz->lock);
}

/**
 * step_wise_throttle - throttles devices associated with the given zone
 * @tz: thermal_zone_device
 * @trip: trip point index
 *
 * Throttling Logic: This uses the trend of the thermal zone to throttle.
 * If the thermal zone is 'heating up' this throttles all the cooling
 * devices associated with the zone and its particular trip point, by one
 * step. If the zone is 'cooling down' it brings back the performance of
 * the devices by one step.
 */
static int step_wise_throttle(struct thermal_zone_device *tz, int trip)
{
	struct thermal_instance *instance;

	thermal_zone_trip_update(tz, trip);

	mutex_lock(&tz->lock);

	list_for_each_entry(instance, &tz->thermal_instances, tz_node)
		thermal_cdev_update(instance->cdev);

	mutex_unlock(&tz->lock);

	return 0;
}

static void exynos_step_thermal(struct exynos_tmu_data *data)
{
	struct thermal_zone_device *tz = data->tzd;
	int i = 0;

	for (i = 0; i < tz->num_trips; i++)
		step_wise_throttle(tz, i); 
}

static void exynos_pi_thermal(struct exynos_tmu_data *data)
{

	struct thermal_zone_device *tz = data->tzd;
	struct exynos_pi_param *params = data->pi_param;
	int ret = 0;
	int switch_on_temp, control_temp, delay;
	struct thermal_trip _trip;

	if (atomic_read(&data->in_suspend))
		return;

	if (data->tzd) {
		if (!(data->tzd->mode == THERMAL_DEVICE_ENABLED)) {
			params->switched_on = false;
			mutex_lock(&data->lock);
			goto polling;
		}
	}

	thermal_zone_device_update(tz, THERMAL_EVENT_UNSPECIFIED);
	mutex_lock(&data->lock);

	ret = thermal_zone_get_trip(tz, params->trip_switch_on, &_trip);
	switch_on_temp = _trip.temperature;

	if (!ret && (tz->temperature < switch_on_temp)) {
		reset_pi_params(data);
		allow_maximum_power(data);
		params->switched_on = false;
		goto polling;
	}

	params->switched_on = true;


	ret = thermal_zone_get_trip(tz, params->control_temp, &_trip);
	control_temp = _trip.temperature;

	if (ret) {
		pr_warn("Failed to get the maximum desired temperature: %d\n",
			 ret);
		goto polling;
	}

	ret = exynos_pi_controller(data, control_temp);

	if (ret) {
		pr_debug("Failed to calculate pi controller: %d\n",
			 ret);
		goto polling;
	}

polling:
	if (params->switched_on)
		delay = data->polling_delay_on;
	else
		delay = data->polling_delay_off;

	if (!atomic_read(&data->in_suspend))
		start_pi_polling(data, delay);
	mutex_unlock(&data->lock);
}
#endif

#if IS_ENABLED(CONFIG_EXYNOS_ESCA_THERMAL)
struct delayed_work tmu_temp_polling_work;

static void exynos_tmu_polling(struct work_struct *work)
{
	struct exynos_tmu_data *data = NULL;

	list_for_each_entry(data, &dtm_dev_list, node) {
		if (data->tzd && data->external_polling)
			thermal_zone_device_update(data->tzd, THERMAL_EVENT_UNSPECIFIED);
	}

	schedule_delayed_work_on(0, &tmu_temp_polling_work, msecs_to_jiffies(1000));
}

static int exynos_tmu_polling_work_init(struct platform_device *pdev)
{
	INIT_DELAYED_WORK(&tmu_temp_polling_work, exynos_tmu_polling);

	schedule_delayed_work_on(0, &tmu_temp_polling_work, msecs_to_jiffies(1000));

	return 0;
}
#else
static void exynos_pi_polling(struct kthread_work *work)
{
	struct exynos_tmu_data *data =
			container_of(work, struct exynos_tmu_data, pi_work.work);

	exynos_pi_thermal(data);
}
#endif

#if IS_ENABLED(CONFIG_EXYNOS_ESCA_THERMAL)
#else
static void exynos_tmu_work(struct kthread_work *work)
{
	struct exynos_tmu_data *data =
			container_of(work, struct exynos_tmu_data, irq_work);

	mutex_lock(&data->lock);

	exynos_acpm_tmu_clear_tz_irq(data->id);

	mutex_unlock(&data->lock);

	exynos_report_trigger(data);

	if (data->use_pi_thermal)
		exynos_pi_thermal(data);
	else
		exynos_step_thermal(data);

	enable_irq(data->irq);
}

static irqreturn_t exynos_tmu_irq(int irq, void *id)
{
	struct exynos_tmu_data *data = id;

	disable_irq_nosync(irq);
	kthread_queue_work(&data->thermal_worker, &data->irq_work);

	return IRQ_HANDLED;
}
#endif

static int exynos_tmu_pm_notify(struct notifier_block *nb,
			     unsigned long mode, void *_unused)
{
	struct exynos_tmu_data *data = container_of(nb,
			struct exynos_tmu_data, nb);

	switch (mode) {
	case PM_HIBERNATION_PREPARE:
	case PM_RESTORE_PREPARE:
	case PM_SUSPEND_PREPARE:
		if (data->use_pi_thermal)
			mutex_lock(&data->lock);

		atomic_set(&data->in_suspend, 1);

		if (data->use_pi_thermal) {
			mutex_unlock(&data->lock);
#if IS_ENABLED(CONFIG_EXYNOS_ESCA_THERMAL)
#else
			kthread_cancel_delayed_work_sync(&data->pi_work);
#endif
		}
#if IS_ENABLED(CONFIG_EXYNOS_ESCA_THERMAL)
		if (data->polling_delay_on || data->polling_delay_off)
			exynos_acpm_tmu_stop_pi_polling(data->id);
#endif
		break;
	case PM_POST_HIBERNATION:
	case PM_POST_RESTORE:
	case PM_POST_SUSPEND:
		atomic_set(&data->in_suspend, 0);
		if (data->use_pi_thermal){
#if IS_ENABLED(CONFIG_EXYNOS_ESCA_THERMAL)
#else
			exynos_pi_thermal(data);
#endif
		}
#if IS_ENABLED(CONFIG_EXYNOS_ESCA_THERMAL)
		if (data->polling_delay_on || data->polling_delay_off)
			exynos_acpm_tmu_start_pi_polling(data->id);
#endif
		break;
	default:
		break;
	}
	return 0;
}

static const struct of_device_id exynos_tmu_match[] = {
	{ .compatible = "samsung,exynos-tmu-v2", },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, exynos_tmu_match);

#if IS_ENABLED(CONFIG_EXYNOS_ESCA_THERMAL)
static int exynos_tmu_esca_callback_work_init(struct platform_device *pdev)
{
	struct cpumask mask;
	struct task_struct *thread;
	struct sched_param param = { .sched_priority = MAX_RT_PRIO / 4 - 1 };
	int ret = 0;
	struct device_node *np;
	const char *buf;

	kthread_init_worker(&esca_tmu_cb.worker);
	thread = kthread_create(kthread_worker_fn,
			&esca_tmu_cb.worker,
			"thermal_esca_callback_worker");
	if (IS_ERR(thread)) {
		dev_err(&pdev->dev,
				"failed to created thermal callback worker: %ld\n",
				PTR_ERR(thread));
		return PTR_ERR(thread);
	}

	np = of_find_node_by_path("/exynos-cpufreq/domain@0");
	if (!np) {
		dev_warn(&pdev->dev, "cannot find /exynos-cpufreq/domain@0\n");
		return ret;
	}

	ret = of_property_read_string(np, "sibling-cpus", &buf);
	if (ret) {
		dev_warn(&pdev->dev, "cannot find sibling-cpus\n");
		return ret;
	}

	dev_info(&pdev->dev, "esca_cb_work is gonna be bound to cpu %s\n", buf);

	cpulist_parse(buf, &mask);
	cpumask_and(&mask, cpu_possible_mask, &mask);
	set_cpus_allowed_ptr(thread, &mask);

	ret = sched_setscheduler_nocheck(thread, SCHED_FIFO, &param);
	if (ret) {
		kthread_stop(thread);
		dev_warn(&pdev->dev,
				"esca thermal failed to set SCHED_FIFO\n");
		return ret;
	}
	wake_up_process(thread);
	kthread_init_work(&esca_tmu_cb.work,
			exynos_tmu_esca_callback_work);

	exynos_tmu_idle_ip_index = exynos_get_idle_ip_index("EXYNOS_TMU", 1);
	exynos_update_ip_idle_status(exynos_tmu_idle_ip_index, 1);

	return 0;
}
#else
static int exynos_tmu_irq_work_init(struct platform_device *pdev)
{
	struct exynos_tmu_data *data = platform_get_drvdata(pdev);
	struct cpumask mask;
	struct sched_param param = { .sched_priority = MAX_RT_PRIO / 4 - 1 };
	struct task_struct *thread;
	int ret = 0;

	kthread_init_worker(&data->thermal_worker);
	thread = kthread_create(kthread_worker_fn, &data->thermal_worker,
			"thermal_%s", data->tmu_name);
	if (IS_ERR(thread)) {
		dev_err(&pdev->dev, "failed to create thermal thread: %ld\n",
				PTR_ERR(thread));
		return PTR_ERR(thread);
	}

	cpulist_parse("0-3", &mask);
	cpumask_and(&mask, cpu_possible_mask, &mask);
	set_cpus_allowed_ptr(thread, &mask);

	ret = sched_setscheduler_nocheck(thread, SCHED_FIFO, &param);
	if (ret) {
		kthread_stop(thread);
		dev_warn(&pdev->dev, "thermal failed to set SCHED_FIFO\n");
		return ret;
	}

	kthread_init_work(&data->irq_work, exynos_tmu_work);

	wake_up_process(thread);

#if IS_ENABLED(CONFIG_SCHED_EMS)
	if (data->hotplug_enable) {
		kthread_init_work(&data->hotplug_work, exynos_throttle_cpu_hotplug);
		snprintf(data->cpuhp_name, THERMAL_NAME_LENGTH, "DTM_%s", data->tmu_name);
		if (data->hotplug_completely_off)
			exynos_cpuhp_add_request(data->cpuhp_name, cpu_possible_mask);
		else
			ecs_request_register(data->cpuhp_name, cpu_possible_mask, ECS_MAX);
	}
#endif

	return ret;
}
#endif

int exynos_thermal_get_tz_temps(int *temps, int size)
{
	struct exynos_tmu_data *data = NULL;
	int id_max = 0;

	list_for_each_entry(data, &dtm_dev_list, node) {
		int *temp = &temps[data->id];

		if (data->id >= size)
			return -ENOMEM;

		if (!data->tzd)
			continue;

		id_max = data->id > id_max ? data->id : id_max;

		exynos_get_temp(data->tzd, temp);
	}

	return id_max;
}
EXPORT_SYMBOL_GPL(exynos_thermal_get_tz_temps);

static int exynos_map_dt_data(struct platform_device *pdev)
{
	struct exynos_tmu_data *data = platform_get_drvdata(pdev);
	const char *tmu_name;
	int ret;

	if (!data || !pdev->dev.of_node)
		return -ENODEV;

	data->np = pdev->dev.of_node;

	if (of_property_read_u32(pdev->dev.of_node, "id", &data->id)) {
		dev_err(&pdev->dev, "failed to get TMU ID\n");
		return -ENODEV;
	}

	if (!IS_ENABLED(CONFIG_EXYNOS_ESCA_THERMAL)) {
		struct resource res;
		data->irq = irq_of_parse_and_map(pdev->dev.of_node, 0);
		if (data->irq <= 0) {
			dev_err(&pdev->dev, "failed to get IRQ\n");
			return -ENODEV;
		}

		if (of_address_to_resource(pdev->dev.of_node, 0, &res)) {
			dev_err(&pdev->dev, "failed to get Resource 0\n");
			return -ENODEV;
		}

		data->base = devm_ioremap(&pdev->dev,
					  res.start, resource_size(&res));
		if (!data->base) {
			dev_err(&pdev->dev, "Failed to ioremap memory\n");
			return -EADDRNOTAVAIL;
		}
	}

	if (of_property_read_string(pdev->dev.of_node, "tmu_name", &tmu_name)) {
		dev_err(&pdev->dev, "failed to get tmu_name\n");
	} else
		strncpy(data->tmu_name, tmu_name, THERMAL_NAME_LENGTH);

	if (of_property_read_bool(pdev->dev.of_node, "thermal_status_level")) {
		of_property_read_u32_array(pdev->dev.of_node, "thermal_status_level", (u32 *)&thermal_status_level,
				(size_t)(ARRAY_SIZE(thermal_status_level)));
		update_thermal_status = true;
	}

#if IS_ENABLED(CONFIG_SCHED_EMS)
	data->hotplug_enable = of_property_read_bool(pdev->dev.of_node, "hotplug_enable");
	if (data->hotplug_enable) {
		const char *buf;

		dev_info(&pdev->dev, "thermal zone use hotplug function \n");
		of_property_read_u32(pdev->dev.of_node, "hotplug_in_threshold",
					&data->hotplug_in_threshold);
		if (!data->hotplug_in_threshold)
			dev_err(&pdev->dev, "No input hotplug_in_threshold \n");

		of_property_read_u32(pdev->dev.of_node, "hotplug_out_threshold",
					&data->hotplug_out_threshold);
		if (!data->hotplug_out_threshold)
			dev_err(&pdev->dev, "No input hotplug_out_threshold \n");

		if (of_property_read_bool(pdev->dev.of_node,
					  "hotplug_completely_off"))
			data->hotplug_completely_off = true;

		ret = of_property_read_string(pdev->dev.of_node, "cpu_domain", &buf);
		if (!ret)
			cpulist_parse(buf, &data->cpu_domain);
	}
#endif

	of_property_read_u32(pdev->dev.of_node, "polling_delay_on",
				&data->polling_delay_on);
	if (!data->polling_delay_on)
		dev_err(&pdev->dev, "No input polling_delay_on \n");

	of_property_read_u32(pdev->dev.of_node, "polling_delay_off",
				&data->polling_delay_off);
	if (!data->polling_delay_off)
		dev_err(&pdev->dev, "No input polling_delay_off \n");

	if (of_property_read_bool(pdev->dev.of_node, "use-pi-thermal")) {
		struct exynos_pi_param *params;
		u32 value;

		data->use_pi_thermal = true;

		params = kzalloc(sizeof(*params), GFP_KERNEL);
		if (!params)
			return -ENOMEM;

		data->use_sync_pi_thermal = of_property_read_bool(pdev->dev.of_node, "use-sync-pi-thermal");

		ret = of_property_read_u32(pdev->dev.of_node, "k_po",
					   &value);
		if (ret < 0)
			dev_err(&pdev->dev, "No input k_po\n");
		else
			params->k_po = int_to_frac(value);


		ret = of_property_read_u32(pdev->dev.of_node, "k_pu",
					   &value);
		if (ret < 0)
			dev_err(&pdev->dev, "No input k_pu\n");
		else
			params->k_pu = int_to_frac(value);

		ret = of_property_read_u32(pdev->dev.of_node, "k_i",
					   &value);
		if (ret < 0)
			dev_err(&pdev->dev, "No input k_i\n");
		else
			params->k_i = int_to_frac(value);

		ret = of_property_read_s32(pdev->dev.of_node, "k_d",
					   &value);
		if (ret > 0)
			params->k_d = int_to_frac(value);

		ret = of_property_read_u32(pdev->dev.of_node, "i_max",
					   &value);
		if (ret < 0)
			dev_err(&pdev->dev, "No input i_max\n");
		else
			params->i_max = int_to_frac(value);

		ret = of_property_read_u32(pdev->dev.of_node, "integral_cutoff",
					   &value);
		if (ret < 0)
			dev_err(&pdev->dev, "No input integral_cutoff\n");
		else
			params->integral_cutoff = value;

		ret = of_property_read_u32(pdev->dev.of_node, "sustainable_power",
					   &value);
		if (ret < 0)
			dev_err(&pdev->dev, "No input sustainable_power\n");
		else
			params->sustainable_power = value;

		data->pi_param = params;
	} else {
		data->use_pi_thermal = false;
	}

	return 0;
}

static const struct thermal_zone_device_ops exynos_hotplug_sensor_ops = {
	.get_temp = exynos_get_temp,
	.set_trip_temp = exynos_tmu_set_trip_temp,
	.set_emul_temp = exynos_tmu_set_emulation,
	.get_trend = exynos_get_trend,
};


#if IS_ENABLED(CONFIG_EXYNOS_ESCA_THERMAL)
static ssize_t
tmu_mode_show(struct device *dev, struct device_attribute *devattr,
		char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct exynos_tmu_data *data = platform_get_drvdata(pdev);
	u32 mode;

	exynos_acpm_tmu_get_tmu_mode(data->id, &mode);

	if (mode == THERMAL_DEVICE_DISABLED) {
		return snprintf(buf, PAGE_SIZE, "TZ[%d] is disabled\n", data->id);
	} else if (mode == THERMAL_DEVICE_ENABLED) {
		return snprintf(buf, PAGE_SIZE, "TZ[%d] is enabled\n", data->id);
	} else {
		return snprintf(buf, PAGE_SIZE,
				"TZ[%d] is unavailable mode %d\n", data->id, mode);
	}
}

static ssize_t
tmu_mode_store(struct device *dev, struct device_attribute *devattr,
		const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct exynos_tmu_data *data = platform_get_drvdata(pdev);
	u32 mode;

	if (kstrtou32(buf, 10, &mode))
		return -EINVAL;

	exynos_acpm_tmu_set_tmu_mode(data->id, mode);

	return count;
}

static ssize_t
polling_external_show(struct device *dev, struct device_attribute *devattr,
		char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct exynos_tmu_data *data = platform_get_drvdata(pdev);

	return snprintf(buf, PAGE_SIZE, "polling_external: %u\n",
			data->external_polling);
}

static ssize_t
polling_external_store(struct device *dev, struct device_attribute *devattr,
		const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct exynos_tmu_data *data = platform_get_drvdata(pdev);
	u32 polling;

	if (!data->use_pi_thermal)
		return -EIO;

	if (kstrtou32(buf, 10, &polling))
		return -EINVAL;

	data->external_polling = !!polling;

	return count;
}

static ssize_t
polling_internal_show(struct device *dev, struct device_attribute *devattr,
		char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct exynos_tmu_data *data = platform_get_drvdata(pdev);

	s64 polling_delay, switched_on;
	s64 is_running;

	if (data->use_pi_thermal) {
		exynos_acpm_tmu_get_pi_params(data->id,
				TMU_DATA_PI_IS_RUNNING, &is_running);
		if (is_running) {
			exynos_acpm_tmu_get_pi_params(data->id,
					TMU_DATA_CUR_POLLING_DELAY, &polling_delay);
			exynos_acpm_tmu_get_pi_params(data->id,
					TMU_DATA_SWITCHED_ON, &switched_on);
			return snprintf(buf, PAGE_SIZE, "switch: %d, delay: %d\n",
					(int)switched_on, (int)polling_delay);
		} else {
			return snprintf(buf, PAGE_SIZE, "Stopped pi polling\n");
		}
	} else {
		return snprintf(buf, PAGE_SIZE, "Not pi thermal zone\n");
	}
}

static ssize_t
polling_internal_store(struct device *dev, struct device_attribute *devattr,
		const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct exynos_tmu_data *data = platform_get_drvdata(pdev);
	u32 polling;

	if (!data->use_pi_thermal)
		return -EIO;

	if (kstrtou32(buf, 10, &polling))
		return -EINVAL;

	if (polling == 1)
		exynos_acpm_tmu_stop_pi_polling(data->id);
	else if (polling == 0)
		exynos_acpm_tmu_start_pi_polling(data->id);
	else
		return -EIO;

	return count;
}

static ssize_t
cooling_dev_state_show(struct device *dev, struct device_attribute *devattr,
		char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct exynos_tmu_data *data = platform_get_drvdata(pdev);
	struct thermal_zone_device *tzd = data->tzd;
	struct thermal_instance *instance;
	unsigned long state_list[16] = {-1, -1, -1, -1, -1, -1, -1, -1,
					-1, -1, -1, -1, -1, -1, -1, -1};
	unsigned long freq_list[16] = {-1, -1, -1, -1, -1, -1, -1, -1,
					-1, -1, -1, -1, -1, -1, -1, -1};
	int idx = 0;

	list_for_each_entry(instance, &tzd->thermal_instances, tz_node) {
		if (instance->cdev) {
			instance->cdev->ops->get_cur_state(instance->cdev,
					&state_list[idx]);
			exynos_acpm_tmu_cdev_get_cur_freq(instance->cdev->id,
					&freq_list[idx]);
			idx++;
		}
	}

	return snprintf(buf, PAGE_SIZE,
			"%8ld %8ld %8ld %8ld %8ld %8ld %8ld %8ld %8ld %8ld %8ld %8ld %8ld %8ld %8ld %8ld\n%8ld %8ld %8ld %8ld %8ld %8ld %8ld %8ld %8ld %8ld %8ld %8ld %8ld %8ld %8ld %8ld\n",
			state_list[0], state_list[1], state_list[2], state_list[3],
			state_list[4], state_list[5], state_list[6], state_list[7],
			state_list[8], state_list[9], state_list[10], state_list[11],
			state_list[12], state_list[13], state_list[14], state_list[15],
			freq_list[0], freq_list[1], freq_list[2], freq_list[3],
			freq_list[4], freq_list[5], freq_list[6], freq_list[7],
			freq_list[8], freq_list[9], freq_list[10], freq_list[11],
			freq_list[12], freq_list[13], freq_list[14], freq_list[15]);
}
#endif

static const struct thermal_zone_device_ops exynos_sensor_ops = {
	.get_temp = exynos_get_temp,
	.set_trip_temp = exynos_tmu_set_trip_temp,
	.set_emul_temp = exynos_tmu_set_emulation,
	.get_trend = exynos_get_trend,
};


static ssize_t
hotplug_out_temp_show(struct device *dev, struct device_attribute *devattr,
		       char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct exynos_tmu_data *data = platform_get_drvdata(pdev);

#if IS_ENABLED(CONFIG_EXYNOS_ESCA_THERMAL)
	int hotplug_out_threshold;

	exynos_acpm_tmu_get_tmu_data(data->id, TMU_DATA_HOTPLUG_OUT_THRESHOLD,
			&hotplug_out_threshold);
	data->hotplug_out_threshold = hotplug_out_threshold;
#endif
	return snprintf(buf, PAGE_SIZE, "%d\n", data->hotplug_out_threshold);
}

static ssize_t
hotplug_out_temp_store(struct device *dev, struct device_attribute *devattr,
			const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct exynos_tmu_data *data = platform_get_drvdata(pdev);
	int hotplug_out = 0;

	mutex_lock(&data->lock);

	if (kstrtos32(buf, 10, &hotplug_out)) {
		mutex_unlock(&data->lock);
		return -EINVAL;
	}
#if IS_ENABLED(CONFIG_EXYNOS_ESCA_THERMAL)
	exynos_acpm_tmu_set_tmu_data(data->id, TMU_DATA_HOTPLUG_OUT_THRESHOLD,
			0, hotplug_out);
#endif
	data->hotplug_out_threshold = hotplug_out;

	mutex_unlock(&data->lock);

	return count;
}

static ssize_t
hotplug_in_temp_show(struct device *dev, struct device_attribute *devattr,
		       char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct exynos_tmu_data *data = platform_get_drvdata(pdev);

#if IS_ENABLED(CONFIG_EXYNOS_ESCA_THERMAL)
	int hotplug_in_threshold;

	exynos_acpm_tmu_get_tmu_data(data->id, TMU_DATA_HOTPLUG_IN_THRESHOLD,
			&hotplug_in_threshold);
	data->hotplug_in_threshold = hotplug_in_threshold;
#endif
	return snprintf(buf, PAGE_SIZE, "%d\n", data->hotplug_in_threshold);
}

static ssize_t
hotplug_in_temp_store(struct device *dev, struct device_attribute *devattr,
			const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct exynos_tmu_data *data = platform_get_drvdata(pdev);
	int hotplug_in = 0;

	mutex_lock(&data->lock);

	if (kstrtos32(buf, 10, &hotplug_in)) {
		mutex_unlock(&data->lock);
		return -EINVAL;
	}
#if IS_ENABLED(CONFIG_EXYNOS_ESCA_THERMAL)
	exynos_acpm_tmu_set_tmu_data(data->id, TMU_DATA_HOTPLUG_IN_THRESHOLD,
			0, hotplug_in);
#endif
	data->hotplug_in_threshold = hotplug_in;

	mutex_unlock(&data->lock);

	return count;
}

#if IS_ENABLED(CONFIG_EXYNOS_EA_DTM)
#else
static ssize_t
sustainable_power_show(struct device *dev, struct device_attribute *devattr,
		       char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct exynos_tmu_data *data = platform_get_drvdata(pdev);

	if (data->pi_param) {
#if IS_ENABLED(CONFIG_EXYNOS_ESCA_THERMAL)
		s64 sustainable_power;

		exynos_acpm_tmu_get_pi_params(data->id,
				TMU_DATA_SUSTAINABLE_POWER,
				&sustainable_power);
		data->pi_param->sustainable_power = (int) sustainable_power;
#endif
		return sprintf(buf, "%u\n",
				(int) data->pi_param->sustainable_power);
	} else {
		return -EIO;
	}
}

static ssize_t
sustainable_power_store(struct device *dev, struct device_attribute *devattr,
			const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct exynos_tmu_data *data = platform_get_drvdata(pdev);
	u32 sustainable_power;

	if (!data->pi_param)
		return -EIO;

	if (kstrtou32(buf, 10, &sustainable_power))
		return -EINVAL;
#if IS_ENABLED(CONFIG_EXYNOS_ESCA_THERMAL)
	exynos_acpm_tmu_set_tmu_data(data->id, TMU_DATA_SUSTAINABLE_POWER,
			0, sustainable_power);
#endif
	data->pi_param->sustainable_power = sustainable_power;

	return count;
}
#endif

#if IS_ENABLED(CONFIG_EXYNOS_ESCA_THERMAL)
static ssize_t
temp_show(struct device *dev, struct device_attribute *devattr,
		       char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct exynos_tmu_data *data = platform_get_drvdata(pdev);
	u32 acpm_data[3], dbginfo = 0;
	int temp = 0, stat = 0, k;
	ssize_t len = 0;

	mutex_lock(&data->lock);

	exynos_acpm_tmu_set_read_temp(data->id, &temp, &stat, acpm_data);
	temp *= MCELSIUS;

	len += snprintf(buf + len, PAGE_SIZE, "id: %d name: %s\n",
			data->id, data->tmu_name);

	if (!tmu_dump_base)
		goto unlock_out;

	dbginfo = atomic_fetch_inc(&tmu_dbginfo_idx) % log_num;

	for (k = 0; k < acpm_data[1]; k++) {
		struct entry_info *entry_info;
		if (!tmu_dbginfo)
			break;
		tmu_dbginfo[dbginfo].entry[k] = __raw_readq(tmu_dump_base +
						  acpm_data[0]
						  + sizeof(u64) * k);
		entry_info = (struct entry_info *)&tmu_dbginfo[dbginfo].entry[k];
		len += snprintf(buf + len, PAGE_SIZE, "0x%08x %3u %3u %3u %3u\n",
					entry_info->info0, entry_info->info1,
					entry_info->info2, entry_info->info3,
					entry_info->info4);
		data->dfs_triggered |= ((entry_info->info0 >> 6) & 0x1);
	}

unlock_out:
	dbg_snapshot_thermal(data, temp / 1000, data->tmu_name, dbginfo);

	mutex_unlock(&data->lock);

	return len;
}
#else
static ssize_t
temp_show(struct device *dev, struct device_attribute *devattr,
		       char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct exynos_tmu_data *tmu_data = platform_get_drvdata(pdev);
	union {
		unsigned int dump[2];
		unsigned char val[8];
	} data;

	exynos_acpm_tmu_ipc_dump(tmu_data->id, data.dump);

	return snprintf(buf, PAGE_SIZE, "%3d %3d %3d %3d %3d %3d %3d\n",
			data.val[1], data.val[2], data.val[3],
			data.val[4], data.val[5], data.val[6], data.val[7]);
}
#endif

#if IS_ENABLED(CONFIG_DEBUG_SNAPSHOT)
static ssize_t thermal_log_show(struct file *file, struct kobject *kobj,
	struct bin_attribute *attr, char *buf, loff_t offset, size_t size)
{
	static u32 front = 0, log_len = 0, i = 0;
	ssize_t count = 0;

	if (!offset) {
		i = 0;
		front = dss_get_first_thermal_log_idx();
		log_len = dss_get_len_thermal_log();
	}

	while (i < log_len && count + 256 < size) {
		struct thermal_log *log = dss_get_thermal_log_iter(front + i);

		count += scnprintf(buf + count, size - count,
				  "%llu %d %s %d %llu\n",
				  log->time, log->cpu, log->cooling_device,
				  log->temp, log->cooling_state);
		i++;
	}

	return count;
}

static ssize_t use_cold_show(struct kobject *kobj, struct kobj_attribute *attr,
		char *buf)
{
	ssize_t count = 0;
	int mode = 0;

	exynos_acpm_tmu_get_use_cold(&mode);

	count += snprintf(buf + count, PAGE_SIZE, "%s\n",
			mode == DISABLE_USE_COLD ? "false" : "true");

	return count;
}

static ssize_t use_cold_store(struct kobject *kobj, struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	int mode = 0;

	if (kstrtou32(buf, 10, &mode))
		return -EINVAL;

	if (mode != 0 && mode != 1) {
		pr_info("%s: usage >> echo [0/1] > /sys/kernel/thermal/use_cold\n", __func__);
		return -EINVAL;
	}

	exynos_acpm_tmu_set_use_cold(mode);

	return count;
}

static struct bin_attribute thermal_log_bin_attr = {
	.attr.name = "thermal_log",
	.attr.mode = 0444,
	.read = thermal_log_show,
};
#endif

static ssize_t thermal_status_show(struct kobject *kobj, struct kobj_attribute *attr,
		char *buf)
{
	ssize_t count = 0;
	struct exynos_tmu_data *devnode;

	count += snprintf(buf + count, PAGE_SIZE, "DOMAIN %10d %10d %10d\n",
			thermal_status_level[0], thermal_status_level[1],
			thermal_status_level[2]);
	list_for_each_entry(devnode, &dtm_dev_list, node) {
#if IS_ENABLED(CONFIG_EXYNOS_ESCA_THERMAL)
		if (devnode->tzd)
			thermal_zone_device_update(devnode->tzd, THERMAL_EVENT_UNSPECIFIED);
#else
		exynos_report_trigger(devnode);
#endif
		mutex_lock(&devnode->lock);
		count += snprintf(buf + count, PAGE_SIZE, "%6s %10llu %10llu %10llu\n",
				devnode->tmu_name, devnode->thermal_status[0],
				devnode->thermal_status[1], devnode->thermal_status[2]);
		devnode->thermal_status[0] = 0;
		devnode->thermal_status[1] = 0;
		devnode->thermal_status[2] = 0;
		mutex_unlock(&devnode->lock);
	}

	return count;
}

static struct kobj_attribute thermal_status_attr = __ATTR(thermal_status, 0440, thermal_status_show, NULL);
static struct kobj_attribute use_cold_attr = __ATTR(use_cold, 0660, use_cold_show, use_cold_store);

#if IS_ENABLED(CONFIG_SEC_PM)
static ssize_t time_in_state_json_show(struct kobject *kobj,
			struct kobj_attribute *attr, char *buf)
{
	ssize_t count = 0;
	struct exynos_tmu_data *devnode;
	int i;

	list_for_each_entry(devnode, &dtm_dev_list, node) {
		mutex_lock(&devnode->lock);
		for (i = 0; i < 3; i++) {
			count += snprintf(buf + count, PAGE_SIZE,
					"\"%s%d\":\"%llu\",",
					devnode->tmu_name, i,
					devnode->thermal_status[i] / MSEC_PER_SEC);
			devnode->thermal_status[i] = 0;
		}
		mutex_unlock(&devnode->lock);
	}

	if (count > 0)
		buf[--count] = '\0';

	return count;
}

static struct kobj_attribute time_in_state_json_attr = __ATTR_RO_MODE(time_in_state_json, 0440);
#endif /* CONFIG_SEC_PM */

#if IS_ENABLED(CONFIG_EXYNOS_ESCA_THERMAL)
#define create_s32_param_attr(name, pos)					\
	static ssize_t								\
name##_show(struct device *dev, struct device_attribute *devattr, 		\
		char *buf)							\
{										\
	struct platform_device *pdev = to_platform_device(dev);			\
	struct exynos_tmu_data *data = platform_get_drvdata(pdev);		\
										\
	if (data->pi_param) {							\
		s64 name;							\
										\
		exynos_acpm_tmu_get_pi_params(data->id, pos, &name);		\
		data->pi_param->name = (int)name;				\
										\
		return sprintf(buf, "%d\n", data->pi_param->name);		\
	} else {								\
		return -EIO;							\
	}									\
}										\
										\
	static ssize_t								\
	name##_store(struct device *dev, struct device_attribute *devattr, 	\
			const char *buf, size_t count)				\
	{									\
		struct platform_device *pdev = to_platform_device(dev);		\
		struct exynos_tmu_data *data = platform_get_drvdata(pdev);	\
		s32 value;							\
										\
		if (!data->pi_param)						\
			return -EIO;						\
										\
		if (kstrtos32(buf, 10, &value))					\
			return -EINVAL;						\
										\
		exynos_acpm_tmu_set_tmu_data(data->id, pos, 0, value);		\
		data->pi_param->name = value;					\
										\
		return count;							\
	}									\
	static DEVICE_ATTR_RW(name)

#else

#define create_s32_param_attr(name)						\
	static ssize_t								\
	name##_show(struct device *dev, struct device_attribute *devattr, 	\
		char *buf)							\
	{									\
	struct platform_device *pdev = to_platform_device(dev);			\
	struct exynos_tmu_data *data = platform_get_drvdata(pdev);		\
										\
	if (data->pi_param)							\
		return sprintf(buf, "%d\n", data->pi_param->name);		\
	else									\
		return -EIO;							\
	}									\
										\
	static ssize_t								\
	name##_store(struct device *dev, struct device_attribute *devattr, 	\
		const char *buf, size_t count)					\
	{									\
		struct platform_device *pdev = to_platform_device(dev);		\
		struct exynos_tmu_data *data = platform_get_drvdata(pdev);	\
		s32 value;							\
										\
		if (!data->pi_param)						\
			return -EIO;						\
										\
		if (kstrtos32(buf, 10, &value))					\
			return -EINVAL;						\
										\
		data->pi_param->name = value;					\
										\
		return count;							\
	}									\
	static DEVICE_ATTR_RW(name)
#endif

#if IS_ENABLED(CONFIG_EXYNOS_ESCA_THERMAL)
static DEVICE_ATTR(cooling_dev_state, S_IWUSR | S_IRUGO, cooling_dev_state_show, NULL);
static DEVICE_ATTR(polling_external, S_IWUSR | S_IRUGO, polling_external_show,
		polling_external_store);
static DEVICE_ATTR(polling_internal, S_IWUSR | S_IRUGO, polling_internal_show,
		polling_internal_store);
static DEVICE_ATTR(tmu_mode, S_IWUSR | S_IRUGO, tmu_mode_show, tmu_mode_store);
#endif
static DEVICE_ATTR(hotplug_out_temp, S_IWUSR | S_IRUGO, hotplug_out_temp_show,
		hotplug_out_temp_store);

static DEVICE_ATTR(hotplug_in_temp, S_IWUSR | S_IRUGO, hotplug_in_temp_show,
		hotplug_in_temp_store);

static DEVICE_ATTR(temp, S_IRUGO, temp_show, NULL);

#if IS_ENABLED(CONFIG_EXYNOS_EA_DTM)
#else
static DEVICE_ATTR_RW(sustainable_power);
#if IS_ENABLED(CONFIG_EXYNOS_ESCA_THERMAL)
create_s32_param_attr(k_po, TMU_DATA_K_PO);
create_s32_param_attr(k_pu, TMU_DATA_K_PU);
create_s32_param_attr(k_i, TMU_DATA_K_I);
create_s32_param_attr(i_max, TMU_DATA_I_MAX);
create_s32_param_attr(integral_cutoff, TMU_DATA_INTEGRAL_CUTOFF);
create_s32_param_attr(k_d, TMU_DATA_K_D);
#else
create_s32_param_attr(k_po);
create_s32_param_attr(k_pu);
create_s32_param_attr(k_i);
create_s32_param_attr(i_max);
create_s32_param_attr(integral_cutoff);
create_s32_param_attr(k_d);
#endif
#endif

static struct attribute *exynos_tmu_attrs[] = {
#if IS_ENABLED(CONFIG_EXYNOS_ESCA_THERMAL)
	&dev_attr_cooling_dev_state.attr,
	&dev_attr_polling_internal.attr,
	&dev_attr_polling_external.attr,
	&dev_attr_tmu_mode.attr,
#endif
	&dev_attr_hotplug_out_temp.attr,
	&dev_attr_hotplug_in_temp.attr,
#if IS_ENABLED(CONFIG_EXYNOS_EA_DTM)
#else
	&dev_attr_sustainable_power.attr,
	&dev_attr_k_po.attr,
	&dev_attr_k_pu.attr,
	&dev_attr_k_i.attr,
	&dev_attr_k_d.attr,
	&dev_attr_i_max.attr,
	&dev_attr_integral_cutoff.attr,
#endif
	&dev_attr_temp.attr,
	NULL,
};

static const struct attribute_group exynos_tmu_attr_group = {
	.attrs = exynos_tmu_attrs,
};

#define PARAM_NAME_LENGTH	25

#if IS_ENABLED(CONFIG_ECT)
static int exynos_tmu_ect_get_param(struct ect_pidtm_block *pidtm_block, char *name)
{
	int i;
	int param_value = -1;

	for (i = 0; i < pidtm_block->num_of_parameter; i++) {
		if (!strncasecmp(pidtm_block->param_name_list[i], name, PARAM_NAME_LENGTH)) {
			param_value = pidtm_block->param_value_list[i];
			break;
		}
	}

	return param_value;
}

static int exynos_tmu_parse_ect(struct exynos_tmu_data *data)
{
	struct thermal_zone_device *tz = data->tzd;
	int ntrips = 0;

	if (!tz)
		return -EINVAL;

	if (!data->use_pi_thermal) {
		/* if pi thermal not used */
		void *thermal_block;
		struct ect_ap_thermal_function *function;
		int i, temperature;
		int hotplug_threshold_temp = 0, hotplug_flag = 0;
		unsigned int freq;

		thermal_block = ect_get_block(BLOCK_AP_THERMAL);
		if (thermal_block == NULL) {
			pr_err("Failed to get thermal block");
			return -EINVAL;
		}

		pr_info("%s %d thermal zone_name = %s\n", __func__, __LINE__, tz->type);

		function = ect_ap_thermal_get_function(thermal_block, tz->type);
		if (function == NULL) {
			pr_err("Failed to get thermal block %s", tz->type);
			return -EINVAL;
		}

		ntrips = tz->num_trips;
		pr_info("Trip count parsed from ECT : %d, ntrips: %d, zone : %s",
			function->num_of_range, ntrips, tz->type);

		for (i = 0; i < function->num_of_range; ++i) {
			temperature = function->range_list[i].lower_bound_temperature;
			freq = function->range_list[i].max_frequency;
			tz->ops->set_trip_temp(tz, i, temperature  * MCELSIUS);

			pr_info("Parsed From ECT : [%d] Temperature : %d, frequency : %u\n",
					i, temperature, freq);

			if (function->range_list[i].flag != hotplug_flag) {
				if (function->range_list[i].flag != hotplug_flag) {
					hotplug_threshold_temp = temperature;
					hotplug_flag = function->range_list[i].flag;
					data->hotplug_out_threshold = temperature;

					if (i)
						data->hotplug_in_threshold = function->range_list[i-1].lower_bound_temperature;

					pr_info("[ECT]hotplug_threshold : %d\n", hotplug_threshold_temp);
					pr_info("[ECT]hotplug_in_threshold : %d\n", data->hotplug_in_threshold);
					pr_info("[ECT]hotplug_out_threshold : %d\n", data->hotplug_out_threshold);
				}
			}

			if (hotplug_threshold_temp != 0)
				data->hotplug_enable = true;
			else
				data->hotplug_enable = false;

		}
	} else {
		void *block;
		struct ect_pidtm_block *pidtm_block;
		struct exynos_pi_param *params;
		int i, temperature, value;
		int hotplug_out_threshold = 0, hotplug_in_threshold = 0;

		block = ect_get_block(BLOCK_PIDTM);
		if (block == NULL) {
			pr_err("Failed to get PIDTM block");
			return -EINVAL;
		}

		pr_info("%s %d thermal zone_name = %s\n", __func__, __LINE__, tz->type);

		pidtm_block = ect_pidtm_get_block(block, tz->type);
		if (pidtm_block == NULL) {
			pr_err("Failed to get PIDTM block %s", tz->type);
			return -EINVAL;
		}

		ntrips = tz->num_trips;
		pr_info("Trip count parsed from ECT : %d, ntrips: %d, zone : %s",
			pidtm_block->num_of_temperature, ntrips, tz->type);

		for (i = 0; i < pidtm_block->num_of_temperature; ++i) {
			temperature = pidtm_block->temperature_list[i];
			tz->ops->set_trip_temp(tz, i, temperature  * MCELSIUS);
			pr_info("Parsed From ECT : [%d] Temperature : %d\n", i, temperature);
		}

		params = data->pi_param;

#if IS_ENABLED(CONFIG_EXYNOS_EA_DTM)
		pr_info("%s: now use EA DTM\n", __func__);
#else
		if ((value = exynos_tmu_ect_get_param(pidtm_block, "k_po")) != -1) {
			pr_info("Parse from ECT k_po: %d\n", value);
			params->k_po = int_to_frac(value);
		} else
			pr_err("Fail to parse k_po parameter\n");

		if ((value = exynos_tmu_ect_get_param(pidtm_block, "k_pu")) != -1) {
			pr_info("Parse from ECT k_pu: %d\n", value);
			params->k_pu = int_to_frac(value);
		} else
			pr_err("Fail to parse k_pu parameter\n");

		if ((value = exynos_tmu_ect_get_param(pidtm_block, "k_i")) != -1) {
			pr_info("Parse from ECT k_i: %d\n", value);
			params->k_i = int_to_frac(value);
		} else
			pr_err("Fail to parse k_i parameter\n");

		if ((value = exynos_tmu_ect_get_param(pidtm_block, "k_d")) != -1) {
			pr_info("Parse from ECT k_d: %d\n", value);
			params->k_d = int_to_frac(value);
		}

		/* integral_max */
		if ((value = exynos_tmu_ect_get_param(pidtm_block, "i_max")) != -1) {
			pr_info("Parse from ECT i_max: %d\n", value);
			params->i_max = value;
		} else
			pr_err("Fail to parse i_max parameter\n");

		if ((value = exynos_tmu_ect_get_param(pidtm_block, "integral_cutoff")) != -1) {
			pr_info("Parse from ECT integral_cutoff: %d\n", value);
			params->integral_cutoff = value;
		} else
			pr_err("Fail to parse integral_cutoff parameter\n");

		if ((value = exynos_tmu_ect_get_param(pidtm_block, "p_control_t")) != -1) {
			pr_info("Parse from ECT p_control_t: %d\n", value);
			params->sustainable_power = value;
		} else
			pr_err("Fail to parse p_control_t parameter\n");
#endif
		if ((value = exynos_tmu_ect_get_param(pidtm_block, "hotplug_out_threshold")) != -1) {
			pr_info("Parse from ECT hotplug_out_threshold: %d\n", value);
			hotplug_out_threshold = value;
		}

		if ((value = exynos_tmu_ect_get_param(pidtm_block, "hotplug_in_threshold")) != -1) {
			pr_info("Parse from ECT hotplug_in_threshold: %d\n", value);
			hotplug_in_threshold = value;
		}

		if (hotplug_out_threshold != 0 && hotplug_in_threshold != 0) {
			data->hotplug_out_threshold = hotplug_out_threshold;
			data->hotplug_in_threshold = hotplug_in_threshold;
			data->hotplug_enable = true;
		} else {
			data->hotplug_enable = false;
		}
	}
	return 0;
};
#endif

#if IS_ENABLED(CONFIG_EXYNOS_ESCA_THERMAL)
static void exynos_tmu_set_tmu_data(struct exynos_tmu_data *data)
{
	struct thermal_zone_device *tz = data->tzd;
	int i, temp, hyst, ntrips;
	enum thermal_trip_type type;
	struct thermal_trip _trip;

	ntrips = tz->num_trips;
	exynos_acpm_tmu_set_tmu_data(data->id, TMU_DATA_TRIP_COUNT, 0, ntrips);

	for (i = 0; i < ntrips; i++) {
		thermal_zone_get_trip(tz, i, &_trip);
		type = _trip.type;
		temp = _trip.temperature;
		hyst = _trip.hysteresis;
		exynos_acpm_tmu_set_tmu_data(data->id,
				TMU_DATA_TRIP_HYST,
				i, hyst / MCELSIUS);
		exynos_acpm_tmu_set_tmu_data(data->id, TMU_DATA_TRIP_TEMP,
				i, temp / MCELSIUS);
		exynos_acpm_tmu_set_tmu_data(data->id, TMU_DATA_TRIP_TYPE,
				i, type);
	}

	if (data->hotplug_enable) {
		exynos_acpm_tmu_set_tmu_data(data->id, TMU_DATA_HOTPLUG_ENABLE,
				0, data->hotplug_enable);
		exynos_acpm_tmu_set_tmu_data(data->id,
				TMU_DATA_HOTPLUG_OUT_THRESHOLD,
				0, data->hotplug_out_threshold);
		exynos_acpm_tmu_set_tmu_data(data->id,
				TMU_DATA_HOTPLUG_IN_THRESHOLD,
				0, data->hotplug_in_threshold);
	}

	exynos_acpm_tmu_set_tmu_data(data->id, TMU_DATA_INIT_DONE, 0, true);
}

static void exynos_tmu_set_pi_params(struct exynos_tmu_data *data)
{
	struct exynos_pi_param *params = data->pi_param;

	if (data->use_pi_thermal) {
		exynos_acpm_tmu_set_tmu_data(data->id, TMU_DATA_USE_PI_THERMAL,
				0, data->use_pi_thermal);
		if (data->use_sync_pi_thermal) {
			exynos_acpm_tmu_set_tmu_data(data->id,
					TMU_DATA_USE_SYNC_PI_THERMAL,
					0, data->use_sync_pi_thermal);
		}
		exynos_acpm_tmu_set_tmu_data(data->id,
				TMU_DATA_TRIP_CONTROL_TEMP,
				0, params->trip_control_temp);
#if IS_ENABLED(CONFIG_EXYNOS_EA_DTM)
#else
		exynos_acpm_tmu_set_tmu_data(data->id,
				TMU_DATA_SUSTAINABLE_POWER,
				0, params->sustainable_power);
		exynos_acpm_tmu_set_tmu_data(data->id, TMU_DATA_K_PO,
				0, params->k_po);
		exynos_acpm_tmu_set_tmu_data(data->id, TMU_DATA_K_PU,
				0, params->k_pu);
		exynos_acpm_tmu_set_tmu_data(data->id, TMU_DATA_K_I,
				0, params->k_i);
		if (params->k_d) {
			exynos_acpm_tmu_set_tmu_data(data->id, TMU_DATA_K_D,
					0, params->k_d);
		}
		exynos_acpm_tmu_set_tmu_data(data->id, TMU_DATA_I_MAX,
				0, params->i_max);
		exynos_acpm_tmu_set_tmu_data(data->id, TMU_DATA_INTEGRAL_CUTOFF,
				0, params->integral_cutoff);
#endif
	}
}

static void exynos_tmu_register_hotplug_request(struct exynos_tmu_data *data)
{
	snprintf(data->cpuhp_name, THERMAL_NAME_LENGTH, "DTM_%s", data->tmu_name);
#if IS_ENABLED(CONFIG_SCHED_EMS)
	if (data->hotplug_completely_off)
		exynos_cpuhp_add_request(data->cpuhp_name, cpu_possible_mask);
	else
		ecs_request_register(data->cpuhp_name, cpu_possible_mask, ECS_MAX);
#endif

}
#endif

#if defined(CONFIG_MALI_DEBUG_KERNEL_SYSFS)
struct exynos_tmu_data *gpu_thermal_data;
#endif

#if IS_ENABLED(CONFIG_SOC_S5E9955)
static void * __iomem hint0;
static void * __iomem hint1;
static void * __iomem hint2;
static u32 hint0_val;
static u32 hint1_val;
static u32 hint2_val;
static u32 invalid_sample;
u32 get_invalid_sample(void)
{
	return invalid_sample;
}
EXPORT_SYMBOL(get_invalid_sample);
#endif

static int exynos_thermal_create_debugfs(void);
static int exynos_tmu_probe(struct platform_device *pdev)
{
	struct exynos_tmu_data *data;
	int ret;
	struct kobject *kobj;
#if IS_ENABLED(CONFIG_EXYNOS_ESCA_THERMAL)
	struct device_node *np;
#else
	struct task_struct *thread;
	struct sched_param param = { .sched_priority = MAX_RT_PRIO / 4 - 1 };
	unsigned int irq_flags = IRQF_SHARED;
#endif

#if IS_ENABLED(CONFIG_SOC_S5E9955)
	hint0 = ioremap(0x10000000+0xD000, 0x3);
	hint1 = ioremap(0x10000000+0xD088, 0x3);
	hint2 = ioremap(0x10000000+0xD08C, 0x3);

	if (hint0 && hint1 && hint2) {
		hint0_val = __raw_readl(hint0);
		hint1_val = __raw_readl(hint1);
		hint2_val = __raw_readl(hint2);
		iounmap(hint0);
		iounmap(hint1);
		iounmap(hint2);

		hint1_val = hint1_val >> 8;

		invalid_sample |= !hint0_val;
		invalid_sample |= (hint1_val >> 8);
		invalid_sample |= hint2_val;

		if (invalid_sample) {
			pr_info("thermal validation : 0x%x, 0x%x, 0x%x\n",
				hint0_val, hint1_val, hint2_val);
			return 0;
		}
	}
#endif

	data = devm_kzalloc(&pdev->dev, sizeof(struct exynos_tmu_data),
					GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	platform_set_drvdata(pdev, data);
	mutex_init(&data->lock);

	ret = exynos_map_dt_data(pdev);
	if (ret)
		goto err_sensor;

	if (list_empty(&dtm_dev_list)) {
#if IS_ENABLED(CONFIG_EXYNOS_ESCA_THERMAL)
		np = of_find_node_by_name(NULL, "acpm_noti_tmu");
		if (np) {
			int size, ch_num;
			ret = esca_ipc_request_channel(np,
					exynos_tmu_callback_func,
					&ch_num, &size);
			if (ret) {
				dev_err(&pdev->dev, "Failed to initialize esca ipc\n");
				goto err_sensor;
			}
			pr_info("[esca_acpm_tmu] found node, ch %d\n", ch_num);
		}
#endif
		exynos_acpm_tmu_init(NULL);
		exynos_acpm_tmu_set_init(&cap);
	}

	data->tzd = devm_thermal_of_zone_register(&pdev->dev, 0, data,
						    data->hotplug_enable ?
						    &exynos_hotplug_sensor_ops :
						    &exynos_sensor_ops);
	if (IS_ERR(data->tzd)) {
		ret = PTR_ERR(data->tzd);
		dev_err(&pdev->dev, "Failed to register sensor: %d\n", ret);
		goto err_sensor;
	}

	data->external_polling = true;

#if IS_ENABLED(CONFIG_EXYNOS_ESCA_THERMAL)
	exynos_acpm_tmu_set_tmu_mode(data->id, THERMAL_DEVICE_DISABLED);
#endif
	thermal_zone_device_disable(data->tzd);

#if IS_ENABLED(CONFIG_ECT)
	exynos_tmu_parse_ect(data);
#endif

#if IS_ENABLED(CONFIG_EXYNOS_ESCA_THERMAL)
	exynos_tmu_set_tmu_data(data);
#endif

	ret = exynos_tmu_initialize(pdev);
	if (ret) {
		dev_err(&pdev->dev, "Failed to initialize TMU\n");
		goto err_thermal;
	}

#if IS_ENABLED(CONFIG_EXYNOS_ESCA_THERMAL)
	if (data->hotplug_enable)
		exynos_tmu_register_hotplug_request(data);
#else
	ret = exynos_tmu_irq_work_init(pdev);
	if (ret) {
		dev_err(&pdev->dev, "cannot exynow tmu interrupt work initialize\n");
		goto err_thermal;
	}
#endif

#if IS_ENABLED(CONFIG_EXYNOS_ESCA_THERMAL)
#else
#ifdef MULTI_IRQ_SUPPORT_ITMON
	irq_flags |= IRQF_GIC_MULTI_TARGET;
#endif
#endif

	if (data->use_pi_thermal) {
#if IS_ENABLED(CONFIG_EXYNOS_ESCA_THERMAL)
		exynos_acpm_tmu_init_pi_work(data->id);
#else
		kthread_init_delayed_work(&data->pi_work, exynos_pi_polling);
#endif
	}
#if IS_ENABLED(CONFIG_EXYNOS_ESCA_THERMAL)
#else
	ret = devm_request_irq(&pdev->dev, data->irq, exynos_tmu_irq,
			irq_flags, dev_name(&pdev->dev), data);
	if (ret) {
		dev_err(&pdev->dev, "Failed to request irq: %d\n", data->irq);
		goto err_thermal;
	}
#endif
	ret = sysfs_create_group(&pdev->dev.kobj, &exynos_tmu_attr_group);
	if (ret)
		dev_err(&pdev->dev, "cannot create exynos tmu attr group");

	mutex_lock(&data->lock);
	list_add_tail(&data->node, &dtm_dev_list);
	num_of_devices++;
	mutex_unlock(&data->lock);

	if (list_is_singular(&dtm_dev_list)) {
		u32 addr, size;
#if IS_ENABLED(CONFIG_EXYNOS_ESCA_THERMAL)
		ret = exynos_tmu_esca_callback_work_init(pdev);
		if (ret)
			return ret;

		ret = exynos_tmu_polling_work_init(pdev);
		if (ret)
			return ret;

		exynos_acpm_tmu_get_addr(TMU_IPC_GET_DSS_DUMP_ADDR,
						  &addr, &size);
		tmu_dump_phy = addr;
		tmu_dump_base = exynos_tmu_remap(addr, size);

#if IS_ENABLED(CONFIG_SOC_S5E9945)
#else
		exynos_acpm_tmu_get_addr(TMU_IPC_GET_CUR_STATE_ADDR,
						  &addr, &size);
		extern_cur_state_phy = addr;
		extern_cur_state_base = exynos_tmu_remap(addr, size);
		extern_cur_state_size = size;
#endif

		log_num = dss_get_len_thermal_log();
		log_num = log_num < 20 ? 20 : log_num;
		if (log_num)
			tmu_dbginfo = vmalloc(sizeof(struct tmu_dbginfo)
					      * log_num);
#endif
		exynos_thermal_create_debugfs();

		if (data->hotplug_enable) {
#if IS_ENABLED(CONFIG_EXYNOS_ESCA_THERMAL)
#else
			kthread_init_worker(&hotplug_worker);
			thread = kthread_create(kthread_worker_fn, &hotplug_worker,
					"thermal_hotplug_kworker");
			kthread_bind(thread, 0);
			sched_setscheduler_nocheck(thread, SCHED_FIFO, &param);
			wake_up_process(thread);
#endif
		}

		kobj = kobject_create_and_add("exynos-thermal", kernel_kobj);
#if IS_ENABLED(CONFIG_DEBUG_SNAPSHOT)
		if (sysfs_create_bin_file(kobj, &thermal_log_bin_attr))
			dev_err(&pdev->dev, "Failed to create bin file\n");
#endif
		if (sysfs_create_file(kobj, &thermal_status_attr.attr))
			dev_err(&pdev->dev, "Failed to create thermal_status file\n");
		if (sysfs_create_file(kobj, &use_cold_attr.attr))
			dev_err(&pdev->dev, "Failed to create thermal_status file\n");
#if IS_ENABLED(CONFIG_SEC_PM)
		if (sysfs_create_file(kobj, &time_in_state_json_attr.attr))
			dev_err(&pdev->dev, "Failed to create time_in_state_json file\n");
#endif /* CONFIG_SEC_PM */
	}

	if (data->use_pi_thermal) {
		reset_pi_trips(data);
#if IS_ENABLED(CONFIG_EXYNOS_ESCA_THERMAL)
		exynos_tmu_set_pi_params(data);
#else
		reset_pi_params(data);
#endif
	}
#if IS_ENABLED(CONFIG_EXYNOS_ESCA_THERMAL)
	if (data->polling_delay_off) {
		exynos_acpm_tmu_set_tmu_data(data->id,
					TMU_DATA_POLLING_DELAY_OFF,
					0, data->polling_delay_off);
	}

	if (data->polling_delay_on) {
		exynos_acpm_tmu_set_tmu_data(data->id, TMU_DATA_TRIP_SWITCH_ON,
				0, 1);
		exynos_acpm_tmu_set_tmu_data(data->id,
				TMU_DATA_POLLING_DELAY_ON,
				0, data->polling_delay_on);
	}

	if (data->polling_delay_off || data->polling_delay_on) {
		start_pi_polling(data, 0);
	}
#endif

	if (!IS_ERR(data->tzd)) {
#if IS_ENABLED(CONFIG_EXYNOS_ESCA_THERMAL)
		exynos_acpm_tmu_set_tmu_mode(data->id, THERMAL_DEVICE_ENABLED);
#endif
		thermal_zone_device_enable(data->tzd);
	}

	data->nb.notifier_call = exynos_tmu_pm_notify;
	register_pm_notifier(&data->nb);

#if defined(CONFIG_MALI_DEBUG_KERNEL_SYSFS)
	if (data->id == EXYNOS_GPU_THERMAL_ZONE_ID)
		gpu_thermal_data = data;
#endif

#if IS_ENABLED(CONFIG_ISP_THERMAL)
	if (!strncmp(data->tmu_name, "ISP", 3))
		exynos_isp_cooling_init();
#endif

	exynos_tmu_control(pdev, true);

	device_enable_async_suspend(&pdev->dev);

	return 0;

err_thermal:
	devm_thermal_of_zone_unregister(&pdev->dev, data->tzd);
err_sensor:
	return ret;
}

static int exynos_tmu_suspend(struct device *dev)
{
	struct platform_device *pdev;
	struct exynos_tmu_data *data;

#if IS_ENABLED(CONFIG_SOC_S5E9955)
	if (invalid_sample)
		return 0;
#endif

	pdev = to_platform_device(dev);
	data = platform_get_drvdata(pdev);

	if (IS_ENABLED(CONFIG_EXYNOS_ESCA_THERMAL)) {
		exynos_acpm_tmu_set_tmu_mode(data->id, THERMAL_DEVICE_DISABLED);
	} else {
		disable_irq(data->irq);
		kthread_flush_work(&data->irq_work);

		if (data->hotplug_enable)
			kthread_flush_work(&data->hotplug_work);
	}

	cp_call_mode = is_aud_on() && cap.acpm_irq;
	if (!cp_call_mode) {
		exynos_tmu_control(pdev, false);
	}

	if (atomic_inc_return(&suspended_count) != num_of_devices)
		goto out;

	/* Last device only */

	if (IS_ENABLED(CONFIG_EXYNOS_ESCA_THERMAL))
		kthread_flush_work(&esca_tmu_cb.work);

	if (cp_call_mode) {
		exynos_acpm_tmu_set_cp_call();
		pr_info("%s: TMU suspend w/ AUD-on\n", __func__);
	} else {
		exynos_acpm_tmu_set_suspend(false);
		pr_info("%s: TMU suspend w/ AUD-off\n", __func__);
	}

out:
	dev->power.must_resume = true;

	return 0;
}

static int exynos_tmu_resume(struct device *dev)
{
	struct platform_device *pdev;
	struct exynos_tmu_data *data;
	int temp, stat;

#if IS_ENABLED(CONFIG_SOC_S5E9955)
	if (invalid_sample)
		return 0;
#endif

	pdev = to_platform_device(dev);
	data = platform_get_drvdata(pdev);

	if (IS_ENABLED(CONFIG_EXYNOS_ESCA_THERMAL)) {
		exynos_acpm_tmu_set_tmu_mode(data->id, THERMAL_DEVICE_ENABLED);
	}

	if (atomic_fetch_dec(&suspended_count) == num_of_devices) {
		/* First device only */
		exynos_acpm_tmu_set_resume();
	}

	if (!cp_call_mode) {
		exynos_tmu_control(pdev, true);
	}

	exynos_acpm_tmu_set_read_temp(data->id, &temp, &stat, NULL);

	pr_info("%s: thermal zone %d temp %d stat %d\n",
			__func__, data->tzd->id, temp, stat);

	if (!IS_ENABLED(CONFIG_EXYNOS_ESCA_THERMAL)) {
		struct cpumask mask;
		cpulist_parse("0-3", &mask);
		cpumask_and(&mask, cpu_possible_mask, &mask);
		set_cpus_allowed_ptr(data->thermal_worker.task, &mask);

		enable_irq(data->irq);
	}

	return 0;
}

static const struct dev_pm_ops exynos_tmu_pm_ops = {
	.suspend_late = exynos_tmu_suspend,
	.resume_early = exynos_tmu_resume,
};

static struct platform_driver exynos_tmu_driver = {
	.driver = {
		.name   = "exynos-tmu",
		.pm     =  &exynos_tmu_pm_ops,
		.of_match_table = exynos_tmu_match,
		.suppress_bind_attrs = true,
	},
	.probe = exynos_tmu_probe,
};

module_platform_driver(exynos_tmu_driver);

static void exynos_acpm_tmu_test_cp_call(bool mode)
{
	struct exynos_tmu_data *devnode;

	if (mode) {
		list_for_each_entry(devnode, &dtm_dev_list, node) {
			disable_irq(devnode->irq);
		}
		exynos_acpm_tmu_set_cp_call();
	} else {
		exynos_acpm_tmu_set_resume();
		list_for_each_entry(devnode, &dtm_dev_list, node) {
			enable_irq(devnode->irq);
		}
	}
}

VISIBLE_IF_KUNIT int emul_call_get(void *data, unsigned long long *val)
{
	*val = exynos_acpm_tmu_is_test_mode();

	return 0;
}
EXPORT_SYMBOL_IF_KUNIT(emul_call_get);

static int emul_call_set(void *data, unsigned long long val)
{
	int status = exynos_acpm_tmu_is_test_mode();

	if ((val == 0 || val == 1) && (val != status)) {
		exynos_acpm_tmu_set_test_mode(val);
		exynos_acpm_tmu_test_cp_call(val);
	}

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(emul_call_fops, emul_call_get, emul_call_set, "%llu\n");

static int log_print_set(void *data, unsigned long long val)
{
	if (val == 0 || val == 1)
		exynos_acpm_tmu_log(val);

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(log_print_fops, NULL, log_print_set, "%llu\n");

#if IS_ENABLED(CONFIG_EXYNOS_ESCA_THERMAL)
static int pi_trace_get(void *data, unsigned long long *val)
{
	exynos_acpm_tmu_get_pi_trace_mode(val);
	return 0;
}

static int pi_trace_set(void *data, unsigned long long val)
{
	if (val != 0 && val != 1)
		return 0;

	exynos_acpm_tmu_set_pi_trace_mode(val);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(pi_trace_fops, pi_trace_get, pi_trace_set, "%llu\n");

static u64 dfs_base_tick;
static ktime_t dfs_base_ktime;

static int exynos_thermal_dfs_show(struct seq_file *buf, void *d)
{
	struct exynos_tmu_data *data = NULL;
	ktime_t dfs_ktime;
	u64 dfs_tick = 0;

	exynos_acpm_tmu_get_dfs_time(&dfs_ktime, &dfs_tick);

	seq_printf(buf, "Elapsed time: %llu [ms]\n",
		   (dfs_ktime - dfs_base_ktime) / NSEC_PER_MSEC);
	seq_printf(buf, "DFS duration: %llu [ms]\n",
#if IS_ENABLED(CONFIG_EXYNOS_ESCA)
		(esca_time_calc(dfs_base_tick, dfs_tick)) / NSEC_PER_MSEC);
#else
		(acpm_time_calc(dfs_base_tick, dfs_tick)) / NSEC_PER_MSEC);
#endif

	list_for_each_entry(data, &dtm_dev_list, node) {
		int temp;

		if (!data->tzd)
			continue;

		exynos_get_temp(data->tzd, &temp);
		mutex_lock(&data->lock);
		seq_printf(buf, "NAME: %-9s trigger: %d\n",
			   data->tmu_name, data->dfs_triggered);
		mutex_unlock(&data->lock);
	}

	return 0;
}

static int exynos_thermal_dfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, exynos_thermal_dfs_show, inode->i_private);
}

static ssize_t exynos_thermal_dfs_write(struct file *file, const char __user *user_buf,
					size_t count, loff_t *ppos)
{
	struct exynos_tmu_data *data = NULL;

	exynos_acpm_tmu_get_dfs_time(&dfs_base_ktime, &dfs_base_tick);

	list_for_each_entry(data, &dtm_dev_list, node) {
		int temp;

		if (!data->tzd)
			continue;

		data->external_polling = false;
		exynos_get_temp(data->tzd, &temp);
		mutex_lock(&data->lock);
		data->dfs_triggered = 0;
		mutex_unlock(&data->lock);
	}

	return count;
}

static const struct file_operations debug_dfs_fops = {
	.open		= exynos_thermal_dfs_open,
	.read		= seq_read,
	.write		= exynos_thermal_dfs_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};
#endif


static struct dentry *debugfs_root;

static int exynos_thermal_create_debugfs(void)
{
	debugfs_root = debugfs_create_dir("exynos-thermal", NULL);
	if (!debugfs_root) {
		pr_err("Failed to create exynos thermal debugfs\n");
		return 0;
	}

	debugfs_create_file("emul_call", 0644, debugfs_root, NULL, &emul_call_fops);
	debugfs_create_file("log_print", 0644, debugfs_root, NULL, &log_print_fops);
#if IS_ENABLED(CONFIG_EXYNOS_ESCA_THERMAL)
	debugfs_create_file("pi_trace", 0644, debugfs_root, NULL, &pi_trace_fops);
	debugfs_create_file("dfs", 0640, debugfs_root, NULL, &debug_dfs_fops);
#endif
	return 0;
}

MODULE_DESCRIPTION("EXYNOS TMU Driver");
MODULE_LICENSE("GPL");

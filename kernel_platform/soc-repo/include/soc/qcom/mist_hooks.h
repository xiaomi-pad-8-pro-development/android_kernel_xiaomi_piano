/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Hooks the Xiaomi mist module (vendor/xiaomi/perf/mist) exports to the
 * Qualcomm memlat, bwmon, WALT and Adreno drivers. Prototypes follow the
 * piano stock modules (HyperOS OS4.0.6) and the dada-closed-modules mist
 * reconstruction.
 */
#ifndef _SOC_QCOM_MIST_HOOKS_H
#define _SOC_QCOM_MIST_HOOKS_H

#include <linux/types.h>
#include <linux/cpumask.h>
#include <linux/hrtimer.h>

struct dcvs_freq;
struct qcom_pmu_data;

typedef void (*mi_dflt_bw_update_cb)(void);
typedef void (*mi_dflt_lat_update_cb)(int cpu, ktime_t ts, struct qcom_pmu_data *data);
typedef void (*mi_dflt_lat_cal_cb)(void);

int mist_ddr_vote_enable(void);
int mi_dflt_get_latency_enable(void);
void mi_dflt_set_latency_enable(int enable);
int mi_dflt_get_latency_enable_from_user(void);
bool mi_dflt_get_latency_vote_start(void);
void mi_dflt_set_latency_vote_start(bool start);
int mi_dflt_get_bwmon_enable(void);
bool mi_dflt_get_bwmon_sample_start(int hw_type);
void mi_dflt_set_bwmon_sample_start(int hw_type, bool start);

void mi_dflt_process_raw_ctrs_hook(int cpu, u64 cycle_data, u64 stall_data);
void mi_dflt_calculate_dflt_lantancy_hook(int hw_type, int cpu, s64 delta_us,
					  u64 instructions, u64 miss, u64 cycles);
void mi_dflt_calculate_mon_sampling_freq_hook(cpumask_t cpus, int hw_type);
void mi_dflt_update_memlat_fp_vote_hook(struct dcvs_freq *voted_freqs, int grp);
void mi_dflt_bwmon_update_cur_freq_hook(u32 new_ib, int hw_type);
void mi_dflt_schedule_hook(void);
void set_adreno_gflt_load(u64 total, u64 busy, u64 ram_time, u64 ram_wait);
u64 gpu_load_calibration(void);
struct devfreq;
void mist_gflt_start(struct devfreq *devf);
void mist_gflt_stop(struct devfreq *devf);
bool mi_gpu_flt_working(bool log, const char *func);
struct workqueue_struct;
void trigger_mist_load_tracking(struct workqueue_struct *wq);

void register_mi_dflt_bw_update_cb(mi_dflt_bw_update_cb cb);
void unregister_mi_dflt_bw_update_cb(void);
void register_mi_dflt_lat_update_cb(mi_dflt_lat_update_cb cb, struct hrtimer *timer,
				    mi_dflt_lat_cal_cb cal_f);
void unregister_mi_dflt_lat_update_cb(void);

/* mist SOC_FREQ_MOD index of the GPU (stock passes 1) */
#define MI_SOC_GPU_DEV 1
typedef int (*MI_SOC_FREQ_ADJHOOK)(struct devfreq *devf, unsigned int interval_ms);
void mi_soc_register_freq_adjhook(char *name, int mod, MI_SOC_FREQ_ADJHOOK hook);

#endif

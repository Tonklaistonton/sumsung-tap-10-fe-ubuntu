#ifndef __ESCA_PLG_H__
#define __ESCA_PLG_H__

#if IS_ENABLED(CONFIG_EXYNOS_ESCA)
extern void __iomem *fvmap_base_address;
extern void *get_fvmap_base(void);
extern u32 esca_enable_flexpmu_profiler(u32 start);
extern void esca_init_eint_clk_req(u32 eint_num);
extern void esca_init_eint_nfc_clk_req(u32 eint_num);
extern void esca_init_eint_uwb_clk_req(u32 eint_num);
extern void get_plugin_dbg_addr(u32 esca_layer, u32 pid, u64 *sram_base, u64 *dbg_addr);
#else
static inline void *get_fvmap_base(void)
{
	return (void *)0;
}
static inline u32 esca_enable_flexpmu_profiler(u32 start)
{
	return 0;
}
static inline void esca_init_eint_clk_req(u32 eint_num) {}
static inline void esca_init_eint_nfc_clk_req(u32 eint_num) {}
static inline void  esca_init_eint_uwb_clk_req(u32 eint_num) {}
static inline void get_plugin_dbg_addr(u32 esca_layer, u32 pid, u64 *sram_base, u64 *dbg_addr) {}
#endif

#define EXYNOS_PMU_SPARE7						(0x3bc)
#define EXYNOS_SPARE_CTRL__DATA7					(0x3c30)

#endif		/* __ESCA_PLG_H__ */

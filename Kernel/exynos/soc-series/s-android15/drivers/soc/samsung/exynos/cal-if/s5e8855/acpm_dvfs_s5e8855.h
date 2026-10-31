#include <soc/samsung/fvmap.h>
#include <soc/samsung/asv_g_spec.h>

enum acpm_dvfs_id {
	MIF = ACPM_VCLK_TYPE,
	INT,
	CPUCL0,
	CPUCL1,
	CPUCL2,
	DSU,
	NPU,
	DNC,
	AUD,
	CP_CPU,
	CP,
	GNSS,
	CP_MCW,
	G3D,
	DISP,
	MFC,
	INTCAM,
	ICPU,
	CAM,
	ISP,
	CSIS,
	WLBT,
	INTSCI,
};

struct vclk acpm_vclk_list[] = {
	CMUCAL_ACPM_VCLK(MIF, NULL, NULL, NULL, NULL, 0),
	CMUCAL_ACPM_VCLK(INT, NULL, NULL, NULL, NULL, 0),
	CMUCAL_ACPM_VCLK(CPUCL0, NULL, NULL, NULL, NULL, 1),
	CMUCAL_ACPM_VCLK(CPUCL1, NULL, NULL, NULL, NULL, 1),
	CMUCAL_ACPM_VCLK(CPUCL2, NULL, NULL, NULL, NULL, 1),
	CMUCAL_ACPM_VCLK(DSU, NULL, NULL, NULL, NULL, 1),
	CMUCAL_ACPM_VCLK(NPU, NULL, NULL, NULL, NULL, 0),
	CMUCAL_ACPM_VCLK(DNC, NULL, NULL, NULL, NULL, 0),
	CMUCAL_ACPM_VCLK(AUD, NULL, NULL, NULL, NULL, 0),
	CMUCAL_ACPM_VCLK(CP_CPU, NULL, NULL, NULL, NULL, 0),
	CMUCAL_ACPM_VCLK(CP, NULL, NULL, NULL, NULL, 0),
	CMUCAL_ACPM_VCLK(GNSS, NULL, NULL, NULL, NULL, 0),
	CMUCAL_ACPM_VCLK(CP_MCW, NULL, NULL, NULL, NULL, 0),
	CMUCAL_ACPM_VCLK(G3D, NULL, NULL, NULL, NULL, 1),
	CMUCAL_ACPM_VCLK(DISP, NULL, NULL, NULL, NULL, 0),
	CMUCAL_ACPM_VCLK(MFC, NULL, NULL, NULL, NULL, 0),
	CMUCAL_ACPM_VCLK(INTCAM, NULL, NULL, NULL, NULL, 0),
	CMUCAL_ACPM_VCLK(ICPU, NULL, NULL, NULL, NULL, 0),
	CMUCAL_ACPM_VCLK(CAM, NULL, NULL, NULL, NULL, 0),
	CMUCAL_ACPM_VCLK(ISP, NULL, NULL, NULL, NULL, 0),
	CMUCAL_ACPM_VCLK(CSIS, NULL, NULL, NULL, NULL, 0),
	CMUCAL_ACPM_VCLK(WLBT, NULL, NULL, NULL, NULL, 0),
	CMUCAL_ACPM_VCLK(INTSCI, NULL, NULL, NULL, NULL, 0),
};

unsigned int acpm_vclk_size = ARRAY_SIZE(acpm_vclk_list);
asv_g_spec(cpucl2, GET_IDX(CPUCL2), 0x10009010, 8);
asv_g_spec(cpucl1, GET_IDX(CPUCL1), 0x10009014, 8);
asv_g_spec(cpucl0, GET_IDX(CPUCL0), 0x10009018, 8);
asv_g_spec(g3d, GET_IDX(G3D), 0x1000901C, 8);
asv_g_spec(dsu, GET_IDX(DSU), 0x1000001C, 8);
asv_g_spec(modem, GET_IDX(CP), 0x1000003C, 8);

static struct attribute *asv_g_spec_attrs[] = {
	asv_g_spec_attr(cpucl2),
	asv_g_spec_attr(cpucl1),
	asv_g_spec_attr(cpucl0),
	asv_g_spec_attr(g3d),
	asv_g_spec_attr(dsu),
	asv_g_spec_attr(modem),
	NULL,
};

const struct attribute_group asv_g_spec_grp = {
	.attrs = asv_g_spec_attrs,
};

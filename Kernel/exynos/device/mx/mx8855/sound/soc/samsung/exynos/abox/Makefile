subdir-ccflags-y += -I${ROOT_DIR}/${EXYNOS_SOC_DIR}/sound/soc/samsung/exynos/abox

# for AW AMP
snd-soc-samsung-abox-sync-aw-$(CONFIG_SEC_SND_SYNCHRONIZED_IPC_AW) += abox_synchronized_ipc_aw.o
obj-$(CONFIG_SEC_SND_SYNCHRONIZED_IPC_AW) += snd-soc-samsung-abox-sync-aw.o

# for TI AMP
snd-soc-samsung-abox-sync-ti-$(CONFIG_SEC_SND_SYNCHRONIZED_IPC_TI) += abox_synchronized_ipc_ti.o
obj-$(CONFIG_SEC_SND_SYNCHRONIZED_IPC_TI) += snd-soc-samsung-abox-sync-ti.o
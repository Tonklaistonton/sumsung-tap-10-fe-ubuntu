# SPDX-License-Identifier: GPL-2.0
#
# Makefile for the Linux kernel panel device drivers.
#

usdm-panel-tft-common-y := \
	tft_function.o \
	tft_common.o

obj-$(CONFIG_USDM_PANEL_TFT_COMMON) += usdm-panel-tft-common.o

subdir-ccflags-$(CONFIG_SEC_KUNIT) += \
	-I$(srctree)/$(src)/.. \
	-Wframe-larger-than=8192 \
	-Wunused-variable \
	-Wunused-function \
	-Wno-missing-braces \
	-Wno-implicit-fallthrough \
	-Wno-format

obj-$(CONFIG_SEC_KUNIT) += kunit_test/

mcd-panel-hx83102j_gts10fe_boe-y += hx83102j_gts10fe_boe.o
obj-$(CONFIG_USDM_PANEL_HX83102J_GTS10FE_BOE) += mcd-panel-hx83102j_gts10fe_boe.o

mcd-panel-nt36523n_gts10fe_csot-y += nt36523n_gts10fe_csot.o
obj-$(CONFIG_USDM_PANEL_NT36523N_GTS10FE_CSOT) += mcd-panel-nt36523n_gts10fe_csot.o

# SPDX-License-Identifier: GPL-2.0
#
# Makefile for the kernel block layer
#

ssg-$(CONFIG_MQ_IOSCHED_SSG)   := ssg-iosched.o ssg-stat.o
ssg-$(CONFIG_MQ_IOSCHED_SSG_CGROUP)    += ssg-cgroup.o
ssg-$(CONFIG_MQ_IOSCHED_SSG_WB) += ssg-wb.o
obj-$(CONFIG_MQ_IOSCHED_SSG)   += ssg.o

obj-$(CONFIG_BLK_SEC_COMMON)   += blk-sec-common.o
blk-sec-stats-$(CONFIG_BLK_SEC_STATS)  := blk-sec-stat.o blk-sec-stat-pio.o blk-sec-stat-traffic.o
obj-$(CONFIG_BLK_SEC_STATS)    += blk-sec-stats.o
obj-$(CONFIG_BLK_SEC_WB)   += blk-sec-wb.o

subdir-ccflags-y += -I${ROOT_DIR}/${KERNEL_DIR}/drivers/ufs/core \
                    -I${ROOT_DIR}/${EXYNOS_SOC_DIR}/drivers/ufs/host

subdir-ccflags-y += -I${ROOT_DIR}/${KERNEL_DIR}/block
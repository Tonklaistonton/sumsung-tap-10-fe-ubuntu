# SPDX-License-Identifier: GPL-2.0

include $(KERNEL_SRC)/../${EXYNOS_SOC_DIR}/Makefile.include

modules modules_install:
	$(MAKE) M=$(M) -C $(KERNEL_SRC) \
	KBUILD_EXTRA_SYMBOLS="$(EXTRA_SYMBOLS)" $(@)

clean:
	$(MAKE) -C $(KERNEL_SRC) M=$(M) clean

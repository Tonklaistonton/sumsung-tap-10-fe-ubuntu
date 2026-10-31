# SPDX-License-Identifier: GPL-2.0

EXTRA_SYMBOLS += $(O)/../exynos/external-modules/sec_input/sec_input/Module.symvers

modules modules_install:
	$(MAKE) M=$(M) -C $(KERNEL_SRC) \
	KBUILD_EXTRA_SYMBOLS="$(EXTRA_SYMBOLS)" $(@)

clean:
	$(MAKE) -C $(KERNEL_SRC) M=$(M) clean

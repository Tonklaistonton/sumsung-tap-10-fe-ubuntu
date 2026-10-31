# SPDX-License-Identifier: GPL-2.0

all:
	$(MAKE) -C $(KERNEL_SRC) M=$(M) modules $(KBUILD_OPTIONS)

modules_install:
	$(MAKE) M=$(M) -C $(KERNEL_SRC) \
	KBUILD_EXTRA_SYMBOLS="$(EXTRA_SYMBOLS)" modules_install

clean:
	$(MAKE) -C $(KERNEL_SRC) M=$(M) clean

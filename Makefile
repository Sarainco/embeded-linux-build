CONFIG_FILE := .config
CONFIGS := $(shell cat $(CONFIG_FILE) 2>/dev/null)

HOST_KCONFIG := host/kconfig/mconf
CONFIG_IN := Config.in

SOURCES := sources
PATCHES := patches
OUTPUT := output

UBOOT_SRC := $(SOURCES)/uboot
KERNEL_SRC := $(SOURCES)/kernel
BUSYBOX_SRC := $(SOURCES)/busybox

UBOOT_PATCH := $(PATCHES)/uboot
KERNEL_PATCH := $(PATCHES)/kernel
BUSYBOX_PATCH := $(PATCHES)/busybox

.PHONY: all menuconfig build clean distclean

all: build

menuconfig: $(HOST_KCONFIG)
	KCONFIG_CONFIG=$(CONFIG_FILE) $(HOST_KCONFIG) $(CONFIG_IN)

$(HOST_KCONFIG):
	@echo "Building mconf (Kconfig menuconfig tool)..."
	$(MAKE) -C host/kconfig

build: build-uboot build-kernel build-busybox

build-uboot:
ifneq (,$(findstring CONFIG_BUILD_UBOOT=y,$(CONFIGS)))
	@echo ">> Building U-Boot..."
	cd $(UBOOT_SRC) && git reset --hard HEAD && git clean -fdx
	cd $(UBOOT_SRC) && git am $(abspath $(UBOOT_PATCH))/*.patch
	cd $(UBOOT_SRC) && make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- defconfig && \
	    make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -j$(nproc)
endif

build-kernel:
ifneq (,$(findstring CONFIG_BUILD_KERNEL=y,$(CONFIGS)))
	@echo ">> Building Kernel..."
	cd $(KERNEL_SRC) && git reset --hard HEAD && git clean -fdx
	cd $(KERNEL_SRC) && git am $(abspath $(KERNEL_PATCH))/*.patch
	cd $(KERNEL_SRC) && make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- defconfig && \
	    make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -j$(nproc)
endif

build-busybox:
ifneq (,$(findstring CONFIG_BUILD_BUSYBOX=y,$(CONFIGS)))
	@echo ">> Building BusyBox..."
	cd $(BUSYBOX_SRC) && git reset --hard HEAD && git clean -fdx
	cd $(BUSYBOX_SRC) && git am $(abspath $(BUSYBOX_PATCH))/*.patch
	cd $(BUSYBOX_SRC) && make defconfig && \
	    make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- install
endif

clean:
	@echo "Cleaning output..."
	rm -rf $(OUTPUT)

distclean: clean
	rm -f $(CONFIG_FILE)

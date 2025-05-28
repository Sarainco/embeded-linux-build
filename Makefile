CONFIG_FILE := .config
CONFIGS := $(shell cat $(CONFIG_FILE) 2>/dev/null)

HOST_KCONFIG := host/kconfig/mconf
CONFIG_IN := Config.in

# 编译参数从配置中提取
TOOLCHAIN_PATH := $(shell awk -F= '/CONFIG_TOOLCHAIN_PATH/ {gsub(/"/,""); print $$2}' $(CONFIG_FILE))
CROSS_COMPILE := $(shell awk -F= '/CONFIG_CROSS_COMPILE_PREFIX/ {gsub(/"/,""); print $$2}' $(CONFIG_FILE))
CROSS := $(TOOLCHAIN_PATH)/bin/$(CROSS_COMPILE)

BUSYBOX_SRC := sources/busybox
BUSYBOX_PATCH := patches/busybox
ROOTFS := output/rootfs

.PHONY: all menuconfig build-busybox clean distclean

all: build-busybox

menuconfig: $(HOST_KCONFIG)
	KCONFIG_CONFIG=$(CONFIG_FILE) $(HOST_KCONFIG) $(CONFIG_IN)

$(HOST_KCONFIG):
	@echo "Building mconf (Kconfig menuconfig tool)..."
	$(MAKE) -C host/kconfig

build-busybox:
ifneq (,$(findstring CONFIG_BUILD_BUSYBOX=y,$(CONFIGS)))
	@echo ">>> Building BusyBox..."
	cd $(BUSYBOX_SRC) && git init && git add . && git commit -m "add original source"
ifneq ("$(wildcard $(BUSYBOX_PATCH)/*.patch)","")
	cd $(BUSYBOX_SRC) && git am $(abspath $(BUSYBOX_PATCH))/*.patch
endif
	cd $(BUSYBOX_SRC) && make ARCH=arm64 CROSS_COMPILE=$(CROSS) defconfig
	cd $(BUSYBOX_SRC) && make ARCH=arm64 CROSS_COMPILE=$(CROSS) -j$(nproc)
	cd $(BUSYBOX_SRC) && make ARCH=arm64 CROSS_COMPILE=$(CROSS) CONFIG_PREFIX=$(abspath $(ROOTFS)) install
endif

EXT4_SIZE := 128
ROOTFS_DIR := output/rootfs
ROOTFS_IMG := output/rootfs.ext4

rootfs.ext4: $(ROOTFS_DIR)
	dd if=/dev/zero of=$(ROOTFS_IMG) bs=1M count=$(EXT4_SIZE)
	mkfs.ext4 -F $(ROOTFS_IMG)
	mkdir -p mnt_rootfs
	sudo mount -o loop $(ROOTFS_IMG) mnt_rootfs
	sudo cp -a $(ROOTFS_DIR)/* mnt_rootfs/
	sync
	sudo umount mnt_rootfs
	rmdir mnt_rootfs
	@echo "rootfs.ext4 generated at $(ROOTFS_IMG)"


clean:
	rm -rf output

distclean: clean
	rm -f $(CONFIG_FILE) .config.old

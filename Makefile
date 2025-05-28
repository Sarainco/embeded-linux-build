CONFIG_FILE := .config
CONFIGS := $(shell cat $(CONFIG_FILE) 2>/dev/null)

HOST_KCONFIG := host/kconfig/mconf
CONFIG_IN := Config.in

SOURCES := sources
PATCHES := patches
OUTPUT := output


.PHONY: all menuconfig clean distclean


menuconfig: $(HOST_KCONFIG)
	KCONFIG_CONFIG=$(CONFIG_FILE) $(HOST_KCONFIG) $(CONFIG_IN)

$(HOST_KCONFIG):
	@echo "Building mconf (Kconfig menuconfig tool)..."
	$(MAKE) -C host/kconfig


clean:
	@echo "Cleaning output..."
	rm -rf $(OUTPUT)

distclean: clean
	rm -f $(CONFIG_FILE)

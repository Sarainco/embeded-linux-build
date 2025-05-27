MAJOR_VERSION := 1
MINOR_VERSION := 0
DEV_VERSION := -rc1

TOP := $(CURDIR)

all: host toolchain kernel app target;



menuconfig: host/kconfig
	KCONFIG_CONFIG=$(MODEL_CONFIG) $(HOST_OUTPUT)/kconfig/mconf Config.in

clean:
	rm -fr $(HOST_OUTPUT) $(OUTPUT)

clean-all:
	rm -fr $(STAGING_DIR)

clean-dist:
	rm -fr .model .staging staging targets/*/model.config.old

status:
	$(H)cd $(SOURCE) && for dir in app/* linux-* ; do \
		echo $$dir; \
		(cd $$dir && git status --porcelain | sed 's/^/  /') ; done

.PHONY: all menuconfig clean clean-all clean-dist

.NOTPARALLEL:

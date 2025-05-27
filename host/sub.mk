
INSTALL_SRC := $(HOST)/scripts/install_source.sh
INSTALL := $(HOST_OUTPUT)/install/install
STRIP_INSTALL := STRIPPROG=$(STRIP) STRIPOPT=-s $(INSTALL) -D -s

host/kconfig:
	$(E)
	$(H)[ -d $(HOST_OUTPUT) ] || mkdir -p $(HOST_OUTPUT)
	+$(HQ)$(MAKE) -C $(HOST)/kconfig O=$(HOST_OUTPUT)/kconfig/

host/install:
	$(E)
	$(HQ)mkdir -p $(HOST_OUTPUT)/install/
	$(HQ)[ -f $(INSTALL) ] || gcc -o $(INSTALL) host/install/install.c host/install/mkdir.c

host: host/kconfig host/install

host/clean:
	rm -fr $(HOST_OUTPUT)

.PHONY: host/kconfig host/clean host/install


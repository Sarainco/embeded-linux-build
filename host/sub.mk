
host/kconfig:
	$(E)
	$(H)[ -d $(HOST_OUTPUT) ] || mkdir -p $(HOST_OUTPUT)
	+$(HQ)$(MAKE) -C $(HOST)/kconfig O=$(HOST_OUTPUT)/kconfig/


host: host/kconfig


.PHONY: host/kconfig host/clean


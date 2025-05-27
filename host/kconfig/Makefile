
cflags := -I. -Wall -Wmissing-prototypes -Wstrict-prototypes -O2 -fomit-frame-pointer -std=gnu89 \
			-D_DEFAULT_SOURCE -D_XOPEN_SOURCE=600 -DCURSES_LOC="<ncurses.h>" \
			-DNCURSES_WIDECHAR=1 -DLOCALE -pipe

ldflags := -pipe

mconf_ldflags := -lncursesw -ltinfo

all: $(O)lxdialog $(O)conf $(O)mconf;

$(O)lxdialog:
	mkdir -p $@
	
$(O)conf: $(O)conf.o $(O)zconf.tab.o 
	$(CC) -o $@ $^ $(ldflags)

$(O)mconf: $(O)mconf.o $(O)zconf.tab.o $(addprefix $(O)lxdialog/,checklist.o util.o inputbox.o textbox.o yesno.o menubox.o)
	$(CC) -o $@ $^ $(mconf_ldflags) $(ldflags)

$(O)%.o: %.c
	$(CC) $(cflags) -c -o $@ $<


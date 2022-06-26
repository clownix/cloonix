GLOB_INC=../../glob_include
IO_CLOWNIX=../../lib_io_clownix/include

SOURCES = main_cli.c simu_cloon.c simu_tx.c clownix_timer.c
CFLAGS += -I$(GLOB_INC) -I$(IO_CLOWNIX) -I../libmdl -I../libcli -g
LDFLAGS += -L../libcli -lcli -L../libmdl -lmdl -lutil -lpthread

OBJECTS := $(patsubst %.c,obj/%.o,$(SOURCES))
all: cloonix_xwy_cli

cloonix_xwy_cli: $(OBJECTS)
	$(CC) $^ -o $@ $(LDFLAGS)

obj:
	mkdir $@

obj/%.o: %.c obj
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	$(RM) $(OBJECTS) cloonix_xwy_cli
	if test -e obj; then rmdir obj; fi

.PHONY: all clean

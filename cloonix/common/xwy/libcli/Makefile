SOURCES = cli_lib.c getsock.c scp.c \
          x11_cli.c x11_init.c x11_tx.c \
          xdoors.c

OBJECTS := $(patsubst %.c,obj/%.o,$(SOURCES))
CFLAGS += -I../libmdl -I../../lib_io_clownix/include -I../../glob_include

RANLIB?=ranlib
AR?=ar

all: libcli.a

%.a: $(OBJECTS)
	$(AR) rc $@ $^
	$(RANLIB) $@

obj:
	mkdir $@

obj/%.o: %.c obj
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	$(RM) $(OBJECTS) libcli.a
	if test -e obj; then rmdir obj; fi

.PHONY: all clean

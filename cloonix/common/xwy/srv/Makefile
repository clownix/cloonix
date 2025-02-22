SOURCES = main_srv.c x11_fwd.c scp_srv.c tools.c cloon.c \
          pty_fork.c first_x11_magic.c hide_dirs.c

VPATH= .:../../../client/hide_dirs/

CFLAGS+=-mtune=generic -fno-stack-protector
CFLAGS += -I../libmdl -I../../glob_include -g -rdynamic
LDFLAGS += -g -rdynamic -L../libmdl -lmdl -lutil -lpthread

OBJECTS := $(patsubst %.c,obj/%.o,$(SOURCES))

all: cloonix-xwy-srv

cloonix-xwy-srv: $(OBJECTS)
	$(CC) $^ -o $@ $(LDFLAGS)

obj:
	mkdir $@

obj/%.o: %.c obj
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	$(RM) $(OBJECTS) cloonix-xwy-srv
	if test -e obj; then rmdir obj; fi

.PHONY: all clean

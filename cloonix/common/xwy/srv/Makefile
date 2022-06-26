SOURCES = main_srv.c x11_fwd.c scp_srv.c tools.c cloon.c \
          pty_fork.c first_x11_magic.c
CFLAGS += -I../libmdl -g 
LDFLAGS += -L../libmdl -lmdl -lutil -lpthread

OBJECTS := $(patsubst %.c,obj/%.o,$(SOURCES))

all: cloonix_xwy_srv

cloonix_xwy_srv: $(OBJECTS)
	$(CC) $^ -o $@ $(LDFLAGS)

obj:
	mkdir $@

obj/%.o: %.c obj
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	$(RM) $(OBJECTS) cloonix_xwy_srv
	if test -e obj; then rmdir obj; fi

.PHONY: all clean

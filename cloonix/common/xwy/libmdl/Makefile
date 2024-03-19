SOURCES = \
          mdl.c \
          low_write.c \
          debug.c \
          wrap.c \
          thread_tx.c \
          thread_x11.c \
          circle.c \
          fd_spy.c \
          thread_spy.c \
          cloonix_timer.c \
          dialog_thread.c

OBJECTS := $(patsubst %.c,obj/%.o,$(SOURCES))

CFLAGS+=-mtune=generic -fno-stack-protector

RANLIB?=ranlib
AR?=ar

all: libmdl.a

%.a: $(OBJECTS)
	$(AR) rc $@ $^
	$(RANLIB) $@

obj:
	mkdir $@

obj/%.o: %.c obj
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	$(RM) $(OBJECTS) libmdl.a
	if test -e obj; then rmdir obj; fi

.PHONY: all clean

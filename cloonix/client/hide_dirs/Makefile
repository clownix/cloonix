OBJ_DIR=./obj/
WARNINGS = -Wall  -Wno-format-truncation
EXE = cloonix-hide-dirs
SRC = client_hide.c hide_dirs.c pthexec.c
VPATH=../../common/glob_include
INCLUDES = -I../../common/glob_include 
CFLAGS = -g $(WARNINGS) $(INCLUDES)
all: $(shell mkdir -p ./obj) $(EXE)

OBJ = $(SRC:.c=.o)
OBJS = $(addprefix $(OBJ_DIR), $(OBJ))

$(OBJ_DIR)%.o: %.c
	$(CC) $(CFLAGS) -c $^ -o $@

$(EXE): $(OBJS)
	$(CC) $(LDFLAGS) $^ -o $(EXE) 

clean:
	@-rm -vrf $(OBJ_DIR) $(EXE)


OBJ_DIR=./obj/
SRV = sock_srv
SSRC = sock_srv.c \
       sock_utils.c 
LIBS = 
CFLAGS+= -g -Wall

all: $(shell mkdir -p ./obj) $(SRV)

SOBJ = $(SSRC:.c=.o)

SOBJS = $(addprefix $(OBJ_DIR), $(SOBJ))

$(OBJ_DIR)%.o: %.c
	$(CC) $(CFLAGS) -c $^ -o $@

$(SRV): $(SOBJS)
	$(CC) $(LDFLAGS) $^ $(LIBS) -o $(SRV) 

clean:
	@-rm -vrf $(OBJ_DIR) $(SRV)


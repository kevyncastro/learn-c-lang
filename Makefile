CC      = gcc
CFLAGS  = -std=c11 -D_GNU_SOURCE -g -O1 -Wall -Wextra -pthread \
          -fsanitize=address,undefined
LDFLAGS = -pthread -fsanitize=address,undefined
SRC     = $(wildcard src/*.c)
OBJ     = $(SRC:.c=.o)

server: $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

clean:
	rm -f $(OBJ) server


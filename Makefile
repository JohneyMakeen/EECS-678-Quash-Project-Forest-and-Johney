CC = gcc
CFLAGS = -Wall -Wextra -g

all: new_quash

quash: new_quash.c
	$(CC) $(CFLAGS) -o quash quash.c

clean:
	rm -f new_quash

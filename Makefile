CC = gcc
CFLAGS = -Wall -Wextra -g

all: quash

quash: quash.c
	$(CC) $(CFLAGS) -o quash quash.c

clean:
	rm -f quash

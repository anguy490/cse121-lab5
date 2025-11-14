CC = gcc
CFLAGS = -Wall -lwiringPi

all: send

send: send.c
	$(CC) send.c -o send $(CFLAGS)

clean:
	rm -f send

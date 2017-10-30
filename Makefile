CC=gcc

CFLAGS=-Wall -W -g -std=c99



all: client server

client: client.c raw.c
	$(CC) client.c raw.c $(CFLAGS) -o client

server: server.c 
	$(CC) server.c $(CFLAGS) -o server

clean:
	rm -f client server *.o

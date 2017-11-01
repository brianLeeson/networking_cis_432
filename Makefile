CC=g++

CFLAGS=-Wall -W -g

PROG_NAME = clie
ARGS = localhost 3000 bel


all: client server

client: client.c raw.c
	$(CC) client.c raw.c $(CFLAGS) -o client

server: server.c 
	$(CC) server.c raw.c $(CFLAGS) -o server

clean:
	rm -f client server *.o

waterproof_c: 
	(valgrind --track-origins=yes --leak-check=full  --show-leak-kinds=all -v ./client $(ARGS)) || true
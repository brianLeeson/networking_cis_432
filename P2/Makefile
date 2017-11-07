CC=g++

CFLAGS=-Wall -W -g

ARGS_C = localhost 3000 bel
ARGS_S = localhost 3000

all: client server

client: client.c raw.c listOfLists.h
	$(CC) client.c raw.c $(CFLAGS) -o client

server: server.c raw.c listOfLists.h
	$(CC) server.c raw.c  $(CFLAGS) -o server

clean:
	rm -f client server *.o

waterproof_c: 
	(valgrind --track-origins=yes --leak-check=full  --show-leak-kinds=all -v ./client $(ARGS_C)) || true
	
waterproof_s: 
	(valgrind --track-origins=yes --leak-check=full  --show-leak-kinds=all -v ./server $(ARGS_S)) || true
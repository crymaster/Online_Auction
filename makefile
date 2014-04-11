CC=gcc
CFLAGS=-Wall -g
OBJ=server.o client.o

all: server client

client.o: client.c
	$(CC) -c -o $@ $< $(CFLAGS)

server.o: server.c
	$(CC) -c -o $@ $< $(CFLAGS)

server: server.o
	gcc -o $@ server.o $(CFLAGS)
	
client: client.o
	gcc -o $@ client.o $(CFLAGS)

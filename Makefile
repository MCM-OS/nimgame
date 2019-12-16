all: server client

server: nimserver.c
	clang -lpthread nimserver.c -o server

client: nimclient.c
	clang nimclient.c -o client


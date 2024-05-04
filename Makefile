CC=gcc
FLAGS=-g3 -Wall -Wextra -Werror -pthread -fsanitize=address,undefined

all: client server

client: src/client.c src/chatroom.h 
	$(CC) $(FLAGS) -o client src/client.c

server: src/server.c src/chatroom.h 
	$(CC) $(FLAGS) -o server src/server.c

clean:
	rm -f client server

.PHONY: clean
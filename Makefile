CC=gcc
FLAGS=-g3 -Wall -Wextra -Werror -pthread

all: server

server: src/server.c src/chatroom.h 
	$(CC) $(FLAGS) -o server src/server.c

clean:
	rm -f server

.PHONY: clean
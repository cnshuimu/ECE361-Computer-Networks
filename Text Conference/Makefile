LDFLAGS = -pthread
CC = gcc
all: server client
server: server.c helper.h 
	gcc server.c -o server ${LDFLAGS}
deliver: client.c helper.h 
	gcc client.c -o client ${LDFLAGS}
clean:  
	rm -f *.o server client

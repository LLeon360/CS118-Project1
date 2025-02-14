CC=gcc
CPPFLAGS=-Wall -Wextra 
LDFLAGS= 
LDLIBS=

DEPS=transport.o io.o ooo_buffer.o ack_queue.o sender_window.o

all: server client 

server: server.o $(DEPS)
client: client.o $(DEPS)

clean:
	@rm -rf server client *.bin *.o	

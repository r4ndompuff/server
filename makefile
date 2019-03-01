all: server.o client.o
	gcc -pthread server.c -o server
	gcc -pthread client.c -o client

clean:
	rm -rf 'server' 'client' *.o

s:
	./server 5001

c:
	./client 5001 127.0.0.1

valgrind:
	valgrind --track-origins=yes --leak-check=full ./server

help:
	echo "God bless you. This is all help, that I can provide..."

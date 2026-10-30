CC = gcc
CFLAGS = -Wall -Wextra -std=gnu11
LDLIBS = -pthread


#make
#./server --port 20001 --tnum 4 &
#./server --port 20002 --tnum 4 &
#./server --port 20003 --tnum 4 &
#./client --k 10 --mod 1000000007 --servers servers.txt


all: client server

# Статическая библиотека с общим кодом клиента и сервера
libmultmodulo.a: multmodulo.o
	ar rcs $@ $^

multmodulo.o: multmodulo.c multmodulo.h
	$(CC) $(CFLAGS) -c multmodulo.c -o $@

client: client.c multmodulo.h libmultmodulo.a
	$(CC) $(CFLAGS) client.c -o $@ -L. -lmultmodulo $(LDLIBS)

server: server.c multmodulo.h libmultmodulo.a
	$(CC) $(CFLAGS) server.c -o $@ -L. -lmultmodulo $(LDLIBS)

clean:
	rm -f client server *.o *.a

.PHONY: all clean
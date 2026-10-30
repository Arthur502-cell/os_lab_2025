

# make          
#./tcpserver --port 15000 --bufsize 100
#./tcpclient --ip 127.0.0.1 --port 15000 --bufsize 100
#./udpserver --port 25000 --bufsize 1024
#./udpclient --ip 127.0.0.1 --port 25000 --bufsize 1024

CC = gcc
CFLAGS = -Wall -Wextra -std=gnu99

all: tcpclient tcpserver udpclient udpserver

tcpclient: tcpclient.c
	$(CC) $(CFLAGS) -o tcpclient tcpclient.c

tcpserver: tcpserver.c
	$(CC) $(CFLAGS) -o tcpserver tcpserver.c

udpclient: udpclient.c
	$(CC) $(CFLAGS) -o udpclient udpclient.c

udpserver: udpserver.c
	$(CC) $(CFLAGS) -o udpserver udpserver.c

clean:
	rm -f tcpclient tcpserver udpclient udpserver

.PHONY: all clean
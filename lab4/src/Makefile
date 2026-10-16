CC=gcc
CFLAGS=-I.

all : parallel_min_max process_memory psum

parallel_min_max : utils.o find_min_max.o utils.h find_min_max.h
	$(CC) -o parallel_min_max utils.o find_min_max.o parallel_min_max.c $(CFLAGS)

process_memory : process_memory.c
	$(CC) -o process_memory process_memory.c $(CFLAGS)

psum : parallel_sum.c utils.o libsum.a utils.h sum.h
	$(CC) -o psum parallel_sum.c utils.o -L. -lsum -lpthread $(CFLAGS)

libsum.a : sum.o
	ar rcs libsum.a sum.o

sum.o : sum.h
	$(CC) -o sum.o -c sum.c $(CFLAGS)

utils.o : utils.h
	$(CC) -o utils.o -c utils.c $(CFLAGS)

find_min_max.o : utils.h find_min_max.h
	$(CC) -o find_min_max.o -c find_min_max.c $(CFLAGS)

clean :
	rm -f utils.o find_min_max.o sum.o libsum.a parallel_min_max process_memory psum
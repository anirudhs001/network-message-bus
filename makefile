CC = gcc

all: local_server.o driver.o

local_server.o : local_server.c nmb.c error.c consts.h
	$(CC) local_server.c -o local_server.o

driver.o : driver.c nmb.c consts.h
	$(CC) driver.c -o driver.o

clean :
	rm *.o

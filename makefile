CC = gcc
CFLAGS = -Wall -pedantic -std=c99 -D_POSIX_C_SOURCE=200809L -lm -lssl -lcrypto -pthread
LDFLAGS =

.PHONY: clean

both: server.c client.c
	${CC} -o server.out server.c supportfile.c ${CFLAGS}
	${CC} -o client.out client.c supportfile.c ${CFLAGS}

clean:
	$(RM) server.out client.out *.o

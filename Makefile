CC     = gcc
CFLAGS = -Wall -Wextra -std=c11

all: anlex parser traductor

anlex: anlex.c
	$(CC) $(CFLAGS) -o anlex anlex.c

parser: parser.c
	$(CC) $(CFLAGS) -o parser parser.c

traductor: traductor.c
	$(CC) $(CFLAGS) -o traductor traductor.c

clean:
	rm -f anlex parser traductor *.o

.PHONY: all clean

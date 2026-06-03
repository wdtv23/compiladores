CC     = gcc
CFLAGS = -Wall -Wextra -std=c11

all: anlex parser

anlex: anlex.c
	$(CC) $(CFLAGS) -o anlex anlex.c

parser: parser.c
	$(CC) $(CFLAGS) -o parser parser.c

clean:
	rm -f anlex parser *.o

.PHONY: all clean

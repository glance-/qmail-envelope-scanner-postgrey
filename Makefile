CC=gcc
CFLAGS=-Wall -pedantic -ansi -O3

qmail-envelope-scanner: qmail-envelope-scanner-postgrey.c
	$(CC) $(CFLAGS) -o $@ $?
	strip $(BINARY)

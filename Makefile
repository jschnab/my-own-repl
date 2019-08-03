CC = gcc
CFLAGS = -Wall -std=c99
LDFLAGS = -ledit
repl: main.o
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<

CC = gcc
CFLAGS = -Wall -std=c99
LDFLAGS = -ledit
lispy: main.o mpc.o
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<

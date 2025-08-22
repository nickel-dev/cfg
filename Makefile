CC = cc
CFLAGS = -Wall -s -O2
SRC = example.c

example: example.c cfg.h
	$(CC) -o $@ $(SRC) $(CFLAGS)

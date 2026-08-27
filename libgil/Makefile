# Makefile for libgil — cross-platform C89 library

CC      = cc
CFLAGS  = -std=c89 -pedantic -Wall -Wextra -O2 -Iinclude -Isrc
AR      = ar rcs

SRCS    = src/intern.c src/lexer.c src/parser.c src/gil.c \
          src/load.c src/frontier.c src/query.c src/exec.c
OBJS    = $(SRCS:.c=.o)
TARGET  = libgil.a

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(OBJS)
	$(AR) $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

test: $(TARGET)
	$(CC) $(CFLAGS) test/test.c -L. -lgil -o test/test
	./test/test

clean:
	rm -f $(OBJS) $(TARGET) test/test.o test/test
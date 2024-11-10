# File: Makefile

CC = gcc
CFLAGS = -Wall -g
TARGET = simplesmartloader

all: $(TARGET)

$(TARGET): simplesmartloader.c
	$(CC) $(CFLAGS) -o $(TARGET) simplesmartloader.c

clean:
	rm -f $(TARGET)


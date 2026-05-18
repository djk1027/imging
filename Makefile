CC = gcc
CFLAGS = -g -Wall
LIBS = -lm

SRC_DIR = src
TARGETS = imging imging-cli 

SERVER_SRCS = $(SRC_DIR)/imging.c $(SRC_DIR)/thread.c
CLI_SRCS = $(SRC_DIR)/imging-cli.c

all: $(TARGETS)

imging: $(SERVER_SRCS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

imging-cli: $(CLI_SRCS) 
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)


clean:
	rm -rf $(TARGET)

.PHONY: all clean

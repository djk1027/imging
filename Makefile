CC = gcc
CFLAGS = -g -Wall
LIBS = -lm

SRC_DIR = src
TARGET = imging 

SRCS = $(SRC_DIR)/imging.c

all: $(TARGET)

$(TARGET): $(SRCS) 
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS) $(LIBS)

clean:
	rm -f $(TARGET)

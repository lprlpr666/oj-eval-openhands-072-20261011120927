CC = gcc
CFLAGS = -std=c11 -O2 -Wall -Wextra
TARGET = code
SOURCE = main.c

all: $(TARGET)

$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCE)

clean:
	rm -f $(TARGET) *.o

.PHONY: all clean
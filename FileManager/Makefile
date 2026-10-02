CC = clang
CFLAGS = -Wall -Wextra -O2
TARGET = files

all: $(TARGET)

$(TARGET): files.c
	$(CC) $(CFLAGS) files.c -o $(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all clean

CC = gcc
CFLAGS = -Wall -Wextra -std=c11
TARGET = myshell
SOURCES = shell.c parser.c executor.c builtins.c jobs.c signals.c history.c
OBJECTS = $(SOURCES:.c=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJECTS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)


CC = gcc
CFLAGS = -Wall -Wextra -g -I.
LDFLAGS = -lole32

# Directories
SOURCE_DIR = src

# Source Files
SOURCE_SRC = $(SOURCE_DIR)/main.c 

# Targets
all: main

main: $(SOURCE_SRC)
	$(CC) $(CFLAGS) -I$(SOURCE_DIR) $(SOURCE_SRC) -o main.exe $(LDFLAGS)

clean:
	if exist main.exe del main.exe

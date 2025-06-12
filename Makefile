# Compiler and flags
CC := gcc
CFLAGS := -Wall -Wextra -pedantic -std=c99

# Source files
SRC_DIR = src
SRC_FILES := $(wildcard $(SRC_DIR)/*.c)

# Ensure src/hldb.c is first
SRC_FILES := $(filter $(SRC_DIR)/hldb.c,$(SRC_FILES)) $(filter-out $(SRC_DIR)/hldb.c,$(SRC_FILES))

# Add main file
MAIN = koji.c

# Object files
OBJ_FILES := $(SRC_FILES:.c=.o) $(MAIN:.c=.o)

# Final executable
TARGET = koji

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJ_FILES)
	$(CC) $(CFLAGS) $^ -o $@

# Compile main.c and src/*.c files into .o files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ_FILES) $(TARGET)
# Compiler and flags
CC := gcc
CFLAGS := -Wall -Wextra -pedantic -std=c99

# Directories
SRC_DIR := src
BUILD_DIR := build

# Source files
SRC_FILES := $(wildcard $(SRC_DIR)/*.c)
MAIN_FILE := koji.c

# Object files
OBJ_FILES := $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRC_FILES)) \
             $(BUILD_DIR)/koji.o

# Output binary
TARGET := koji

# Default target
all: $(TARGET)

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Link object files into final binary
$(TARGET): $(OBJ_FILES)
	$(CC) $(OBJ_FILES) -o $@

# Compile source files in src/
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Compile main koji.c file
$(BUILD_DIR)/koji.o: koji.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up
clean:
	rm -rf $(BUILD_DIR) $(TARGET)
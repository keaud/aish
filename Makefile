# Makefile for AISH (AI Shell)

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c11 -D_DEFAULT_SOURCE -D_XOPEN_SOURCE=600
# Add include paths for json-c and curl if they're not in the standard include path
CFLAGS += -I/usr/local/include -I/opt/homebrew/include
LDFLAGS = -lcurl -ljson-c -lutil
# Add library paths for json-c and curl if they're not in the standard library path
LDFLAGS += -L/usr/local/lib -L/opt/homebrew/lib

# Directories
SRC_DIR = src
BUILD_DIR = build
BIN_DIR = bin

# Source files and objects
SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))

# Target executable
TARGET = $(BIN_DIR)/aish

# Default target
all: directories $(TARGET)

# Create necessary directories
directories:
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(BIN_DIR)

# Compile source files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Link object files
$(TARGET): $(OBJS)
	$(CC) $^ -o $@ $(LDFLAGS)

# Clean build files
clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

# Install the executable
install: all
	@echo "Installing AISH to /usr/local/bin..."
	@sudo cp $(TARGET) /usr/local/bin/
	@echo "Installation complete."

# Uninstall the executable
uninstall:
	@echo "Uninstalling AISH..."
	@sudo rm -f /usr/local/bin/aish
	@echo "Uninstallation complete."

# Run the executable
run: all
	$(TARGET)

# Help target
help:
	@echo "AISH (AI Shell) Makefile"
	@echo ""
	@echo "Targets:"
	@echo "  all       - Build the executable (default)"
	@echo "  clean     - Remove build files"
	@echo "  install   - Install the executable to /usr/local/bin"
	@echo "  uninstall - Remove the executable from /usr/local/bin"
	@echo "  run       - Build and run the executable"
	@echo "  help      - Display this help message"

.PHONY: all directories clean install uninstall run help

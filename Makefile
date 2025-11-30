CC = gcc

BUILD_DIR = build

TARGET := main

SRCS = $(wildcard ./*.c)
OBJS = $(patsubst ./%.c,$(BUILD_DIR)/%.o,$(SRCS))

.PHONY: all clean directories

all: directories $(TARGET)

directories:
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: ./%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@ -lcrypt

clean:
	rm -rf $(BUILD_DIR) ./main

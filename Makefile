# Makefile for LuRETools

CC = gcc
CFLAGS = -Wall -Wextra -Iinclude -g
SRC_DIR = src
OBJ_DIR = obj
BIN = luretools

SRCS = \
	$(SRC_DIR)/main.c \
	$(SRC_DIR)/lure_modules.c \
	$(SRC_DIR)/lure_disarm.c \
	$(SRC_DIR)/lure_cfg.c

# Parked until a real Ghidra/Sleigh/pypcode backend is added.
DISABLED_SRCS = \
	$(SRC_DIR)/lure_pcode.c

OBJS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))

$(BIN): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean run

run: $(BIN)
	./$(BIN)

clean:
	rm -rf $(OBJ_DIR) $(BIN)

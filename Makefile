debug ?= 0
NAME := rash
SRC_DIR := src
BUILD_DIR := build
INCLUDE_DIR := include
BIN_DIR := bin

OBJS := $(patsubst %.c,%.o, $(wildcard $(SRC_DIR)/*.c))

CC := gcc
CFLAGS := -Wall -Wextra -pedantic -Werror

ifeq ($(debug), 1)
	CFLAGS := $(CFLAGS) -g -Og
else
	CFLAGS := $(CFLAGS) -O3
endif

$(NAME): dir $(OBJS)
	$(CC) $(CFLAGS) -o $(BIN_DIR)/$@ $(patsubst %, build/%, $(OBJS))

$(OBJS): dir
	@mkdir -p $(BUILD_DIR)/$(@D)
	@$(CC) $(CFLAGS) -o $(BUILD_DIR)/$@ -c $*.c

check: $(NAME)
	valgrind -s --leak-check=full --show-leak-kinds=all $(BIN_DIR)/$(NAME)

dir:
	@mkdir -p $(BUILD_DIR) $(BIN_DIR)

clean:
	@rm -rf $(BUILD_DIR) $(BIN_DIR)

run: $(NAME)
	./$(BIN_DIR)/$(NAME)

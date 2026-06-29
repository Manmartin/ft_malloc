LIBRARY_NAME := libft_malloc

ifeq ($(HOSTTYPE),)
	HOSTTYPE := $(shell uname -m)_$(shell uname -s)
endif

NAME = $(BUILD_DIR)/$(LIBRARY_NAME)_$(HOSTTYPE).so

MKDIR = mkdir -p
RM = rm -rf

CC = gcc
CFLAGS = -Wall -Wextra -Werror -pedantic -std=gnu11

LDFLAGS =

BUILD_DIR := build
SRC_DIR := src

SRCS := src/malloc.c
OBJS := $(SRCS:%.c=$(BUILD_DIR)/%.o)

all: $(NAME)

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

$(NAME): $(OBJS)
	$(CC) $(CFLAGS) -shared -o $@ $(OBJS)
	ln -sf $(NAME) $(LIBRARY_NAME).so

LIBRARY_PATH= "${LD_LIBRARY_PATH}"
test: $(NAME)
	$(CC) $(CFLAGS) tests/main.c -o $@ -L. -lft_malloc
	LD_LIBRARY_PATH="$(shell pwd)" ./$@

debug:: CFLAGS += -g3 -DMALLOC_DEBUG #-fsanitize=address -fsanitize=leak -fsanitize=undefined -fsanitize=bounds -fsanitize=null 
debug:: fclean $(NAME)

clean:
	$(RM) $(OBJS)
fclean: clean
	$(RM) $(LIBRARY_NAME).so
	$(RM) $(BUILD_DIR)

re: fclean all

.PHONY: test debug all sanitize clean fclean re

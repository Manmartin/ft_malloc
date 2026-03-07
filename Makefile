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
	@rm -rf $(LIBRARY_NAME).so
	ln -s $(NAME) $(LIBRARY_NAME).so

test:
	$(CC) $(CFLAGS) main.c

debug:: CFLAGS += -g3 -fsanitize=address -fsanitize=leak -fsanitize=undefined -fsanitize=bounds -fsanitize=null
debug:: fclean $(NAME)

clean:
	$(RM) $(OBJS)
fclean: clean
	$(RM) $()
	$(RM) $(BUILD_DIR)

re: fclean all

.PHONY: all sanitize clean fclean re

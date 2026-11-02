LIBRARY_NAME := libft_malloc
LINK_NAME := ft_malloc

ifeq ($(HOSTTYPE),)
	HOSTTYPE := $(shell uname -m)_$(shell uname -s)
endif

NAME = $(BUILD_DIR)/$(LIBRARY_NAME)_$(HOSTTYPE).so

MKDIR = mkdir -p
RM = rm -rf

CC = gcc
CFLAGS = -Wall -Wextra -Werror -pedantic -std=gnu11

LDFLAGS = -L. -l$(LINK_NAME)

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

# LD_LIBRARY_PATH=./ ./test
test: $(NAME)
	$(CC) $(CFLAGS) -o $@ main.c $(LDFLAGS)

debug:: CFLAGS += -g3 -fsanitize=address -fsanitize=leak -fsanitize=undefined -fsanitize=bounds -fsanitize=null
debug:: fclean $(NAME)

clean:
	$(RM) $(OBJS)
	$(RM) test

fclean: clean
	$(RM) $(LIBRARY_NAME).so
	$(RM) $(BUILD_DIR)

re: fclean all

.PHONY: all sanitize clean fclean re

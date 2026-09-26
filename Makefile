NAME := mymsh
CC ?= cc
CPPFLAGS ?= -D_POSIX_C_SOURCE=200809L -I.
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Werror -O2
LDFLAGS ?=

SOURCES := mymsh.c parser.c executor.c
OBJECTS := $(SOURCES:.c=.o)
TEST_BINARY := build/test_parser

.PHONY: all clean fclean re test sanitize

all: $(NAME)

$(NAME): $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(TEST_BINARY): tests/test_parser.c parser.c parser.h
	mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_parser.c parser.c $(LDFLAGS) -o $@

test: $(NAME) $(TEST_BINARY)
	./$(TEST_BINARY)
	sh tests/test_shell.sh ./$(NAME)

sanitize: CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -Werror -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer
sanitize: LDFLAGS := -fsanitize=address,undefined
sanitize: clean test

clean:
	rm -f $(OBJECTS) $(NAME)
	rm -rf build $(NAME).dSYM

fclean: clean

re: fclean all

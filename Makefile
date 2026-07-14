NAME := mymsh

CC ?= cc
CFLAGS ?= -Wall -Wextra -g

UNAME_S := $(shell uname -s)
UNAME_M := $(shell uname -m)
LIBPARSER_ORIGIN := $(origin LIBPARSER)

ARCHFLAGS :=
UNSUPPORTED_MSG :=

ifeq ($(LIBPARSER_ORIGIN), undefined)
  ifeq ($(UNAME_S), Darwin)
    ifeq ($(UNAME_M), arm64)
      LIBPARSER := libparserARMMac.a
      ARCHFLAGS += -arch arm64
    else
      UNSUPPORTED_MSG := no hay libreria parser incluida para macOS $(UNAME_M). Usa un Mac Apple Silicon, Linux x86_64, o ejecuta make LIBPARSER=ruta/a/libparser.a con una libreria compatible.
    endif
  else ifeq ($(UNAME_S), Linux)
    ifeq ($(UNAME_M), x86_64)
      LIBPARSER := libparser_64.a
    else ifeq ($(UNAME_M), aarch64)
      LIBPARSER := libparserARMLinux.a
    else ifeq ($(UNAME_M), arm64)
      LIBPARSER := libparserARMLinux.a
    else ifeq ($(UNAME_M), i386)
      LIBPARSER := libparser.a
    else ifeq ($(UNAME_M), i686)
      LIBPARSER := libparser.a
    else
      UNSUPPORTED_MSG := no hay libreria parser incluida para Linux $(UNAME_M). Ejecuta make LIBPARSER=ruta/a/libparser.a con una libreria compatible.
    endif
  else
    UNSUPPORTED_MSG := sistema no reconocido: $(UNAME_S) $(UNAME_M). Ejecuta make LIBPARSER=ruta/a/libparser.a con una libreria compatible.
  endif
endif

.PHONY: all clean fclean re parser-info check-parser smoke

all: $(NAME)

$(NAME): check-parser mymsh.c parser.h
	$(CC) $(ARCHFLAGS) $(CFLAGS) mymsh.c $(LIBPARSER) -o $(NAME)

check-parser:
	@if [ -n "$(UNSUPPORTED_MSG)" ]; then \
		printf '%s\n' "Error: $(UNSUPPORTED_MSG)"; \
		exit 1; \
	fi
	@if [ -z "$(LIBPARSER)" ]; then \
		printf '%s\n' "Error: no se ha definido LIBPARSER."; \
		exit 1; \
	fi
	@if [ ! -f "$(LIBPARSER)" ]; then \
		printf '%s\n' "Error: no existe la libreria $(LIBPARSER)."; \
		exit 1; \
	fi

parser-info:
	@printf '%s\n' "Sistema detectado: $(UNAME_S) $(UNAME_M)"
	@printf '%s\n' "Libreria seleccionada: $(LIBPARSER)"
	@if [ -n "$(UNSUPPORTED_MSG)" ]; then \
		printf '%s\n' "Aviso: $(UNSUPPORTED_MSG)"; \
	fi

smoke: $(NAME)
	./$(NAME) "echo minishell"
	./$(NAME) "echo hola | wc -c"

clean:
	rm -f $(NAME)
	rm -rf $(NAME).dSYM

fclean: clean

re: fclean all

#include "parser.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef enum {
    TOKEN_WORD,
    TOKEN_PIPE,
    TOKEN_INPUT,
    TOKEN_OUTPUT,
    TOKEN_APPEND,
    TOKEN_ERROR,
    TOKEN_ERROR_APPEND,
    TOKEN_BACKGROUND
} TokenType;

typedef struct {
    TokenType type;
    char *text;
} Token;

typedef struct {
    Token *items;
    size_t count;
    size_t capacity;
} TokenList;

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} Buffer;

static int buffer_append(Buffer *buffer, char character) {
    char *grown;

    if (buffer->length + 1 >= buffer->capacity) {
        size_t capacity = buffer->capacity == 0 ? 32 : buffer->capacity * 2;
        grown = realloc(buffer->data, capacity);
        if (grown == NULL) {
            return -1;
        }
        buffer->data = grown;
        buffer->capacity = capacity;
    }
    buffer->data[buffer->length++] = character;
    buffer->data[buffer->length] = '\0';
    return 0;
}

static int buffer_append_text(Buffer *buffer, const char *text) {
    while (*text != '\0') {
        if (buffer_append(buffer, *text++) == -1) {
            return -1;
        }
    }
    return 0;
}

static int append_expansion(Buffer *buffer, const char *input, size_t *position) {
    size_t start;
    char *name;
    const char *value;

    (*position)++;
    if (input[*position] == '$') {
        char pid[32];
        snprintf(pid, sizeof(pid), "%ld", (long)getpid());
        (*position)++;
        return buffer_append_text(buffer, pid);
    }
    if (input[*position] == '{') {
        (*position)++;
        start = *position;
        while (input[*position] != '\0' && input[*position] != '}') {
            (*position)++;
        }
        if (input[*position] != '}') {
            return -2;
        }
    } else {
        start = *position;
        if (!(isalpha((unsigned char)input[*position]) || input[*position] == '_')) {
            return buffer_append(buffer, '$');
        }
        while (isalnum((unsigned char)input[*position]) || input[*position] == '_') {
            (*position)++;
        }
    }

    name = malloc(*position - start + 1);
    if (name == NULL) {
        return -1;
    }
    memcpy(name, input + start, *position - start);
    name[*position - start] = '\0';
    value = getenv(name);
    free(name);
    if (input[*position] == '}') {
        (*position)++;
    }
    return value == NULL ? 0 : buffer_append_text(buffer, value);
}

static int token_list_add(TokenList *tokens, TokenType type, char *text) {
    Token *grown;

    if (tokens->count == tokens->capacity) {
        size_t capacity = tokens->capacity == 0 ? 16 : tokens->capacity * 2;
        grown = realloc(tokens->items, capacity * sizeof(*grown));
        if (grown == NULL) {
            return -1;
        }
        tokens->items = grown;
        tokens->capacity = capacity;
    }
    tokens->items[tokens->count].type = type;
    tokens->items[tokens->count].text = text;
    tokens->count++;
    return 0;
}

static void free_tokens(TokenList *tokens) {
    size_t i;

    for (i = 0; i < tokens->count; i++) {
        free(tokens->items[i].text);
    }
    free(tokens->items);
}

static int add_operator(const char *input, size_t *position, TokenList *tokens) {
    TokenType type;

    if (input[*position] == '|') {
        type = TOKEN_PIPE;
        (*position)++;
    } else if (input[*position] == '&') {
        type = TOKEN_BACKGROUND;
        (*position)++;
    } else if (input[*position] == '<') {
        type = TOKEN_INPUT;
        (*position)++;
    } else if (input[*position] == '>') {
        (*position)++;
        if (input[*position] == '>') {
            type = TOKEN_APPEND;
            (*position)++;
        } else {
            type = TOKEN_OUTPUT;
        }
    } else {
        (*position) += 2;
        if (input[*position] == '>') {
            type = TOKEN_ERROR_APPEND;
            (*position)++;
        } else {
            type = TOKEN_ERROR;
        }
    }
    return token_list_add(tokens, type, NULL);
}

static int lex_word(const char *input, size_t *position, TokenList *tokens, char **error) {
    Buffer word = {0};
    char quote = '\0';
    int started = 0;

    while (input[*position] != '\0') {
        char current = input[*position];

        if (quote == '\0' && (isspace((unsigned char)current) || current == '|' ||
            current == '&' || current == '<' || current == '>')) {
            break;
        }
        if (quote == '\0' && current == '2' && input[*position + 1] == '>' && !started) {
            break;
        }
        started = 1;
        if (current == '\\' && quote != '\'') {
            (*position)++;
            if (input[*position] == '\0') {
                *error = strdup("barra invertida sin caracter posterior");
                free(word.data);
                return *error == NULL ? -1 : 1;
            }
            if (quote == '"' && input[*position] != '$' && input[*position] != '"' &&
                input[*position] != '\\' && input[*position] != '\n' &&
                buffer_append(&word, '\\') == -1) {
                free(word.data);
                return -1;
            }
            if (input[*position] != '\n' && buffer_append(&word, input[*position]) == -1) {
                free(word.data);
                return -1;
            }
            (*position)++;
        } else if (current == '\'' || current == '"') {
            if (quote == '\0') {
                quote = current;
                (*position)++;
            } else if (quote == current) {
                quote = '\0';
                (*position)++;
            } else {
                if (buffer_append(&word, current) == -1) {
                    free(word.data);
                    return -1;
                }
                (*position)++;
            }
        } else if (current == '$' && quote != '\'') {
            int result = append_expansion(&word, input, position);
            if (result == -2) {
                *error = strdup("expansion ${...} sin cierre");
                free(word.data);
                return *error == NULL ? -1 : 1;
            }
            if (result == -1) {
                free(word.data);
                return -1;
            }
        } else {
            if (buffer_append(&word, current) == -1) {
                free(word.data);
                return -1;
            }
            (*position)++;
        }
    }

    if (quote != '\0') {
        *error = strdup("comillas sin cierre");
        free(word.data);
        return *error == NULL ? -1 : 1;
    }
    if (word.data == NULL) {
        word.data = strdup("");
        if (word.data == NULL) {
            return -1;
        }
    }
    return token_list_add(tokens, TOKEN_WORD, word.data);
}

static int lex(const char *input, TokenList *tokens, char **error) {
    size_t position = 0;

    while (input[position] != '\0') {
        while (isspace((unsigned char)input[position])) {
            position++;
        }
        if (input[position] == '\0') {
            break;
        }
        if (input[position] == '|' || input[position] == '&' || input[position] == '<' ||
            input[position] == '>' || (input[position] == '2' && input[position + 1] == '>')) {
            if (add_operator(input, &position, tokens) == -1) {
                return -1;
            }
        } else {
            int result = lex_word(input, &position, tokens, error);
            if (result != 0) {
                return result;
            }
        }
    }
    return 0;
}

static int add_command(tline *line) {
    tcommand *grown = realloc(line->commands, (size_t)(line->ncommands + 1) * sizeof(*grown));

    if (grown == NULL) {
        return -1;
    }
    line->commands = grown;
    line->commands[line->ncommands].argc = 0;
    line->commands[line->ncommands].argv = NULL;
    line->ncommands++;
    return 0;
}

static int add_argument(tcommand *command, const char *argument) {
    char **grown = realloc(command->argv, (size_t)(command->argc + 2) * sizeof(*grown));

    if (grown == NULL) {
        return -1;
    }
    command->argv = grown;
    command->argv[command->argc] = strdup(argument);
    if (command->argv[command->argc] == NULL) {
        return -1;
    }
    command->argc++;
    command->argv[command->argc] = NULL;
    return 0;
}

static int set_error(tline *line, const char *message) {
    line->error = strdup(message);
    return line->error == NULL ? -1 : 1;
}

static int set_redirect(char **destination, const char *value, tline *line) {
    if (*destination != NULL) {
        return set_error(line, "redireccion duplicada");
    }
    *destination = strdup(value);
    return *destination == NULL ? -1 : 0;
}

static int parse_tokens(tline *line, const TokenList *tokens) {
    size_t index;

    if (tokens->count == 0) {
        return 0;
    }
    if (add_command(line) == -1) {
        return -1;
    }

    for (index = 0; index < tokens->count; index++) {
        Token token = tokens->items[index];
        tcommand *command = &line->commands[line->ncommands - 1];

        if (token.type == TOKEN_WORD) {
            if (add_argument(command, token.text) == -1) {
                return -1;
            }
        } else if (token.type == TOKEN_PIPE) {
            if (command->argc == 0) {
                return set_error(line, "pipe sin comando a la izquierda");
            }
            if (add_command(line) == -1) {
                return -1;
            }
        } else if (token.type == TOKEN_BACKGROUND) {
            if (index + 1 != tokens->count || command->argc == 0) {
                return set_error(line, "& solo se admite al final de una orden");
            }
            line->background = 1;
        } else {
            char **destination;
            if (index + 1 >= tokens->count || tokens->items[index + 1].type != TOKEN_WORD) {
                return set_error(line, "redireccion sin nombre de fichero");
            }
            if (token.type == TOKEN_INPUT) {
                destination = &line->redirect_input;
            } else if (token.type == TOKEN_OUTPUT || token.type == TOKEN_APPEND) {
                destination = &line->redirect_output;
                line->output_append = token.type == TOKEN_APPEND;
            } else {
                destination = &line->redirect_error;
                line->error_append = token.type == TOKEN_ERROR_APPEND;
            }
            if (set_redirect(destination, tokens->items[++index].text, line) != 0) {
                return line->error == NULL ? -1 : 1;
            }
        }
    }

    if (line->commands[line->ncommands - 1].argc == 0) {
        return set_error(line, "pipe sin comando a la derecha");
    }
    return 0;
}

tline *parse_line(const char *input) {
    tline *line = calloc(1, sizeof(*line));
    TokenList tokens = {0};
    char *lex_error = NULL;
    int result;

    if (line == NULL) {
        return NULL;
    }
    result = lex(input, &tokens, &lex_error);
    if (result == -1) {
        free_tokens(&tokens);
        free_line(line);
        return NULL;
    }
    if (result == 1) {
        line->error = lex_error;
    } else if (parse_tokens(line, &tokens) == -1) {
        free_tokens(&tokens);
        free_line(line);
        return NULL;
    }
    free_tokens(&tokens);
    return line;
}

void free_line(tline *line) {
    int command_index;

    if (line == NULL) {
        return;
    }
    for (command_index = 0; command_index < line->ncommands; command_index++) {
        int argument_index;
        for (argument_index = 0; argument_index < line->commands[command_index].argc; argument_index++) {
            free(line->commands[command_index].argv[argument_index]);
        }
        free(line->commands[command_index].argv);
    }
    free(line->commands);
    free(line->redirect_input);
    free(line->redirect_output);
    free(line->redirect_error);
    free(line->error);
    free(line);
}

#ifndef MINISHELL_PARSER_H
#define MINISHELL_PARSER_H

typedef struct {
    int argc;
    char **argv;
} tcommand;

typedef struct {
    int ncommands;
    tcommand *commands;
    char *redirect_input;
    char *redirect_output;
    char *redirect_error;
    int output_append;
    int error_append;
    int background;
    char *error;
} tline;

tline *parse_line(const char *input);
void free_line(tline *line);

#endif

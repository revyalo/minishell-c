#include "parser.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_pipeline_and_redirects(void) {
    tline *line = parse_line("cat < input.txt | grep 'hola mundo' >> output.txt 2>> errors.txt &");

    assert(line != NULL);
    assert(line->error == NULL);
    assert(line->ncommands == 2);
    assert(strcmp(line->commands[0].argv[0], "cat") == 0);
    assert(strcmp(line->commands[1].argv[1], "hola mundo") == 0);
    assert(strcmp(line->redirect_input, "input.txt") == 0);
    assert(strcmp(line->redirect_output, "output.txt") == 0);
    assert(strcmp(line->redirect_error, "errors.txt") == 0);
    assert(line->output_append);
    assert(line->error_append);
    assert(line->background);
    free_line(line);
}

static void test_invalid_syntax(void) {
    tline *line = parse_line("echo hola |");

    assert(line != NULL);
    assert(line->error != NULL);
    free_line(line);
}

int main(void) {
    test_pipeline_and_redirects();
    test_invalid_syntax();
    puts("parser tests: OK");
    return 0;
}

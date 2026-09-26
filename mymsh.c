#include "executor.h"
#include "parser.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define INPUT_SIZE 4096

static int install_signal_handlers(void) {
    struct sigaction action;

    memset(&action, 0, sizeof(action));
    action.sa_handler = SIG_IGN;
    if (sigemptyset(&action.sa_mask) == -1) {
        return -1;
    }
    if (sigaction(SIGINT, &action, NULL) == -1 ||
        sigaction(SIGQUIT, &action, NULL) == -1 ||
        sigaction(SIGTTOU, &action, NULL) == -1) {
        return -1;
    }
    return 0;
}

static int run_line(ShellState *shell, const char *input) {
    tline *line = parse_line(input);
    int status;

    if (line == NULL) {
        fprintf(stderr, "msh: memoria insuficiente\n");
        return 1;
    }
    if (line->error != NULL) {
        fprintf(stderr, "msh: error de sintaxis: %s\n", line->error);
        free_line(line);
        return 2;
    }
    if (line->ncommands == 0) {
        free_line(line);
        return 0;
    }

    status = shell_execute(shell, line, input);
    free_line(line);
    return status;
}

int main(int argc, char *argv[]) {
    ShellState shell;
    char input[INPUT_SIZE];
    int status = 0;
    int interactive = isatty(STDIN_FILENO);

    if (argc > 2) {
        fprintf(stderr, "Uso: %s [\"comando\"]\n", argv[0]);
        return 2;
    }
    if (install_signal_handlers() == -1) {
        perror("msh: sigaction");
        return 1;
    }
    shell_init(&shell, interactive);

    if (argc == 2) {
        status = run_line(&shell, argv[1]);
        shell_destroy(&shell);
        return status;
    }

    while (!shell.should_exit) {
        shell_reap_jobs(&shell);
        if (interactive) {
            fputs("msh> ", stdout);
            fflush(stdout);
        }
        if (fgets(input, sizeof(input), stdin) == NULL) {
            if (ferror(stdin) && errno == EINTR) {
                clearerr(stdin);
                continue;
            }
            if (interactive) {
                putchar('\n');
            }
            break;
        }
        if (strchr(input, '\n') == NULL && !feof(stdin)) {
            int character;
            while ((character = getchar()) != '\n' && character != EOF) {
            }
            fprintf(stderr, "msh: linea demasiado larga (maximo %d caracteres)\n", INPUT_SIZE - 2);
            status = 2;
            continue;
        }
        input[strcspn(input, "\r\n")] = '\0';
        status = run_line(&shell, input);
        shell.last_status = status;
    }

    if (shell.should_exit) {
        status = shell.exit_status;
    }
    shell_destroy(&shell);
    return status;
}

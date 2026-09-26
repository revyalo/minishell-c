#ifndef MINISHELL_EXECUTOR_H
#define MINISHELL_EXECUTOR_H

#include "parser.h"

#include <sys/types.h>

#define MAX_JOBS 16

typedef struct {
    pid_t *pids;
    int process_count;
    pid_t process_group;
    char *command;
    unsigned long sequence;
    int active;
} Job;

typedef struct {
    Job jobs[MAX_JOBS];
    unsigned long next_sequence;
    pid_t shell_process_group;
    int interactive;
    int should_exit;
    int exit_status;
    int last_status;
} ShellState;

void shell_init(ShellState *shell, int interactive);
void shell_destroy(ShellState *shell);
void shell_reap_jobs(ShellState *shell);
int shell_execute(ShellState *shell, const tline *line, const char *source);

#endif

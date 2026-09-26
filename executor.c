#include "executor.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

static void release_job(Job *job) {
    free(job->pids);
    free(job->command);
    memset(job, 0, sizeof(*job));
}

void shell_init(ShellState *shell, int interactive) {
    memset(shell, 0, sizeof(*shell));
    shell->interactive = interactive;
    shell->shell_process_group = getpgrp();
}

void shell_destroy(ShellState *shell) {
    int index;

    shell_reap_jobs(shell);
    for (index = 0; index < MAX_JOBS; index++) {
        release_job(&shell->jobs[index]);
    }
}

static int job_has_processes(const Job *job) {
    int index;

    for (index = 0; index < job->process_count; index++) {
        if (job->pids[index] > 0) {
            return 1;
        }
    }
    return 0;
}

void shell_reap_jobs(ShellState *shell) {
    pid_t pid;
    int status;
    int job_index;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        for (job_index = 0; job_index < MAX_JOBS; job_index++) {
            Job *job = &shell->jobs[job_index];
            int process_index;

            if (!job->active) {
                continue;
            }
            for (process_index = 0; process_index < job->process_count; process_index++) {
                if (job->pids[process_index] == pid) {
                    job->pids[process_index] = 0;
                }
            }
            if (!job_has_processes(job)) {
                if (shell->interactive) {
                    printf("[%d] terminado  %s\n", job_index + 1, job->command);
                }
                release_job(job);
            }
        }
    }
}

static int add_job(ShellState *shell, pid_t *pids, int count, pid_t group, const char *source) {
    int index;

    for (index = 0; index < MAX_JOBS; index++) {
        Job *job = &shell->jobs[index];
        if (!job->active) {
            job->pids = pids;
            job->process_count = count;
            job->process_group = group;
            job->command = strdup(source);
            if (job->command == NULL) {
                job->pids = NULL;
                return -1;
            }
            job->sequence = ++shell->next_sequence;
            job->active = 1;
            printf("[%d] %ld\n", index + 1, (long)group);
            return 0;
        }
    }
    fprintf(stderr, "msh: limite de %d trabajos en segundo plano alcanzado\n", MAX_JOBS);
    return -1;
}

static void list_jobs(ShellState *shell) {
    int index;

    shell_reap_jobs(shell);
    for (index = 0; index < MAX_JOBS; index++) {
        if (shell->jobs[index].active) {
            printf("[%d] ejecutando  %s\n", index + 1, shell->jobs[index].command);
        }
    }
}

static int newest_job(const ShellState *shell) {
    int index;
    int selected = -1;
    unsigned long newest = 0;

    for (index = 0; index < MAX_JOBS; index++) {
        if (shell->jobs[index].active && shell->jobs[index].sequence > newest) {
            newest = shell->jobs[index].sequence;
            selected = index;
        }
    }
    return selected;
}

static int wait_for_pid(pid_t pid) {
    int status;

    while (waitpid(pid, &status, 0) == -1) {
        if (errno == EINTR) {
            continue;
        }
        if (errno == ECHILD) {
            return 0;
        }
        perror("msh: waitpid");
        return 1;
    }
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    if (WIFSIGNALED(status)) {
        return 128 + WTERMSIG(status);
    }
    return 0;
}

static int foreground_job(ShellState *shell, const tcommand *command) {
    char *end = NULL;
    long requested;
    int index;
    int process_index;
    int status = 0;
    Job *job;

    shell_reap_jobs(shell);
    if (command->argc == 1) {
        index = newest_job(shell);
    } else {
        requested = strtol(command->argv[1], &end, 10);
        if (*command->argv[1] == '\0' || *end != '\0' || requested < 1 || requested > MAX_JOBS) {
            fprintf(stderr, "fg: identificador no valido: %s\n", command->argv[1]);
            return 2;
        }
        index = (int)requested - 1;
    }
    if (index < 0 || !shell->jobs[index].active) {
        fprintf(stderr, "fg: no existe ese trabajo\n");
        return 1;
    }

    job = &shell->jobs[index];
    printf("%s\n", job->command);
    if (shell->interactive && tcsetpgrp(STDIN_FILENO, job->process_group) == -1) {
        perror("fg: tcsetpgrp");
    }
    if (kill(-job->process_group, SIGCONT) == -1 && errno != ESRCH) {
        perror("fg: SIGCONT");
    }
    for (process_index = 0; process_index < job->process_count; process_index++) {
        if (job->pids[process_index] > 0) {
            status = wait_for_pid(job->pids[process_index]);
            job->pids[process_index] = 0;
        }
    }
    if (shell->interactive && tcsetpgrp(STDIN_FILENO, shell->shell_process_group) == -1) {
        perror("fg: restaurar terminal");
    }
    release_job(job);
    return status;
}

static int builtin_cd(const tcommand *command) {
    const char *directory;

    if (command->argc > 2) {
        fprintf(stderr, "cd: demasiados argumentos\n");
        return 2;
    }
    directory = command->argc == 1 ? getenv("HOME") : command->argv[1];
    if (directory == NULL) {
        fprintf(stderr, "cd: HOME no esta definida\n");
        return 1;
    }
    if (chdir(directory) == -1) {
        perror("cd");
        return 1;
    }
    return 0;
}

static int builtin_pwd(void) {
    char *directory = getcwd(NULL, 0);

    if (directory == NULL) {
        perror("pwd");
        return 1;
    }
    puts(directory);
    free(directory);
    return 0;
}

static int builtin_umask(const tcommand *command) {
    mode_t current;
    char *end = NULL;
    long value;

    if (command->argc == 1) {
        current = umask(0);
        umask(current);
        printf("%04o\n", current);
        return 0;
    }
    if (command->argc != 2) {
        fprintf(stderr, "umask: uso: umask [modo-octal]\n");
        return 2;
    }
    value = strtol(command->argv[1], &end, 8);
    if (*command->argv[1] == '\0' || *end != '\0' || value < 0 || value > 0777) {
        fprintf(stderr, "umask: modo no valido: %s\n", command->argv[1]);
        return 2;
    }
    umask((mode_t)value);
    return 0;
}

static int run_builtin(ShellState *shell, const tline *line, int *handled) {
    const tcommand *command = &line->commands[0];
    const char *name = command->argv[0];

    *handled = 1;
    if (strcmp(name, "cd") == 0) {
        return builtin_cd(command);
    }
    if (strcmp(name, "pwd") == 0) {
        return builtin_pwd();
    }
    if (strcmp(name, "umask") == 0) {
        return builtin_umask(command);
    }
    if (strcmp(name, "jobs") == 0) {
        list_jobs(shell);
        return 0;
    }
    if (strcmp(name, "fg") == 0) {
        return foreground_job(shell, command);
    }
    if (strcmp(name, "exit") == 0) {
        long value = shell->last_status;
        char *end = NULL;
        if (command->argc > 2) {
            fprintf(stderr, "exit: demasiados argumentos\n");
            return 2;
        }
        if (command->argc == 2) {
            value = strtol(command->argv[1], &end, 10);
            if (*command->argv[1] == '\0' || *end != '\0') {
                fprintf(stderr, "exit: se esperaba un estado numerico\n");
                value = 2;
            }
        }
        shell->should_exit = 1;
        shell->exit_status = (unsigned char)value;
        return shell->exit_status;
    }
    *handled = 0;
    return 0;
}

static int redirect_descriptor(const char *path, int target, int flags) {
    int descriptor = open(path, flags, 0666);

    if (descriptor == -1) {
        perror(path);
        return -1;
    }
    if (dup2(descriptor, target) == -1) {
        perror("msh: dup2");
        close(descriptor);
        return -1;
    }
    if (close(descriptor) == -1) {
        perror("msh: close");
        return -1;
    }
    return 0;
}

static int apply_redirections(const tline *line, int command_index) {
    if (command_index == 0 && line->redirect_input != NULL &&
        redirect_descriptor(line->redirect_input, STDIN_FILENO, O_RDONLY) == -1) {
        return -1;
    }
    if (command_index == line->ncommands - 1 && line->redirect_output != NULL) {
        int flags = O_WRONLY | O_CREAT | (line->output_append ? O_APPEND : O_TRUNC);
        if (redirect_descriptor(line->redirect_output, STDOUT_FILENO, flags) == -1) {
            return -1;
        }
    }
    if (line->redirect_error != NULL) {
        int flags = O_WRONLY | O_CREAT | (line->error_append ? O_APPEND : O_TRUNC);
        if (redirect_descriptor(line->redirect_error, STDERR_FILENO, flags) == -1) {
            return -1;
        }
    }
    return 0;
}

static void close_pipes(int (*pipes)[2], int pipe_count) {
    int index;

    for (index = 0; index < pipe_count; index++) {
        close(pipes[index][0]);
        close(pipes[index][1]);
    }
}

static int execute_pipeline(ShellState *shell, const tline *line, const char *source) {
    int pipe_count = line->ncommands - 1;
    int (*pipes)[2] = NULL;
    pid_t *pids = calloc((size_t)line->ncommands, sizeof(*pids));
    pid_t group = 0;
    int command_index;
    int created = 0;
    int status = 0;

    if (pids == NULL) {
        perror("msh: calloc");
        return 1;
    }
    if (pipe_count > 0) {
        pipes = malloc((size_t)pipe_count * sizeof(*pipes));
        if (pipes == NULL) {
            perror("msh: malloc");
            free(pids);
            return 1;
        }
        for (command_index = 0; command_index < pipe_count; command_index++) {
            if (pipe(pipes[command_index]) == -1) {
                perror("msh: pipe");
                close_pipes(pipes, command_index);
                free(pipes);
                free(pids);
                return 1;
            }
        }
    }

    for (command_index = 0; command_index < line->ncommands; command_index++) {
        pid_t pid = fork();
        if (pid == -1) {
            perror("msh: fork");
            break;
        }
        if (pid == 0) {
            struct sigaction action;
            memset(&action, 0, sizeof(action));
            action.sa_handler = SIG_DFL;
            sigemptyset(&action.sa_mask);
            sigaction(SIGINT, &action, NULL);
            sigaction(SIGQUIT, &action, NULL);
            if (line->background) {
                setpgid(0, group == 0 ? 0 : group);
            }
            if (command_index > 0 && dup2(pipes[command_index - 1][0], STDIN_FILENO) == -1) {
                perror("msh: dup2");
                _exit(126);
            }
            if (command_index < pipe_count && dup2(pipes[command_index][1], STDOUT_FILENO) == -1) {
                perror("msh: dup2");
                _exit(126);
            }
            close_pipes(pipes, pipe_count);
            if (apply_redirections(line, command_index) == -1) {
                _exit(126);
            }
            execvp(line->commands[command_index].argv[0], line->commands[command_index].argv);
            fprintf(stderr, "msh: %s: %s\n", line->commands[command_index].argv[0], strerror(errno));
            _exit(errno == ENOENT ? 127 : 126);
        }
        if (group == 0) {
            group = pid;
        }
        if (line->background && setpgid(pid, group) == -1 && errno != EACCES && errno != ESRCH) {
            perror("msh: setpgid");
        }
        pids[command_index] = pid;
        created++;
    }
    close_pipes(pipes, pipe_count);
    free(pipes);

    if (created != line->ncommands) {
        for (command_index = 0; command_index < created; command_index++) {
            wait_for_pid(pids[command_index]);
        }
        free(pids);
        return 1;
    }
    if (line->background) {
        if (add_job(shell, pids, line->ncommands, group, source) == -1) {
            free(pids);
            return 1;
        }
        return 0;
    }
    for (command_index = 0; command_index < line->ncommands; command_index++) {
        status = wait_for_pid(pids[command_index]);
    }
    free(pids);
    return status;
}

int shell_execute(ShellState *shell, const tline *line, const char *source) {
    int handled = 0;

    if (line->ncommands == 1 && !line->background && line->redirect_input == NULL &&
        line->redirect_output == NULL && line->redirect_error == NULL) {
        int status = run_builtin(shell, line, &handled);
        if (handled) {
            return status;
        }
    }
    return execute_pipeline(shell, line, source);
}

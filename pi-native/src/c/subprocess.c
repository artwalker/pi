#include "pi/c/subprocess.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

pi_subprocess_result pi_subprocess_run(const char *command) {
    pi_subprocess_result result;
    result.exit_code  = -1;
    result.output     = NULL;
    result.output_len = 0;

    int pipefd[2];
    if (pipe(pipefd) != 0) {
        result.output = (char *)calloc(1, 1);
        return result;
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        result.output = (char *)calloc(1, 1);
        return result;
    }

    if (pid == 0) {
        /* Child: redirect stdout+stderr into the pipe, then exec the shell. */
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[0]);
        close(pipefd[1]);
        execl("/bin/sh", "sh", "-c", command, (char *)NULL);
        _exit(127); /* exec failed */
    }

    /* Parent: drain the read end into a growing buffer. */
    close(pipefd[1]);

    size_t cap = 4096;
    size_t len = 0;
    char  *buf = (char *)malloc(cap);
    if (buf) {
        for (;;) {
            if (len + 4096 + 1 > cap) {
                size_t new_cap = cap * 2;
                char  *grown   = (char *)realloc(buf, new_cap);
                if (!grown) {
                    break;
                }
                buf = grown;
                cap = new_cap;
            }
            ssize_t n = read(pipefd[0], buf + len, 4096);
            if (n > 0) {
                len += (size_t)n;
            } else {
                break;
            }
        }
    }
    close(pipefd[0]);

    int status = 0;
    waitpid(pid, &status, 0);

    if (!buf) {
        buf = (char *)calloc(1, 1);
        len = 0;
    } else {
        buf[len] = '\0';
    }

    result.output     = buf;
    result.output_len = len;
    result.exit_code  = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    return result;
}

void pi_subprocess_free(pi_subprocess_result *result) {
    if (result && result->output) {
        free(result->output);
        result->output     = NULL;
        result->output_len = 0;
    }
}

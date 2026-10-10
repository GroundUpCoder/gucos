/* Minimal libbb spawn helpers for the hand-built multicall binary.
 * The CLOEXEC pipe reports exec failure without pretending fork shares memory. */
#include "libbb.h"

pid_t FAST_FUNC spawn(char **argv)
{
    int status_pipe[2], child_errno = 0;
    pid_t pid;
    if (pipe(status_pipe) < 0) return -1;
    if (fcntl(status_pipe[1], F_SETFD, FD_CLOEXEC) < 0) {
        int saved = errno; close(status_pipe[0]); close(status_pipe[1]); errno = saved; return -1;
    }
    fflush_all();
    pid = fork();
    if (pid == 0) {
        close(status_pipe[0]);
        execvp(argv[0], argv);
        child_errno = errno;
        full_write(status_pipe[1], &child_errno, sizeof(child_errno));
        _exit(127);
    }
    close(status_pipe[1]);
    if (pid < 0) { int saved=errno;close(status_pipe[0]);errno=saved;return -1; }
    int count = full_read(status_pipe[0], &child_errno, sizeof(child_errno));
    close(status_pipe[0]);
    if (count > 0) {
        while (waitpid(pid, NULL, 0) < 0 && errno == EINTR) {}
        errno = child_errno; return -1;
    }
    return pid;
}

pid_t FAST_FUNC xspawn(char **argv)
{
    pid_t pid = spawn(argv);
    if (pid < 0) bb_simple_perror_msg_and_die(*argv);
    return pid;
}

int FAST_FUNC spawn_and_wait(char **argv) { return wait4pid(spawn(argv)); }

/* /bin/cc — the compiler command as an OS binary. There is no wasm image to exec
 * (the compiler is compiler.js, living in the kernel worker), so this is a
 * thin shim over the __compile_chunk syscall: ship argv+cwd to the kernel's
 * compile hook, then write the returned stdout/stderr to our OWN fds — so
 * redirection and pipes apply to compiler output like any other program's.
 * Default output is ./a.out, immediately spawnable (`cc hello.c && ./a.out`).
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* Two pages of up to 4096 UTF-16 units each, UTF-8 encoded by the host. */
static char buf[32780];
__import int __compile_chunk(const char *, char *const *, char *, int);

int main(int argc, char **argv) {
    (void)argc;
    char cwd[512];
    if (!getcwd(cwd, sizeof cwd)) strcpy(cwd, "/");
    int exit_code;
    do {
        int n = __compile_chunk(cwd, argv, buf, sizeof buf);
        if (n < 0) {
            perror("cc");
            return 1;
        }
        int out_len, err_len;
        memcpy(&exit_code, buf, 4);
        memcpy(&out_len, buf + 4, 4);
        memcpy(&err_len, buf + 8, 4);
        if (fwrite(buf + 12, 1, (size_t)out_len, stdout) != (size_t)out_len ||
            fwrite(buf + 12 + out_len, 1, (size_t)err_len, stderr) != (size_t)err_len) return 1;
        argv = 0;
    } while (exit_code < 0);
    return exit_code;
}

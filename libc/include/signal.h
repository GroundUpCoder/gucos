
#pragma once
__require_source("__signal.c");
typedef int sig_atomic_t;
typedef void (*__sighandler_t)(int);
typedef __sighandler_t sighandler_t;
typedef __sighandler_t sig_t;
#define SIG_DFL ((__sighandler_t)0)
#define SIG_IGN ((__sighandler_t)1)
#define SIG_ERR ((__sighandler_t)-1)
#define SIGABRT 6
#define SIGFPE  8
#define SIGILL  4
#define SIGINT  2
#define SIGSEGV 11
#define SIGTERM 15
#define SIGHUP  1
#define SIGQUIT 3
#define SIGTRAP 5
#define SIGKILL 9
#define SIGBUS  7
#define SIGSYS  31
#define SIGPIPE 13
#define SIGALRM 14
#define SIGURG  23
#define SIGSTOP 19
#define SIGTSTP 20
#define SIGCONT 18
#define SIGCHLD 17
#define SIGTTIN 21
#define SIGTTOU 22
#define SIGUSR1 10
#define SIGUSR2 12
#define SIGIO   29
#define SIGPROF 27
#define SIGWINCH 28
#define SIGVTALRM 26
#define SIGXCPU 24
#define SIGXFSZ 25
#define NSIG 32

/* sigprocmask 'how' */
#define SIG_BLOCK   0
#define SIG_UNBLOCK 1
#define SIG_SETMASK 2

/* sa_flags */
#define SA_NOCLDSTOP 0x00000001
#define SA_NOCLDWAIT 0x00000002
#define SA_SIGINFO   0x00000004
#define SA_ONSTACK   0x08000000
#define SA_RESTART   0x10000000
#define SA_NODEFER   0x40000000
#define SA_RESETHAND 0x80000000

typedef unsigned long long sigset_t;

union sigval { int sival_int; void *sival_ptr; };
typedef struct {
  int si_signo, si_code, si_errno, si_pid, si_uid, si_status;
  void *si_addr;
  union sigval si_value;
} siginfo_t;

struct sigaction {
  __sighandler_t sa_handler;
  void (*sa_sigaction)(int, siginfo_t *, void *);
  sigset_t sa_mask;
  int sa_flags;
  void (*sa_restorer)(void);
};

__sighandler_t signal(int __sig, __sighandler_t __handler);
int raise(int __sig);
int sigaction(int __sig, const struct sigaction *__act, struct sigaction *__old);
int sigemptyset(sigset_t *__set);
int sigfillset(sigset_t *__set);
int sigaddset(sigset_t *__set, int __sig);
int sigdelset(sigset_t *__set, int __sig);
int sigismember(const sigset_t *__set, int __sig);
int sigprocmask(int __how, const sigset_t *__set, sigset_t *__old);
int pthread_sigmask(int __how, const sigset_t *__set, sigset_t *__old);
int sigpending(sigset_t *__set);
int pause(void);
int sigsuspend(const sigset_t *__mask);
int kill(int __pid, int __sig);
int killpg(int __pgrp, int __sig);

/* Notify the runtime of a disposition change (kind: 0=DFL 1=IGN 2=HANDLER) so
   the process kernel's kill() applies the right action. Host-provided. */
__import void __on_sigdisp(int __sig, int __kind);
/* Publish the blocked mask to the kernel page (low 32 bits; NSIG is 32) and
   let the host deliver any kernel-pending signal that just became
   unblocked. No-op without a kernel. Host-provided. */
__import void __on_sigmask(unsigned __mask);
/* Park on the kernel doorbell until a signal has been DELIVERED (the host
   runs the handlers before this returns). -1/ENOSYS without a kernel —
   never hangs. Backs pause()/sigsuspend(). Host-provided. */
__import int __sig_pause(void);
  
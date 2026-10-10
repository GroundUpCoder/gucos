
#include <signal.h>
#include <errno.h>
#include <stddef.h>
#include <unistd.h>   /* getpid */
#include <sys/time.h> /* struct itimerval / ITIMER_REAL (docs/archive/0044) */
__import void __exit(int status);

/* Per-process disposition state (the libc owns it — sigaction set it here).
   A running wasm instance can't be preempted, so delivery happens at SAFE
   POINTS: raise()/abort() deliver synchronously, and with a kernel attached
   (kernel.js) the host claims kernel-posted signals at every syscall
   boundary and calls the exported __sig_dispatch (docs/KERNEL.md Phase 2).
   The runtime is told the disposition KIND via __on_sigdisp so kill()
   applies the right action, and the blocked mask via __on_sigmask so the
   kernel parks blocked signals as pending. Pure-compute loops never reach a
   safe point — settled caveat, SIGKILL still works. */
#include <__thread.h>
struct __signal_state {
  __sighandler_t handlers[NSIG];
  void (*actions[NSIG])(int, siginfo_t *, void *);
  int flags[NSIG];
};
static struct __signal_state __main_signals;
static struct __signal_state *__signal_state(void) {
  unsigned ptr = __THREAD_FIELD(__builtin(thread_record), __THREAD_PROCESS_SIGNALS);
  return ptr ? (struct __signal_state *)ptr : &__main_signals;
}
#define __sig_h (__signal_state()->handlers)
#define __sig_a (__signal_state()->actions)
#define __sig_fl (__signal_state()->flags)
void __process_clone_signals(void) {
  unsigned rec = __builtin(thread_record);
  struct __signal_state *state = (struct __signal_state *)(rec + 2144);
  *state = *__signal_state();
  __THREAD_FIELD(rec, __THREAD_PROCESS_SIGNALS) = (unsigned)state;
}
__export __process_clone_signals = __process_clone_signals;
#define __sig_blocked __THREAD_FIELD(__builtin(thread_record), __THREAD_SIGBLOCK)
#define __sig_pending __THREAD_FIELD(__builtin(thread_record), __THREAD_SIGPENDING)

/* Runtime image transitions: no callbacks or imports while restoring state. */
void __process_signals(unsigned blocked, unsigned ignored, int child) {
  __sig_pending = 0;
  if (child) return; /* fork retains copied dispositions and blocked mask */
  __sig_blocked = blocked;
  for (int s = 1; s < NSIG; s++) {
    __sig_h[s] = (ignored & (1u << (s-1))) ? SIG_IGN : SIG_DFL;
    __sig_a[s] = 0; __sig_fl[s] = 0;
  }
}

__export __process_signals = __process_signals;

static int __sig_ok(int s) { return s > 0 && s < NSIG; }
static int __sig_uncatchable(int s) { return s == SIGKILL || s == SIGSTOP; }

/* Default action: 0=terminate 1=ignore 2=stop 3=continue. (if-chains, not a
   switch, so this doesn't perturb the compiler's br_table-strategy golden when
   __signal.c links into every stdlib-using program via abort.) */
static int __sig_default_action(int s) {
  if (s == SIGCHLD || s == SIGURG || s == SIGWINCH) return 1;
  if (s == SIGCONT) return 3;
  if (s == SIGSTOP || s == SIGTSTP || s == SIGTTIN || s == SIGTTOU) return 2;
  return 0;
}

/* Disposition kind for the runtime mirror: 0=DFL 1=IGN 2=HANDLER. */
static int __sig_kind(int s) {
  if (__sig_a[s]) return 2;
  if (__sig_h[s] == SIG_DFL) return 0;
  if (__sig_h[s] == SIG_IGN) return 1;
  return 2;
}

/* Counts user-visible deliveries (handler invocations). sigsuspend() uses it
   to detect that unblocking already delivered — POSIX says it must return
   then, not park for a second signal. */
static volatile int __sig_ncalls;

/* Deliver one signal to this process's tables right now. Async deliveries
   (host safe-point dispatch, unblock drains, kill(getpid(), sig)) park
   blocked signals in __sig_pending; raise() keeps its historical synchronous
   semantics and does not consult the mask (C11 raise; matches the existing
   goldens). Returns nonzero when an interrupted primitive may transparently
   restart: SA_RESTART on the action that ran, or nothing user-visible ran. */
static int __sig_deliver(int sig, int async) {
  if (async && (__sig_blocked & ((sigset_t)1 << (sig - 1)))) {
    __sig_pending |= (sigset_t)1 << (sig - 1);
    return 1;
  }
  if (__sig_a[sig]) {
    siginfo_t info; info.si_signo = sig; info.si_code = 0; info.si_errno = 0;
    info.si_pid = getpid(); info.si_uid = 0; info.si_status = 0; info.si_addr = NULL;
    info.si_value.sival_int = 0;
    void (*a)(int, siginfo_t *, void *) = __sig_a[sig];
    int restart = (__sig_fl[sig] & SA_RESTART) != 0;   /* before the handler can sigaction() */
    if (__sig_fl[sig] & SA_RESETHAND) { __sig_a[sig] = NULL; __on_sigdisp(sig, 0); }
    __sig_ncalls++;
    a(sig, &info, NULL);
    return restart;
  }
  __sighandler_t h = __sig_h[sig];
  if (h == SIG_IGN) return 1;
  if (h == SIG_DFL) {
    if (__sig_default_action(sig) != 0) return 1;   /* ignore; stop/cont → docs/archive/0003 */
    /* Terminate: prefer the kernel — kill-self makes the termsig round-trip
       to the parent as WIFSIGNALED. Without a kernel (__spawn_kill ENOSYS
       and returns), approximate with the classic 128+sig exit. */
    __spawn_kill(getpid(), sig);
    __exit(128 + sig);
  }
  int restart = (__sig_fl[sig] & SA_RESTART) != 0;
  if (__sig_fl[sig] & SA_RESETHAND) { __sig_h[sig] = SIG_DFL; __on_sigdisp(sig, 0); }
  __sig_ncalls++;
  h(sig);
  return restart;
}

/* The mask changed: publish it to the kernel page (the host delivers any
   kernel-pending signal that became claimable inside __on_sigmask), then
   drain locally-parked pending signals that are now unblocked. */
static void __sig_mask_changed(void) {
  __on_sigmask((unsigned)__sig_blocked);
  sigset_t ready;
  while ((ready = (__sig_pending & ~__sig_blocked)) != 0) {
    for (int s = 1; s < NSIG; s++) {
      sigset_t b = (sigset_t)1 << (s - 1);
      if (ready & b) { __sig_pending &= ~b; __sig_deliver(s, 1); }
    }
  }
}

/* Host-called safe-point dispatch: kernel.js posted sig on our kernel page
   and host.js claimed it. Returns the may-restart verdict (see
   __sig_deliver) so interrupted waits can honor SA_RESTART. */
int __sig_dispatch(int sig) {
  if (!__sig_ok(sig)) return 1;
  return __sig_deliver(sig, 1);
}
__export __sig_dispatch = __sig_dispatch;

int sigemptyset(sigset_t *set) { if (set) *set = 0; return 0; }
int sigfillset(sigset_t *set) { if (set) *set = ~(sigset_t)0; return 0; }
int sigaddset(sigset_t *set, int sig) {
  if (!__sig_ok(sig)) { errno = EINVAL; return -1; }
  if (set) *set |= (sigset_t)1 << (sig - 1);
  return 0;
}
int sigdelset(sigset_t *set, int sig) {
  if (!__sig_ok(sig)) { errno = EINVAL; return -1; }
  if (set) *set &= ~((sigset_t)1 << (sig - 1));
  return 0;
}
int sigismember(const sigset_t *set, int sig) {
  if (!__sig_ok(sig)) { errno = EINVAL; return -1; }
  return set ? (int)((*set >> (sig - 1)) & 1) : 0;
}

int sigprocmask(int how, const sigset_t *set, sigset_t *oldset) {
  if (oldset) *oldset = __sig_blocked;
  if (set) {
    if (how == SIG_BLOCK) __sig_blocked |= *set;
    else if (how == SIG_UNBLOCK) __sig_blocked &= ~*set;
    else if (how == SIG_SETMASK) __sig_blocked = *set;
    else { errno = EINVAL; return -1; }
    /* SIGKILL/SIGSTOP can never be blocked. */
    __sig_blocked &= ~(((sigset_t)1 << (SIGKILL - 1)) | ((sigset_t)1 << (SIGSTOP - 1)));
    __sig_mask_changed();
  }
  return 0;
}

int pthread_sigmask(int how, const sigset_t *set, sigset_t *oldset) {
  int saved = errno;
  int result = sigprocmask(how, set, oldset) < 0 ? errno : 0;
  errno = saved;
  return result;
}

int sigpending(sigset_t *set) { if (set) *set = __sig_pending; return 0; }

__sighandler_t signal(int sig, __sighandler_t handler) {
  if (!__sig_ok(sig) || __sig_uncatchable(sig)) { errno = EINVAL; return SIG_ERR; }
  __sighandler_t old = __sig_a[sig] ? (__sighandler_t)0 : __sig_h[sig];
  __sig_h[sig] = handler;
  __sig_a[sig] = NULL;
  __sig_fl[sig] = 0;
  __on_sigdisp(sig, __sig_kind(sig));
  return old;
}

int sigaction(int sig, const struct sigaction *act, struct sigaction *oldact) {
  if (!__sig_ok(sig)) { errno = EINVAL; return -1; }
  if (oldact) {
    oldact->sa_handler = __sig_h[sig];
    oldact->sa_sigaction = __sig_a[sig];
    oldact->sa_mask = 0;
    oldact->sa_flags = __sig_fl[sig];
    oldact->sa_restorer = NULL;
  }
  if (act) {
    if (__sig_uncatchable(sig)) { errno = EINVAL; return -1; }
    __sig_fl[sig] = act->sa_flags;
    if (act->sa_flags & SA_SIGINFO) { __sig_a[sig] = act->sa_sigaction; __sig_h[sig] = SIG_DFL; }
    else { __sig_h[sig] = act->sa_handler; __sig_a[sig] = NULL; }
    __on_sigdisp(sig, __sig_kind(sig));
  }
  return 0;
}

/* raise(): synchronous self-delivery (C11). The handler runs right here; a
   default-terminate disposition exits (via kill-self when a kernel exists,
   so the parent sees WIFSIGNALED). Historically mask-blind — kept. */
int raise(int sig) {
  if (!__sig_ok(sig)) { errno = EINVAL; return -1; }
  __sig_deliver(sig, 0);
  return 0;
}

/* pause()/sigsuspend(): park on the kernel doorbell; the host delivers the
   waking signal's handlers inside __sig_pause, so these return EINTR right
   after. Without a kernel __sig_pause reports ENOSYS (never hangs). */
int pause(void) {
  if (__sig_pause() < 0) return -1;   /* errno set by the host */
  errno = EINTR;
  return -1;
}
int sigsuspend(const sigset_t *mask) {
  sigset_t old = __sig_blocked;
  int before = __sig_ncalls;
  __sig_blocked = (mask ? *mask : 0)
    & ~(((sigset_t)1 << (SIGKILL - 1)) | ((sigset_t)1 << (SIGSTOP - 1)));
  __sig_mask_changed();               /* may deliver a newly-unblocked pending signal */
  int r = 0;
  if (__sig_ncalls == before) r = __sig_pause();
  __sig_blocked = old;
  __sig_mask_changed();
  if (r < 0) return -1;
  errno = EINTR;
  return -1;
}

/* kill(): route through the process kernel (kernel.js). Self-directed
   signals deliver locally with async semantics (blocked → pending, handler
   runs before return otherwise) — the kernel round-trip couldn't reach this
   very instance any sooner than we can. */
int kill(int pid, int sig) {
  /* sig 0 is the POSIX existence probe (kill(2)): route + error-check,
     deliver nothing. Only kill() takes it — signal()/sigaction()/raise()
     keep rejecting 0 via __sig_ok. */
  if (sig != 0 && !__sig_ok(sig)) { errno = EINVAL; return -1; }
  if (pid == getpid()) { if (sig != 0) __sig_deliver(sig, 1); return 0; }
  return __spawn_kill(pid, sig);
}
int killpg(int pgrp, int sig) {
  if (pgrp < 0) { errno = EINVAL; return -1; }
  return kill(pgrp == 0 ? 0 : -pgrp, sig);
}

/* ---- interval timers (docs/archive/0044): ITIMER_REAL -> SIGALRM ----
   The kernel owns ONE real-time timer per process (kernel.js; VIRTUAL/PROF
   answer EINVAL — no CPU accounting). The wire ABI is milliseconds; these
   wrappers own the timeval <-> ms conversion. out2/old2 = {value_ms,
   interval_ms} of the PREVIOUS (setitimer) or CURRENT (getitimer) timer. */
__import int __setitimer(int which, unsigned value_ms, unsigned interval_ms, unsigned *old2);
__import int __getitimer(int which, unsigned *out2);

/* timeval -> ms, clamped to INT_MAX ms (~24.8 days — the embedder's timer
   ceiling); nonzero sub-ms rounds UP so an armed value never converts to
   "disarmed". Caller has already range-checked the fields. */
static unsigned __itimer_tv2ms(const struct timeval *tv) {
  if (tv->tv_sec > 2147483) return 2147483647u;
  long long ms = tv->tv_sec * 1000LL + ((long long)tv->tv_usec + 999) / 1000;
  return ms > 2147483647LL ? 2147483647u : (unsigned)ms;
}
static void __itimer_ms2tv(unsigned ms, struct timeval *tv) {
  tv->tv_sec = ms / 1000u;
  tv->tv_usec = (long)(ms % 1000u) * 1000;
}

int setitimer(int which, const struct itimerval *nv, struct itimerval *ov) {
  if (!nv || nv->it_value.tv_sec < 0 || nv->it_interval.tv_sec < 0 ||
      nv->it_value.tv_usec < 0 || nv->it_value.tv_usec > 999999 ||
      nv->it_interval.tv_usec < 0 || nv->it_interval.tv_usec > 999999) {
    errno = EINVAL;
    return -1;
  }
  unsigned old[2];
  if (__setitimer(which, __itimer_tv2ms(&nv->it_value),
                  __itimer_tv2ms(&nv->it_interval), old) < 0) return -1;
  if (ov) {
    __itimer_ms2tv(old[0], &ov->it_value);
    __itimer_ms2tv(old[1], &ov->it_interval);
  }
  return 0;
}

int getitimer(int which, struct itimerval *cur) {
  if (!cur) { errno = EINVAL; return -1; }
  unsigned v[2];
  if (__getitimer(which, v) < 0) return -1;
  __itimer_ms2tv(v[0], &cur->it_value);
  __itimer_ms2tv(v[1], &cur->it_interval);
  return 0;
}

/* POSIX alarm() cannot report failure; without a kernel (__setitimer
   ENOSYS) it returns 0 and the timer simply never fires — consistent with
   the rest of the spawn/signal surface failing loud only where an error
   return exists. Seconds remaining round UP (a still-armed alarm never
   reports 0). */
unsigned alarm(unsigned seconds) {
  unsigned ms = seconds > 2147483u ? 2147483647u : seconds * 1000u;
  unsigned old[2];
  if (__setitimer(ITIMER_REAL, ms, 0, old) < 0) return 0;
  return (old[0] + 999) / 1000;
}

unsigned ualarm(unsigned usecs, unsigned interval) {
  unsigned old[2];
  /* div-then-round (not +999 first): usecs near UINT_MAX must not wrap to
     a tiny value and silently disarm. */
  if (__setitimer(ITIMER_REAL, usecs / 1000 + (usecs % 1000 != 0),
                  interval / 1000 + (interval % 1000 != 0), old) < 0) return 0;
  return old[0] > 4294967u ? 4294967295u : old[0] * 1000u;
}
  
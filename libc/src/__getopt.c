
#include <getopt.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

char *optarg = (void *)0;
int optind = 1;
int opterr = 1;
int optopt = 0;

// Position within the current argv[optind] (0 means we haven't started
// scanning the current arg, so the next call will look at optind fresh).
static int __optpos = 0;

// Skip GNU-mode prefix flags ('+'/'-') and detect the silent ':' flag.
static const char *__skip_modifiers(const char *s, int *silent) {
  if (*s == '+' || *s == '-') s++;
  if (*s == ':') { *silent = 1; s++; }
  return s;
}

// Find character c in optstring (after modifiers stripped).
// Returns pointer to the matching char, or NULL if not found.
static const char *__find_short(int c, const char *optstring) {
  if (c == ':' || c == '?' || c == '\0') return (void *)0;
  for (const char *p = optstring; *p; p++) {
    if (*p == c) return p;
  }
  return (void *)0;
}

// Strip leading directory components from argv[0] for diagnostic output.
static const char *__progname(int argc, char *const argv[]) {
  if (argc <= 0 || !argv[0]) return "?";
  const char *s = argv[0];
  const char *base = s;
  while (*s) {
    if (*s == '/') base = s + 1;
    s++;
  }
  return base;
}

int getopt(int argc, char *const argv[], const char *optstring) {
  // POSIX/glibc convention: optind == 0 requests a reset of internal state.
  if (optind == 0) { optind = 1; __optpos = 0; }

  optarg = (void *)0;
  int silent = 0;
  const char *opts = __skip_modifiers(optstring, &silent);

  if (__optpos == 0) {
    if (optind >= argc) return -1;
    const char *arg = argv[optind];
    if (!arg || arg[0] != '-' || arg[1] == 0) return -1;
    if (arg[1] == '-' && arg[2] == 0) {
      // "--" terminator: consume and stop option processing
      optind++;
      return -1;
    }
    __optpos = 1;  // skip the leading '-'
  }

  const char *arg = argv[optind];
  int c = (unsigned char)arg[__optpos];
  __optpos++;
  int reached_end = (arg[__optpos] == 0);

  const char *match = __find_short(c, opts);

  if (!match) {
    if (reached_end) { optind++; __optpos = 0; }
    if (opterr && !silent) {
      fprintf(stderr, "%s: invalid option -- '%c'\n", __progname(argc, argv), c);
    }
    optopt = c;
    return '?';
  }

  if (match[1] != ':') {
    // No argument
    if (reached_end) { optind++; __optpos = 0; }
    return c;
  }

  // Has an argument (required if "x:", optional if "x::")
  int optional = (match[2] == ':');

  if (!reached_end) {
    optarg = (char *)&arg[__optpos];
    optind++;
    __optpos = 0;
    return c;
  }

  // Argument is in the next argv element (or missing)
  __optpos = 0;
  optind++;

  if (optional) return c;  // optional and not given

  if (optind >= argc) {
    if (opterr && !silent) {
      fprintf(stderr, "%s: option requires an argument -- '%c'\n", __progname(argc, argv), c);
    }
    optopt = c;
    return silent ? ':' : '?';
  }

  optarg = argv[optind++];
  return c;
}

// --- Long-option core (shared by getopt_long / getopt_long_only) -------

static int __getopt_long_core(int argc, char *const argv[], const char *optstring,
                              const struct option *longopts, int *longindex,
                              int allow_dash) {
  if (optind == 0) { optind = 1; __optpos = 0; }

  // Mid-arg short-option chain: continue scanning short.
  if (__optpos != 0) {
    return getopt(argc, argv, optstring);
  }

  if (optind >= argc) return -1;
  const char *arg = argv[optind];
  if (!arg || arg[0] != '-' || arg[1] == 0) return -1;

  // "--" terminator
  if (arg[1] == '-' && arg[2] == 0) {
    optind++;
    return -1;
  }

  int silent = 0;
  __skip_modifiers(optstring, &silent);

  // Determine where the long-option name starts.
  // "--name..." → always treated as long.
  // "-name..."  → treated as long only in long_only mode.
  const char *name;
  int double_dash = (arg[1] == '-');
  if (double_dash) {
    name = arg + 2;
  } else if (allow_dash) {
    name = arg + 1;
  } else {
    return getopt(argc, argv, optstring);
  }

  // Split out "=value" if present.
  const char *eq = strchr(name, '=');
  size_t namelen = eq ? (size_t)(eq - name) : strlen(name);

  // Match against longopts: prefer exact match, else unambiguous prefix.
  int match_idx = -1;
  int exact = 0;
  int ambig = 0;
  for (int i = 0; longopts[i].name; i++) {
    if (strncmp(longopts[i].name, name, namelen) == 0) {
      if (strlen(longopts[i].name) == namelen) {
        match_idx = i;
        exact = 1;
        ambig = 0;
        break;
      }
      if (match_idx == -1) match_idx = i;
      else ambig = 1;
    }
  }

  // In long-only mode, if no exact long match, fall back to short option
  // processing (so "-h" with optstring "h" works even if there's no long
  // option named "h").
  if (allow_dash && !double_dash && !exact) {
    return getopt(argc, argv, optstring);
  }

  if (ambig) {
    if (opterr && !silent) {
      fprintf(stderr, "%s: option '%s%.*s' is ambiguous\n",
              __progname(argc, argv), double_dash ? "--" : "-", (int)namelen, name);
    }
    optind++;
    optopt = 0;
    return '?';
  }

  if (match_idx == -1) {
    if (opterr && !silent) {
      fprintf(stderr, "%s: unrecognized option '%s%s'\n",
              __progname(argc, argv), double_dash ? "--" : "-", name);
    }
    optind++;
    optopt = 0;
    return '?';
  }

  const struct option *lo = &longopts[match_idx];
  if (longindex) *longindex = match_idx;

  optarg = (void *)0;

  if (lo->has_arg == required_argument || lo->has_arg == optional_argument) {
    if (eq) {
      optarg = (char *)(eq + 1);
      optind++;
    } else if (lo->has_arg == required_argument) {
      optind++;
      if (optind >= argc) {
        if (opterr && !silent) {
          fprintf(stderr, "%s: option '--%s' requires an argument\n",
                  __progname(argc, argv), lo->name);
        }
        optopt = lo->val;
        return silent ? ':' : '?';
      }
      optarg = argv[optind++];
    } else {
      // optional_argument and not given
      optind++;
    }
  } else {
    // no_argument
    if (eq) {
      if (opterr && !silent) {
        fprintf(stderr, "%s: option '--%s' doesn't allow an argument\n",
                __progname(argc, argv), lo->name);
      }
      optind++;
      optopt = lo->val;
      return '?';
    }
    optind++;
  }

  if (lo->flag) {
    *lo->flag = lo->val;
    return 0;
  }
  return lo->val;
}

int getopt_long(int argc, char *const argv[], const char *optstring,
                const struct option *longopts, int *longindex) {
  return __getopt_long_core(argc, argv, optstring, longopts, longindex, 0);
}

int getopt_long_only(int argc, char *const argv[], const char *optstring,
                     const struct option *longopts, int *longindex) {
  return __getopt_long_core(argc, argv, optstring, longopts, longindex, 1);
}
  
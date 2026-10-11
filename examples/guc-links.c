/* Bulk package link publication. gucman retains the publication lock and
 * atomic current-release switch. This replaces per-file shell subprocesses,
 * preserving the rule that unmanaged paths and other packages are untouched. */
#define _GNU_SOURCE 1
#define _DARWIN_C_SOURCE 1
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

struct output { char *name; mode_t mode; };
static int output_compare(const void *a,const void *b) {
  return strcmp(((const struct output *)a)->name,((const struct output *)b)->name);
}

/* The guest owner performs the existence check and rename in one operation.
 * Native probes use their platform's atomic exclusive-rename interface. */
#ifdef __wasm_posix__
__import int __rename_noreplace(const char *, const char *);
#else
static int __rename_noreplace(const char *from, const char *to) {
#ifdef __APPLE__
  return renameatx_np(AT_FDCWD, from, AT_FDCWD, to, RENAME_EXCL);
#elif defined(__linux__)
  return renameat2(AT_FDCWD, from, AT_FDCWD, to, RENAME_NOREPLACE);
#else
  (void)from; (void)to; errno = ENOSYS; return -1;
#endif
}
#endif

static int problem(const char *message, const char *path) {
  fprintf(stderr, "gucman: %s: %s\n", message, path);
  return 1;
}
static int relative_name(const char *name) {
  if (!*name || *name == '/')
    return 0;
  const char *part = name;
  for (;;) {
    const char *end = strchr(part, '/');
    size_t length = end ? (size_t)(end - part) : strlen(part);
    if (!length || (length == 1 && part[0] == '.') ||
        (length == 2 && part[0] == '.' && part[1] == '.'))
      return 0;
    if (!end)
      return 1;
    part = end + 1;
  }
}
static int directory(const char *path, int create) {
  struct stat st;
  if (!lstat(path, &st))
    return S_ISDIR(st.st_mode)
               ? 0
               : problem("invalid installation directory", path);
  if (errno != ENOENT)
    return problem("cannot inspect installation directory", path);
  if (create && mkdir(path, 0755))
    return problem("cannot create installation directory", path);
  return 0;
}
static int parents(char *path, size_t prefix_length, int create) {
  /* The installation prefix already exists. Never follow symlink parents. */
  for (char *p = path + prefix_length; *p; p++)
    if (*p == '/') {
      *p = 0;
      int result = directory(path, create);
      *p = '/';
      if (result)
        return result;
    }
  return 0;
}
/* A failed copy leaves an unpublished release, never changes public links.
 * Revalidate path/mode/ownership boundaries even though the shell preflights.
 * No shell processes per output; checksums still use the independent verifier. */
static int copy_outputs(const char *source,const char *release,const char *list,const char *package) {
  struct stat st;
  if(lstat(source,&st)||!S_ISDIR(st.st_mode)||lstat(release,&st)||!S_ISDIR(st.st_mode))
    return problem("copy roots must be real directories",release);
  FILE *f=fopen(list,"r");if(!f)return problem("cannot read outputs",list);
  if(fstat(fileno(f),&st)||!S_ISREG(st.st_mode)||st.st_size>64*1024*1024){fclose(f);return problem("invalid output list",list);}
  struct output *items=NULL;size_t count=0,capacity=0,total=0;int result=1;
  char line[PATH_MAX+16];
  for(;;) {
    size_t length=0;int c;
    while((c=fgetc(f))!=EOF){if(!c||length==sizeof line-1)goto done;line[length++]=(char)c;if(c=='\n')break;}
    if(!length)break;line[length]=0;
    if(count>=100000)goto done;
    if(length&&line[length-1]=='\n')line[--length]=0;
    char *tab=strchr(line,'\t');if(!tab)goto done;*tab++=0;
    mode_t mode;if(!strcmp(tab,"420"))mode=0644;else if(!strcmp(tab,"493"))mode=0755;else goto done;
    if(!relative_name(line))goto done;
    for(char *p=line;*p;p++)if(!strchr("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_./+-",*p))goto done;
    if(strncmp(line,"bin/",4)&&strncmp(line,"sbin/",5)&&strncmp(line,"include/",8)&&
       strncmp(line,"share/",6)&&strncmp(line,"libexec/",8)&&strncmp(line,"lib/",4))goto done;
    if(!strcmp(line,"lib/gucman")||!strncmp(line,"lib/gucman/",11)||
       !strcmp(line,"lib/wasm-posix")||!strncmp(line,"lib/wasm-posix/",15))goto done;
    total+=strlen(line)+1;if(total>64*1024*1024)goto done;
    if(count==capacity){size_t next=capacity?capacity*2:64;void *p=realloc(items,next*sizeof *items);if(!p)goto done;items=p;capacity=next;}
    items[count].name=strdup(line);if(!items[count].name)goto done;items[count++].mode=mode;
  }
  if(ferror(f)||!count)goto done;
  qsort(items,count,sizeof *items,output_compare);
  for(size_t i=0;i<count;i++) {
    if(i&&!strcmp(items[i-1].name,items[i].name))goto done;
    char prefix[PATH_MAX];snprintf(prefix,sizeof prefix,"%s",items[i].name);
    for(char *p=prefix;*p;p++)if(*p=='/'){
      *p=0;struct output key={prefix,0};int conflict=bsearch(&key,items,count,sizeof *items,output_compare)!=NULL;*p='/';if(conflict)goto done;
    }
    char from[PATH_MAX],to[PATH_MAX];
    if(snprintf(from,sizeof from,"%s/%s",source,items[i].name)>=(int)sizeof from||
       snprintf(to,sizeof to,"%s/%s",release,items[i].name)>=(int)sizeof to)goto done;
    if(parents(from,strlen(source),0)||lstat(from,&st)||!S_ISREG(st.st_mode)||
       parents(to,strlen(release),0)||!lstat(to,&st)||errno!=ENOENT)goto done;
  }
  fprintf(stderr,"gucman: Installing %s: 0/%zu files\n",package,count);
  for(size_t i=0;i<count;i++) {
    char from[PATH_MAX],to[PATH_MAX];snprintf(from,sizeof from,"%s/%s",source,items[i].name);snprintf(to,sizeof to,"%s/%s",release,items[i].name);
    if(parents(from,strlen(source),0)||parents(to,strlen(release),1))goto done;
    int input=open(from,O_RDONLY|O_NOFOLLOW);if(input<0)goto done;
    if(fstat(input,&st)||!S_ISREG(st.st_mode)){close(input);goto done;}
    int output=open(to,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW,0600);if(output<0){close(input);goto done;}
    char buffer[65536];int failed=0;
    for(;;){
      ssize_t n=read(input,buffer,sizeof buffer);if(n<0){if(errno==EINTR)continue;failed=1;break;}if(!n)break;
      ssize_t offset=0;while(offset<n){ssize_t wrote=write(output,buffer+offset,(size_t)(n-offset));if(wrote<0&&errno==EINTR)continue;if(wrote<=0){failed=1;break;}offset+=wrote;}if(failed)break;
    }
    if(!failed&&fchmod(output,items[i].mode))failed=1;
    if(close(input))failed=1;if(close(output))failed=1;
    if(failed){unlink(to);goto done;}
    if((i+1)%25==0||i+1==count)fprintf(stderr,"gucman: Installing %s: %zu/%zu files\n",package,i+1,count);
  }
  result=0;
done:
  if(result)problem("invalid outputs or failed staged copy",list);
  fclose(f);for(size_t i=0;i<count;i++)free(items[i].name);free(items);return result;
}
static int owned(const char *path, const char *target, int *exists) {
  struct stat st;
  *exists = 0;
  if (lstat(path, &st))
    return errno == ENOENT ? 0
                           : problem("cannot inspect installation path", path);
  *exists = 1;
  if (!S_ISLNK(st.st_mode))
    return problem("refusing to replace unmanaged path", path);
  char previous[PATH_MAX];
  ssize_t n = readlink(path, previous, sizeof previous - 1);
  if (n < 0 || n == (ssize_t)sizeof previous - 1)
    return problem("cannot inspect installation link", path);
  previous[n] = 0;
  return strcmp(previous, target)
             ? problem("file conflict or unmanaged symlink", path)
             : 0;
}
/* Validate the complete declared closure before any recipe executes. Checking
 * sorted prefixes in C avoids quadratic shell/IPC work for header-heavy ports. */
static int path_compare(const void *a, const void *b) {
  return strcmp(*(char *const *)a, *(char *const *)b);
}
static int closure(int count, char **lists) {
  char **paths = NULL; size_t used = 0, capacity = 0, bytes = 0;
  for (int i=0;i<count;i++) {
    FILE *file=fopen(lists[i],"r");
    if(!file)return problem("cannot open closure file",lists[i]);
    char name[PATH_MAX];
    while(fgets(name,sizeof name,file)) {
      size_t n=strlen(name);
      if(!n || name[n-1]!='\n')return problem("invalid closure path",name);
      name[--n]=0;
      if(!relative_name(name))return problem("invalid closure path",name);
      if(used>=100000 || (bytes+=n+1)>64*1024*1024)return problem("closure exceeds bound",name);
      if(used==capacity) {
        capacity=capacity?capacity*2:256;
        char **next=realloc(paths,capacity*sizeof(*paths));
        if(!next)return problem("out of memory",name);paths=next;
      }
      paths[used]=strdup(name);if(!paths[used])return problem("out of memory",name);used++;
    }
    if(ferror(file))return problem("cannot read closure file",lists[i]);
    fclose(file);
  }
  qsort(paths,used,sizeof(*paths),path_compare);
  for(size_t i=0;i<used;i++) {
    if(i && !strcmp(paths[i-1],paths[i]))return problem("dependency output ownership conflict",paths[i]);
    char prefix[PATH_MAX];strcpy(prefix,paths[i]);
    for(char *p=prefix;*p;p++)if(*p=='/') {
      *p=0;char *key=prefix;
      int found=bsearch(&key,paths,used,sizeof(*paths),path_compare)!=NULL;
      *p='/';if(found)return problem("dependency output ownership conflict",paths[i]);
    }
  }
  for(size_t i=0;i<used;i++)free(paths[i]);free(paths);return 0;
}
int main(int argc, char **argv) {
  if(argc==6&&!strcmp(argv[1],"copy"))return copy_outputs(argv[2],argv[3],argv[4],argv[5]);
  if(argc>=3 && !strcmp(argv[1],"closure"))return closure(argc-2,argv+2);
  if (argc == 4 && !strcmp(argv[1], "move-noreplace"))
    return __rename_noreplace(argv[2], argv[3]) ?
      problem("cannot move source without replacing destination", argv[3]) : 0;
  if (argc != 5 || (strcmp(argv[1], "check") && strcmp(argv[1], "link") && strcmp(argv[1], "unlink")) ||
      argv[2][0] != '/' || argv[3][0] != '/') {
    fputs("usage: guc-links check|link|unlink PREFIX PACKAGE_ROOT FILE_LIST\n",
          stderr);
    return 2;
  }
  int create = !strcmp(argv[1], "link"), result = 0;
  int remove = !strcmp(argv[1], "unlink");
  FILE *list = fopen(argv[4], "r");
  if (!list)
    return problem("cannot open package file list", argv[4]);
  char name[PATH_MAX];
  while (fgets(name, sizeof name, list)) {
    size_t length = strlen(name);
    if (length && name[length - 1] == '\n')
      name[--length] = 0;
    else if (!feof(list)) {
      result = problem("package path too long", name);
      break;
    }
    if (!relative_name(name)) {
      result = problem("invalid package file name", name);
      break;
    }
    char path[PATH_MAX], target[PATH_MAX];
    int exists;
    if (snprintf(path, sizeof path, "%s/%s", argv[2], name) >=
            (int)sizeof path ||
        snprintf(target, sizeof target, "%s/current/%s", argv[3], name) >=
            (int)sizeof target) {
      result = problem("package path too long", name);
      break;
    }
    if (parents(path, strlen(argv[2]), 0) || owned(path, target, &exists)) {
      result = 1;
      break;
    }
    if (create && !exists &&
        (parents(path, strlen(argv[2]), 1) || symlink(target, path))) {
      result = problem("cannot publish package link", path);
      break;
    }
    if(remove && exists && unlink(path)) {
      result=problem("cannot remove owned package link",path);break;
    }
  }
  if (ferror(list))
    result = problem("cannot read package file list", argv[4]);
  fclose(list);
  return result;
}

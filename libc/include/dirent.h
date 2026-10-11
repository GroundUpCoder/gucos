
#pragma once
#include <sys/types.h>
#include <stdlib.h>
__require_source("__dirent.c");

#define DT_UNKNOWN 0
#define DT_DIR     4
#define DT_REG     8
#define DT_LNK    10

/* NOTE: d_ino is always 0 (Node.js directory APIs don't expose inodes).
   Use stat() to get st_ino if needed. */
struct dirent {
  long           d_ino;
  int            d_type;
  char           d_name[256];
};

struct __DIR;
typedef struct __DIR DIR;

DIR *opendir(const char *name);
int closedir(DIR *dirp);
struct dirent *readdir(DIR *dirp);
void rewinddir(DIR *dirp);
#include <errno.h>
static inline int scandir(const char *path, struct dirent ***out,
                         int (*filter)(const struct dirent *),
                         int (*compare)(const struct dirent **, const struct dirent **)) {
 DIR *dir=opendir(path);if(!dir)return -1;
 struct dirent **list=0;int count=0,error=0;
 for(;;){errno=0;struct dirent *entry=readdir(dir);if(!entry){error=errno;break;}
  if(filter&&!filter(entry))continue;
  struct dirent *copy=malloc(sizeof(*copy));if(!copy){error=ENOMEM;break;}*copy=*entry;
  struct dirent **next=realloc(list,(count+1)*sizeof(*list));if(!next){free(copy);error=ENOMEM;break;}
  list=next;list[count++]=copy;
 }
 closedir(dir);
 if(error){for(int i=0;i<count;i++)free(list[i]);free(list);errno=error;return -1;}
 if(compare)for(int i=1;i<count;i++){struct dirent *entry=list[i];int j=i;while(j>0&&compare((const struct dirent **)&list[j-1],(const struct dirent **)&entry)>0){list[j]=list[j-1];j--;}list[j]=entry;}
 *out=list;return count;
}

  
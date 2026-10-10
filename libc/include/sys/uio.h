
#pragma once
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
struct iovec { void *iov_base; size_t iov_len; };
#define IOV_MAX 1024
static inline ssize_t __iov_io(int fd, const struct iovec *iov, int count, int writing) {
 if(count<0||count>IOV_MAX){errno=EINVAL;return -1;}
 size_t size=0;for(int i=0;i<count;i++){if(iov[i].iov_len>INT_MAX-size){errno=EINVAL;return -1;}size+=iov[i].iov_len;}
 if(!size)return 0;
 char *data=malloc(size);if(!data){errno=ENOMEM;return -1;}
 size_t offset=0;
 if(writing)for(int i=0;i<count;i++){memcpy(data+offset,iov[i].iov_base,iov[i].iov_len);offset+=iov[i].iov_len;}
 ssize_t result=writing?write(fd,data,size):read(fd,data,size);
 if(!writing&&result>0){offset=0;for(int i=0;i<count&&offset<(size_t)result;i++){size_t n=iov[i].iov_len;if(n>(size_t)result-offset)n=result-offset;memcpy(iov[i].iov_base,data+offset,n);offset+=n;}}
 int saved=errno;free(data);errno=saved;return result;
}
static inline ssize_t readv(int fd,const struct iovec *iov,int n){return __iov_io(fd,iov,n,0);}
static inline ssize_t writev(int fd,const struct iovec *iov,int n){return __iov_io(fd,iov,n,1);}
  
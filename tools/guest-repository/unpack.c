/* Extract ordinary ustar files/directories, safe relative symlinks, and hard
 * links to already extracted regular files into a fresh private directory.
 * No devices, forward hard links, extension headers or restored owners.
 * Fail closed before following any archive-created symlink parent. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>
static void die(const char *s){fprintf(stderr,"guc-unpack: %s\n",s);exit(1);}
static unsigned long number(const unsigned char *p,size_t n){
 unsigned long v=0;size_t i=0;while(i<n&&p[i]==' ')i++;int digits=0;
 for(;i<n&&p[i]>='0'&&p[i]<='7';i++){if(v>(ULONG_MAX-7)/8)die("numeric overflow");v=v*8+p[i]-'0';digits=1;}
 for(;i<n;i++)if(p[i]&&p[i]!=' ')die("invalid octal field");if(!digits)die("missing octal field");return v;
}
static void field(char *out,const unsigned char *p,size_t n){size_t i=0;while(i<n&&p[i]){out[i]=(char)p[i];i++;}out[i]=0;}
static int safe(const char *path){
 if(!*path||*path=='/')return 0;
 const char *part=path;
 for(const unsigned char *p=(const unsigned char *)path;*p;p++)if(*p<32||*p==127)return 0;
 for(;;){const char *slash=strchr(part,'/');size_t n=slash?(size_t)(slash-part):strlen(part);if(!n||(n==1&&part[0]=='.')||(n==2&&part[0]=='.'&&part[1]=='.'))return 0;if(!slash)return 1;part=slash+1;}
}
static void parents(char *path,int create){
 for(char *p=path;*p;p++)if(*p=='/'){*p=0;struct stat st;if(lstat(path,&st)){if(!create||errno!=ENOENT||mkdir(path,0755))die("cannot access parent");}else if(!S_ISDIR(st.st_mode))die("non-directory parent");*p='/';}
}
int main(int argc,char **argv){
 if(argc!=3)die("usage: guc-unpack ARCHIVE NEW_DIRECTORY");
 FILE *input=fopen(argv[1],"rb");if(!input)die("cannot open archive");
 if(mkdir(argv[2],0700)||chdir(argv[2]))die("destination must be new private directory");
 unsigned char header[512],buffer[65536];int zeros=0;
 while(fread(header,1,512,input)==512){
  int zero=1;for(int i=0;i<512;i++)if(header[i]){zero=0;break;}
  if(zero){zeros++;continue;}if(zeros)die("nonzero data after tar end");
  unsigned long sum=0;for(int i=0;i<512;i++)sum+=i>=148&&i<156?' ':header[i];
  if(sum!=number(header+148,8))die("header checksum mismatch");
  if(memcmp(header+257,"ustar",5))die("unsupported tar format");
  char base[101],prefix[156],path[258],target[101];field(base,header,100);field(prefix,header+345,155);field(target,header+157,100);
  if(*prefix)snprintf(path,sizeof path,"%s/%s",prefix,base);else snprintf(path,sizeof path,"%s",base);
  char *relative=path;if(!strncmp(relative,"./",2))relative+=2;
  size_t length=strlen(relative);char type=(char)header[156];
  if(type=='5'&&length&&relative[length-1]=='/')relative[--length]=0;
  unsigned long size=number(header+124,12),mode=number(header+100,8)&0777;
  if(type=='5'&&(!strcmp(relative,".")||!*relative)){if(size)die("directory has data");continue;}
  if(!safe(relative))die("unsafe archive path");parents(relative,1);
  struct stat st;int exists=!lstat(relative,&st);
  if(type=='5'){
   if(size)die("directory has data");if(exists){if(!S_ISDIR(st.st_mode))die("directory collision");}else if(mkdir(relative,0755))die("mkdir failed");continue;
  }
  if(exists||errno!=ENOENT)die("duplicate or inaccessible archive path");
  if(type=='2'){if(size||!safe(target))die("unsafe symlink");if(symlink(target,relative))die("symlink failed");continue;}
  if(type=='1'){
   char *source=target;if(!strncmp(source,"./",2))source+=2;
   if(size||!safe(source))die("unsafe hard link");parents(source,0);
   struct stat original;if(lstat(source,&original)||!S_ISREG(original.st_mode))die("hard link target must be an earlier regular file");
   if(link(source,relative))die("hard link failed");continue;
  }
  if(type!='0'&&type!=0)die("unsupported tar entry");
  FILE *output=fopen(relative,"wb");if(!output)die("cannot create output");
  unsigned long left=size;while(left){size_t n=left>sizeof buffer?sizeof buffer:(size_t)left;if(fread(buffer,1,n,input)!=n||fwrite(buffer,1,n,output)!=n)die("short archive or write failure");left-=n;}
  if(fclose(output)||chmod(relative,(mode_t)mode))die("output finalization failed");
  size_t padding=(size_t)((512-size%512)%512);if(padding&&fread(buffer,1,padding,input)!=padding)die("short padding");
 }
 if(ferror(input)||!feof(input)||zeros<2)die("incomplete tar end");
 // fread can return a partial final block: its bytes are not a valid tar end.
 long position=ftell(input);if(position<0||position%512)die("partial tar block");
 fclose(input);return 0;
}

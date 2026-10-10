/* Strict bounded JSON queries for guest repository metadata. Output is data,
 * never shell source. Control characters in extracted scalars are refused. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#define MAX_BYTES (8*1024*1024)
#define MAX_NODES 200000
struct node { char type; char *text; int child, next, count; };
static struct node *nodes;
static int used,capacity;
static const unsigned char *cursor,*end;
static void die(const char *s){fprintf(stderr,"guc-json: %s\n",s);exit(1);}
static void *allocate(size_t n){void *p=malloc(n);if(!p)die("out of memory");return p;}
static void space(void){while(cursor<end&&(*cursor==' '||*cursor=='\t'||*cursor=='\r'||*cursor=='\n'))cursor++;}
static int hex(unsigned c){if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;if(c>='A'&&c<='F')return c-'A'+10;die("invalid unicode escape");return 0;}
static unsigned unicode(void){if(end-cursor<4)die("short unicode escape");unsigned v=0;for(int i=0;i<4;i++)v=v*16+hex(*cursor++);return v;}
static char *string(void){
 if(cursor==end||*cursor++!='"')die("expected string");
 const unsigned char *start=cursor,*scan=cursor;int escaped=0;
 while(scan<end){unsigned c=*scan++;if(c<32)die("string control character");if(!escaped&&c=='"')break;if(!escaped&&c=='\\')escaped=1;else escaped=0;}
 if(scan==start||scan[-1]!='"'||escaped)die("unterminated string");
 char *out=allocate((size_t)(scan-start)+1),*p=out;
 while(cursor<scan-1){unsigned c=*cursor++;
  if(c=='\\'){
   if(cursor>=scan-1)die("short escape");c=*cursor++;
   if(c=='"'||c=='\\'||c=='/')*p++=(char)c;
   else if(c=='b')*p++='\b';else if(c=='f')*p++='\f';else if(c=='n')*p++='\n';else if(c=='r')*p++='\r';else if(c=='t')*p++='\t';
   else if(c=='u'){
    unsigned v=unicode();if(v>=0xd800&&v<=0xdbff){if(end-cursor<2||cursor[0]!='\\'||cursor[1]!='u')die("missing surrogate");cursor+=2;unsigned low=unicode();if(low<0xdc00||low>0xdfff)die("invalid surrogate");v=0x10000+((v-0xd800)<<10)+(low-0xdc00);}else if(v>=0xdc00&&v<=0xdfff)die("unpaired surrogate");
    if(v==0)die("NUL string unsupported");
    if(v<128)*p++=(char)v;else if(v<2048){*p++=(char)(192|(v>>6));*p++=(char)(128|(v&63));}else if(v<65536){*p++=(char)(224|(v>>12));*p++=(char)(128|((v>>6)&63));*p++=(char)(128|(v&63));}else{*p++=(char)(240|(v>>18));*p++=(char)(128|((v>>12)&63));*p++=(char)(128|((v>>6)&63));*p++=(char)(128|(v&63));}
   }else die("invalid escape");
  }else *p++=(char)c;
 }
 if(cursor!=scan-1)die("escape crosses string boundary");cursor++;*p=0;return out;
}
static int add(char type){if(used==MAX_NODES)die("too many values");if(used==capacity){capacity=capacity?capacity*2:256;if(capacity>MAX_NODES)capacity=MAX_NODES;void *p=realloc(nodes,(size_t)capacity*sizeof(*nodes));if(!p)die("out of memory");nodes=p;}int n=used++;nodes[n]=(struct node){type,NULL,-1,0,0};return n;}
static int compare(const void *a,const void *b){return strcmp(*(char *const *)a,*(char *const *)b);}
static int value(int depth){
 if(depth>64)die("nesting too deep");space();if(cursor==end)die("missing value");
 unsigned c=*cursor;int n=add((char)c);
 if(c=='{'||c=='['){
  cursor++;space();int close=c=='{'?'}':']';if(cursor<end&&*cursor==close){cursor++;nodes[n].next=used;return n;}
  nodes[n].child=used;
  for(;;){
   if(c=='{'){space();int key=add('s');nodes[key].text=string();nodes[key].next=used;space();if(cursor==end||*cursor++!=':')die("missing colon");}
   value(depth+1);nodes[n].count++;space();if(cursor==end)die("unclosed container");if(*cursor==close){cursor++;break;}if(*cursor++!=',')die("expected comma");
  }
  if(c=='{'){
   char **keys=allocate((size_t)nodes[n].count*sizeof(*keys));int k=nodes[n].child;
   for(int i=0;i<nodes[n].count;i++){keys[i]=nodes[k].text;k=nodes[k+1].next;}
   qsort(keys,nodes[n].count,sizeof(*keys),compare);for(int i=1;i<nodes[n].count;i++)if(!strcmp(keys[i-1],keys[i]))die("duplicate object key");free(keys);
  }
 }else if(c=='"'){nodes[n].type='s';nodes[n].text=string();}
 else{
  const unsigned char *start=cursor;
  if(c=='t'||c=='f'||c=='n'){const char *word=c=='t'?"true":c=='f'?"false":"null";size_t len=strlen(word);if((size_t)(end-cursor)<len||memcmp(cursor,word,len))die("invalid literal");cursor+=len;nodes[n].type=c=='n'?'z':'b';}
  else{
   nodes[n].type='n';if(*cursor=='-')cursor++;if(cursor==end)die("invalid number");
   if(*cursor=='0')cursor++;else{if(*cursor<'1'||*cursor>'9')die("invalid number");while(cursor<end&&isdigit(*cursor))cursor++;}
   if(cursor<end&&*cursor=='.'){cursor++;if(cursor==end||!isdigit(*cursor))die("invalid fraction");while(cursor<end&&isdigit(*cursor))cursor++;}
   if(cursor<end&&(*cursor=='e'||*cursor=='E')){cursor++;if(cursor<end&&(*cursor=='+'||*cursor=='-'))cursor++;if(cursor==end||!isdigit(*cursor))die("invalid exponent");while(cursor<end&&isdigit(*cursor))cursor++;}
  }
  size_t length=(size_t)(cursor-start);nodes[n].text=allocate(length+1);memcpy(nodes[n].text,start,length);nodes[n].text[length]=0;
 }
 nodes[n].next=used;return n;
}
static int member(int n,const char *key){
 if(nodes[n].type=='{'){int k=nodes[n].child;for(int i=0;i<nodes[n].count;i++){if(!strcmp(nodes[k].text,key))return k+1;k=nodes[k+1].next;}}
 else if(nodes[n].type=='['){char *stop;errno=0;long index=strtol(key,&stop,10);if(errno||!*key||*stop||index<0||index>=nodes[n].count)return -1;int k=nodes[n].child;while(index--)k=nodes[k].next;return k;}
 return -1;
}
static const char *scalar(int n){if(!nodes[n].text)die("expected scalar");for(const unsigned char *s=(unsigned char *)nodes[n].text;*s;s++)if(*s<32||*s==127)die("scalar contains control character");return nodes[n].text;}
static void output(int n){puts(scalar(n));}
static const char *text_member(int n,const char *key){int k=member(n,key);if(k<0||nodes[k].type!='s')die("missing string field");return scalar(k);}
static int identifier(const char *s){size_t len=strlen(s);if(!len||len>128||!((*s>='a'&&*s<='z')||(*s>='0'&&*s<='9')))return 0;for(;*s;s++)if(!((*s>='a'&&*s<='z')||(*s>='0'&&*s<='9')||*s=='+'||*s=='.'||*s=='-'))return 0;return 1;}
static int has_string(int n,const char *value){if(n<0||nodes[n].type!='[')die("expected string array");int k=nodes[n].child,found=0;for(int i=0;i<nodes[n].count;i++){if(nodes[k].type!='s')die("expected string array");if(!strcmp(scalar(k),value))found=1;k=nodes[k].next;}return found;}
/* Platform versions are strict stable Major.Minor.Patch triples. */
static void semver(const char *s,unsigned v[3]){
 for(int i=0;i<3;i++){
  if(!isdigit(*s)||(*s=='0'&&isdigit(s[1])))die("invalid stable semantic version");
  unsigned n=0;while(isdigit(*s)){if(n>100000000)die("version component too large");n=n*10+(*s++-'0');}v[i]=n;
  if(i<2){if(*s++!='.')die("invalid stable semantic version");}else if(*s)die("prerelease or invalid version not accepted");
 }
}
static int compare_version(const char *a,const char *b){unsigned x[3],y[3];semver(a,x);semver(b,y);for(int i=0;i<3;i++)if(x[i]!=y[i])return x[i]<y[i]?-1:1;return 0;}
static void compatible(int n,int argc,char **argv){
 if(argc!=10)die("compatible needs target version runtimeABI objectABI features legacyBuild currentBuild");
 int p=member(n,"platform");
 if(p<0){if(strcmp(text_member(n,"runtime"),argv[8])&&strcmp(text_member(n,"runtime"),argv[9]))die("unrecognized legacy binary baseline");return;}
 if(nodes[p].type!='{')die("invalid platform requirements");
 if(strcmp(text_member(p,"target"),argv[3]))die("binary target mismatch");
 const char *minimum=text_member(p,"minVersion");unsigned want[3],host[3];semver(minimum,want);semver(argv[4],host);
 if(want[0]!=host[0]||compare_version(argv[4],minimum)<0)die("binary platform version mismatch");
 int k=member(p,"runtimeAbi");if(k<0||nodes[k].type!='n'||strcmp(scalar(k),argv[5]))die("binary runtime ABI mismatch");
 k=member(p,"objectAbi");if(k>=0&&(nodes[k].type!='n'||strcmp(scalar(k),argv[6])))die("binary object ABI mismatch");
 k=member(p,"features");if(k<0||nodes[k].type!='[')die("missing platform features");
 int f=nodes[k].child;for(int i=0;i<nodes[k].count;i++){
  if(nodes[f].type!='s'||!identifier(scalar(f)))die("invalid feature name");
  char needle[132];snprintf(needle,sizeof needle,",%s,",scalar(f));if(!strstr(argv[7],needle))die("required platform feature unavailable");f=nodes[f].next;
 }
}
static void dependency(int n,int argc,char **argv){
 if(argc!=6)die("dependency needs name version manifestSHA256");
 int requirements=member(n,"dependencyRequirements"),r=requirements<0?-1:member(requirements,argv[3]);
 if(r<0){if(member(n,"platform")>=0)die("missing dependency compatibility requirement");return;}
 if(nodes[r].type!='{')die("invalid dependency requirement");
 int constraints=0,k=member(r,"version");if(k>=0){constraints++;if(nodes[k].type!='s'||strcmp(scalar(k),argv[4]))die("dependency version mismatch");}
 k=member(r,"sha256");if(k>=0){constraints++;if(nodes[k].type!='s'||strcmp(scalar(k),argv[5]))die("dependency manifest mismatch");}
 k=member(r,"minVersion");if(k>=0){constraints++;if(nodes[k].type!='s'||compare_version(argv[4],scalar(k))<0)die("dependency minimum version mismatch");}
 k=member(r,"maxExclusive");if(k>=0){constraints++;if(nodes[k].type!='s'||compare_version(argv[4],scalar(k))>=0)die("dependency maximum version mismatch");}
 if(!constraints)die("empty dependency compatibility requirement");
}
static int executable_path(const char *s){
 if(strncmp(s,"/usr/local/bin/",15)&&strncmp(s,"/usr/local/sbin/",16)&&strncmp(s,"/usr/local/libexec/",19))return 0;
 for(const char *p=s+1;*p;){const char *start=p;while(*p&&*p!='/'){unsigned char c=*p++;if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='+'||c=='.'||c=='-'))return 0;}size_t len=(size_t)(p-start);if(!len||(len==1&&*start=='.')||(len==2&&!memcmp(start,"..",2)))return 0;if(*p&&!p[1])return 0;if(*p)p++;}return 1;
}
static void launch(int n,int argc,char **argv){
 if(argc<5)die("launch needs launcher and installed release prefix");
 const char *name=argv[3],*prefix=argv[4];if(!identifier(name))die("invalid launcher name");
 const char *package=text_member(n,"name");if(!identifier(package))die("invalid package name");
 if(strcmp(name,package)&&!has_string(member(n,"aliases"),name))die("launcher does not own name or alias");
 int launchers=member(n,"launchers");if(launchers<0||nodes[launchers].type!='{'||nodes[launchers].count>64)die("invalid launchers");
 int spec=member(launchers,name);if(spec<0||nodes[spec].type!='{')die("launcher unavailable in installed record");
 const char *exe=text_member(spec,"executable");if(!executable_path(exe)||!has_string(member(n,"paths"),exe))die("launcher executable is not owned");
 int args=member(spec,"args");if(args<0||nodes[args].type!='['||nodes[args].count>64)die("invalid launcher args");
 // The trusted runner supplies a verified immutable release, not current's
 // mutable public symlink. No command string, shell, PATH search or eval.
 size_t prefixlen=strlen(prefix);if(!prefixlen||prefix[0]!='/'||prefixlen>4096)die("invalid release prefix");
 char *program=allocate(prefixlen+strlen(exe)+1);strcpy(program,prefix);strcat(program,exe+10);
 char **arguments=allocate(((size_t)argc+66)*sizeof(*arguments));int count=0;arguments[count++]=(char *)exe;
 int k=nodes[args].child;for(int i=0;i<nodes[args].count;i++){if(nodes[k].type!='s')die("expected launcher string arg");const char *arg=scalar(k);if(strlen(arg)>4096)die("launcher arg too long");arguments[count++]=(char *)arg;k=nodes[k].next;}
 for(int i=5;i<argc;i++)arguments[count++]=argv[i];arguments[count]=NULL;
 execv(program,arguments);fprintf(stderr,"guc-json: launch failed: %s\n",strerror(errno));exit(126);
}
static void utf8(const unsigned char *s,size_t length){
 for(size_t i=0;i<length;){unsigned c=s[i++],v=0;int count=0;
  if(c<128)continue;
  if(c>=0xc2&&c<=0xdf){v=c&31;count=1;}else if(c>=0xe0&&c<=0xef){v=c&15;count=2;}else if(c>=0xf0&&c<=0xf4){v=c&7;count=3;}else die("invalid UTF-8");
  int n=count;if(length-i<(size_t)n)die("short UTF-8");
  while(n--){c=s[i++];if((c&0xc0)!=0x80)die("invalid UTF-8 continuation");v=(v<<6)|(c&63);}
  if((count==2&&v<0x800)||(count==3&&v<0x10000)||v>0x10ffff||(v>=0xd800&&v<=0xdfff))die("noncanonical UTF-8");
 }
}
int main(int argc,char **argv){
 if(argc<3)die("usage: guc-json get|string|keys|items|strings FILE [PATH ...]; rows FILE ARRAY FIELD...");
 FILE *f=fopen(argv[2],"rb");if(!f)die("cannot open JSON");char *bytes=allocate(MAX_BYTES+1);size_t size=fread(bytes,1,MAX_BYTES+1,f);if(ferror(f)||size>MAX_BYTES)die("JSON exceeds bound or read failed");fclose(f);
 utf8((unsigned char *)bytes,size);cursor=(unsigned char *)bytes;end=cursor+size;int n=value(0);space();if(cursor!=end)die("trailing JSON data");
 if(!strcmp(argv[1],"launch"))launch(n,argc,argv);
 if(!strcmp(argv[1],"compatible")){compatible(n,argc,argv);return 0;}
 if(!strcmp(argv[1],"dependency")){dependency(n,argc,argv);return 0;}
 if(!strcmp(argv[1],"pairs")){
  if(argc!=5)die("pairs needs object and string field");n=member(n,argv[3]);if(n<0||nodes[n].type!='{')die("expected pairs object");
  int k=nodes[n].child;
  for(int i=0;i<nodes[n].count;i++){int v=k+1;if(nodes[v].type!='{')die("expected pair value object");const char *key=scalar(k),*value=text_member(v,argv[4]);fputs(key,stdout);putchar('\t');puts(value);k=nodes[v].next;}
 }else if(!strcmp(argv[1],"rows")){
  if(argc<5)die("rows needs array and fields");n=member(n,argv[3]);if(n<0||nodes[n].type!='[')die("expected rows array");
  int k=nodes[n].child;
  for(int i=0;i<nodes[n].count;i++){
   if(nodes[k].type!='{')die("expected row object");
   for(int f=4;f<argc;f++){int field=member(k,argv[f]);if(field<0)die("missing row field");const char *text=scalar(field);if(f>4)putchar('\t');fputs(text,stdout);}
   putchar('\n');k=nodes[k].next;
  }
 }else{
 for(int i=3;i<argc;i++){n=member(n,argv[i]);if(n<0)return 2;}
 if(!strcmp(argv[1],"get"))output(n);
 else if(!strcmp(argv[1],"string")){if(nodes[n].type!='s')die("expected string value");output(n);}
 else if(!strcmp(argv[1],"keys")){if(nodes[n].type!='{')die("expected object");int k=nodes[n].child;for(int i=0;i<nodes[n].count;i++){output(k);k=nodes[k+1].next;}}
 else if(!strcmp(argv[1],"items")||!strcmp(argv[1],"strings")){if(nodes[n].type!='[')die("expected array");int k=nodes[n].child;for(int i=0;i<nodes[n].count;i++){if(!strcmp(argv[1],"strings")&&nodes[k].type!='s')die("expected string array");output(k);k=nodes[k].next;}}
 else die("unknown operation");
 }
 for(int i=0;i<used;i++)free(nodes[i].text);free(nodes);free(bytes);return 0;
}

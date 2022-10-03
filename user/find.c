#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

//find函数在ls整体框架基础上进行修改

char*
fmtname(char *path) //函数用于生成路径字符串
{
  static char buf[DIRSIZ+1];
  char *p;

  // Find first character after last slash.
  for(p=path+strlen(path); p >= path && *p != '/'; p--)
    ;
  p++;

  // Return blank-padded name.
  if(strlen(p) >= DIRSIZ)
    return p;
  memmove(buf, p, strlen(p));
  memset(buf+strlen(p), 0, DIRSIZ-strlen(p)); //原程序在路径后填充空格，现将' '修改为0判断循环条件
  return buf; //buf为最后一个斜杠后的字符串
}

int recurse(char *path){
  char *buf = fmtname(path);
  if (buf[0] == '.' && buf[1] == 0) //不要递归进入"."
  {
    return 0;
  }else if (buf[0] == '.' && buf[1] == '.' && buf[2] == 0) //不要递归进入".."
  {
    return 0;
  }else{ //否则还有可能查找到target继续递归
    return 1;
  }

}

void
find(char *path, char *target)
{
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;

  if((fd = open(path, 0)) < 0){
    fprintf(2, "ls: cannot open %s\n", path);
    return;
  }

  if(fstat(fd, &st) < 0){
    fprintf(2, "ls: cannot stat %s\n", path);
    close(fd);
    return;
  }

  if (strcmp(fmtname(path),target) == 0) //查找成功输出路径
  {
    printf("%s\n", path);
  }
  

  switch(st.type){
  case T_FILE:
    break;

  case T_DIR:
    if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
      printf("ls: path too long\n");
      break;
    }
    strcpy(buf, path);
    p = buf+strlen(buf); //字符串指针定向到结尾
    *p++ = '/';
    while(read(fd, &de, sizeof(de)) == sizeof(de)){
      if(de.inum == 0)
        continue;
      memmove(p, de.name, DIRSIZ); //将'/'后的文件或路径名复制到字符串结尾
      p[DIRSIZ] = 0;
      if(stat(buf, &st) < 0){
        printf("ls: cannot stat %s\n", buf);
        continue;
      }

      if (recurse(buf)) //递归查找
      {
        find(buf,target);
      }
      

    }
    break;
  }
  close(fd);
}

int
main(int argc, char *argv[])
{
  if(argc == 1){
      printf("Too few argument!\n");
      exit(0);
  }

  if(argc == 2){ //只在.中查找target文件
    find(".",argv[1]);
    exit(0);
  }

  if (argc == 3) //在提供的path中查找target文件
  {
    find(argv[1],argv[2]);
    exit(0);
  }

  exit(0);
}
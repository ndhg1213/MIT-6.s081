#include "kernel/types.h"
#include "user.h"
#include "stddef.h"
#include "kernel/param.h"
#define MAXSTDIN 512

void fmtargs(char *stdin, char **args, int args_cnt){

    char buf[MAXSTDIN];
    int buf_len = 0;

    for (int i = 0; i < strlen(stdin); i++)
    {
        if((stdin[i] == ' ' || stdin[i] == '\n') && buf_len){ //参数字符串输入完毕
            args[args_cnt] = malloc(buf_len + 1);
            memcpy(args[args_cnt], buf, buf_len);
            buf_len = 0;
            args_cnt++;
        }else{
            buf[buf_len] = stdin[i]; //接收参数字符串
            buf_len++;
        }
    }

    args[MAXARG] = 0; //参数结尾增加空指针0
    
}

int main(int argc,char* argv[]){

    char stdin[MAXSTDIN]; //标准输入
    
    int args_cnt = 0;
    char* args[MAXARG + 1]; //多一个字符串空间用于满足结尾是空指针0的要求

    for(int i = 1;i < argc;i++){ 
        args[args_cnt] = malloc(sizeof(argv[i])); 
        memcpy(args[args_cnt],argv[i],sizeof(argv[i])); //复制xargs后的所有参数
        args_cnt++;
    }

    if (read(0,stdin,MAXSTDIN)) //存在标准输入
    {
        fmtargs(stdin,args,args_cnt);
    }
    
    exec(args[0],args);

    exit(0); //确保进程退出
}
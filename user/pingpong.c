#include "kernel/types.h"
#include "user.h"
#include "stddef.h"

int main(int argc,char* argv[]){
    char childbuf[6];
    char parentbuf[6];
    int pid;
    if(argc != 1){
        printf("wrong argument!\n"); //检查参数数量是否正确
        exit(-1);
    }

    int p[2];

    if (pipe(p) < 0)
    {
        exit(1); //检查管道是否创建成功
    }
    
    if((pid = fork()) == 0){
        read(p[0],childbuf,5);
        printf("0: received %s",childbuf);
        write(p[1],"pong\n",5);
    }else{
        write(p[1],"ping\n",5);
        wait(NULL); //等待子进程完成全部操作
        read(p[0],parentbuf,5);
        printf("%d: received %s",pid,parentbuf);
    }
    exit(0); //确保进程退出
}
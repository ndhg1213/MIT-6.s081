#include "kernel/types.h"
#include "user.h"
#include "stddef.h"
#define MAXSIZE 36     //数组长度
#define MAXBYTE 144    //数组长度换算得到字节长度

void prime(int pipe_read, int pipe_write){
    int buf[MAXSIZE];
    int value = 0;
    read(pipe_read,buf,MAXBYTE); //读取父进程传递的buf
    for (int i = 0; i < MAXSIZE; i++)
    {
        if(buf[i] == 1){ //确定本轮所用质数筛
            value = i;
            break;
        }
    }

    if (value == 0)
    {
        exit(0);
    }else{
        printf("prime %d\n",value);
        for (int i = 0; i < MAXSIZE; i++) //将本轮非质数筛掉
        {
            if(i % value == 0){
                buf[i] = 0; 
            }
        }
        if (fork() == 0)
        {
            prime(pipe_read,pipe_write); //子进程进入下一次质数筛取
        }else{
            write(pipe_write,buf,MAXBYTE); //父进程写入buf
        }
    }
}

int main(int argc,char* argv[]){
    int buf[MAXSIZE];

    for (int i = 1; i < MAXSIZE; i++)
    {
        buf[i] = 1;
    }

    int p[2];
    if (pipe(p) < 0)
    {
        exit(1); //检查管道是否创建成功
    }

    if(argc != 1){
        printf("wrong argument!\n"); //检查参数数量是否正确
        exit(-1);
    }

    if (fork() == 0)
    {
        prime(p[0],p[1]);
        wait(NULL); //确保孙进程退出
    }else{
        buf[0] = 0; //buf[0],buf[1]对应自然数为无用数据
        buf[1] = 0;
        write(p[1],buf,MAXBYTE); //write函数中的n为写入字节数，一个int对应4个字节，此时需写入字节数为4*36 = 144
        wait(NULL); // 子进程退出
    }

    exit(0); //确保进程退出
}
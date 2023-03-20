#include "cppNetWork.h"
#include <iostream>
#include <signal.h>
// 多进程服务端程序

LI::TcpServer tcp_server;  // 服务端对象

// 程序退出时调用的函数
// 避免僵尸进程(子进程先于父进程结束, 父进程没有回收子进程)
void ParentEXIT(int sig);  // 父进程退出函数
void ChildEXIT(int sig);   // 子进程退出函数


int main(int argc, char const *argv[])
{
    if (argc != 3) {
        std::cout << "No port" << std::endl;
        std::cout << "Using example: ./mpserver 192.168.1.101 5005 " << std::endl;
        return -1;
    }
    
    // 忽略所有信号
    for (int i = 0; i < 80; ++i) {
        signal(i, SIG_IGN);
    }
    // 给定终止信号
    signal(SIGINT, ParentEXIT); // ctrl + c 终止父进程
    signal(SIGTERM, ParentEXIT); // kill + 进程号终止父进程
    // 初始化tcp通信端口
    if (tcp_server.InitServer(argv[1], atoi(argv[2])) == false) {
        ParentEXIT(-1);
    }
    
    printf("mpserver start.\n");

    while (1) {
        // 等待客户端连接
        if (tcp_server.Accept() == false) {
            continue;
        }

        // 父进程返回的 pid > 0
        // 子进程返回的 pid == 0 
        if (fork() > 0) {
            tcp_server.CloseClient(); // 父进程不需要交换数据 节省文件描述符
            continue; // 父进程回到 Accept 
        }

        // 子进程退出信号
        signal(SIGINT, ChildEXIT);
        signal(SIGTERM, ChildEXIT);

        tcp_server.CloseListen(); // 子进程不需要监听

        printf("client %s connected.\n", tcp_server.GetIP());

        char strbuffer[1024];

        while (true) {
            memset(strbuffer, 0, sizeof(strbuffer));
            // 10s 超时
            if (tcp_server.Read(strbuffer, 10) == false) {
                break;
            }

            strcat(strbuffer, "ok"); // 加上 ok
            if (tcp_server.Write(strbuffer) == false) {
                break; // 发送错误
            }
        }

        printf("client %s disconnected.\n", tcp_server.GetIP());

        ChildEXIT(-1); // 子进程结束
    }



    return 0;
}


void ParentEXIT(int sig) {
    if (sig > 0) {
        // 忽略信号 防止再次收到
        signal(sig, SIG_IGN);
        signal(SIGINT, SIG_IGN);
        signal(SIGTERM, SIG_IGN);
    }

    kill(0, 15); // 给同进程组的进程发送信号 15 
    printf("Parent Process exit.\n");
    // 释放资源
    tcp_server.CloseListen();

    exit(0);
}

void ChildEXIT(int sig) {
    if (sig > 0) {
        // 忽略信号 防止再次收到
        signal(sig, SIG_IGN);
        signal(SIGINT, SIG_IGN);
        signal(SIGTERM, SIG_IGN);
    }
    printf("Child Process exit.\n");
    // 释放资源
    tcp_server.CloseClient();

    exit(0);
}


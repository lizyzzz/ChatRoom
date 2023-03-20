#include "cppNetWork.h"
#include <iostream>
#include <signal.h>
#include <thread>
#include <mutex>
// 多线程服务端程序

LI::TcpServer tcp_server;  // 服务端对象


// 程序退出时调用的函数
// 避免僵尸进程(子进程先于父进程结束, 父进程没有回收子进程)
void mainEXIT(int sig);  // 父进程退出函数

void do_work(const char* ip, int sockfd) {
    printf("client %s connected.\n", ip);

    int ibuflen = 0;
    char strbuffer[1024];

    while (true) {
        memset(strbuffer, 0, sizeof(strbuffer));
        // 10s 超时(不使用tcp_server防止数据竞争)
        if (LI::TcpRead(sockfd, strbuffer, &ibuflen, 10) == false) {
            break;
        }

        strcat(strbuffer, "ok"); // 加上 ok
        if (LI::TcpWrite(sockfd, strbuffer) == false) {
            break; // 发送错误
        }
    }

    printf("client %s disconnected.\n", ip);
    close(sockfd);
    return;
}

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
    signal(SIGINT, mainEXIT); // ctrl + c 终止父进程
    signal(SIGTERM, mainEXIT); // kill + 进程号终止父进程
    // 初始化tcp通信端口
    if (tcp_server.InitServer(argv[1], atoi(argv[2])) == false) {
        mainEXIT(-1);
    }
    
    printf("mpserver start.\n");

    while (1) {
        // 等待客户端连接
        if (tcp_server.Accept() == false) {
            continue;
        }
        // 创建线程
        std::thread t(do_work, tcp_server.GetIP(), tcp_server.m_connfd);
        t.detach(); // 分离线程
    }

    return 0;
}


void mainEXIT(int sig) {
    if (sig > 0) {
        // 忽略信号 防止再次收到
        signal(sig, SIG_IGN);
        signal(SIGINT, SIG_IGN);
        signal(SIGTERM, SIG_IGN);
    }

    printf("main Process exit.\n");
    // 释放资源
    tcp_server.CloseListen();

    exit(0);
}


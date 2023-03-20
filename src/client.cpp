#include "cppNetWork.h"
#include <iostream>
// 客户端程序


int main(int argc, char const *argv[])
{
    if (argc != 3) {
        printf("No ip and port\nUsing example: ./client 192.168.32.116 5000");
        return -1;
    }
    
    LI::TcpClient tcp_client;
    // 连接服务端
    if (tcp_client.ConnectToServer(argv[1], atoi(argv[2])) == false) {
        printf("Connect \"%s\" %s failed.\n", argv[1], argv[2]);
        return -1;
    }

    // printf("Connect \"%s\" %s success.\n", argv[1], argv[2]);

    char strbuffer[128];
    for (int i = 0; i < 30; ++i) {
        memset(strbuffer, 0, sizeof(strbuffer));
        snprintf(strbuffer, 100, "%d, this is %d boy, id: %03d.", getpid(), i + 1, i + 1);
        // printf("send: %s\n", strbuffer);
        // 发送数据
        if (tcp_client.Write(strbuffer) == false) {
            break; // 发送错误
        }

        memset(strbuffer, 0, sizeof(strbuffer));
        if (tcp_client.Read(strbuffer, 10) == false) {
            break; // 接收错误
        }
        // printf("read: %s\n", strbuffer);

        sleep(1); // 休眠 1s
    }

    tcp_client.Close();

    return 0;
}

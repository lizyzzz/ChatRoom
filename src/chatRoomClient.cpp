#include "cppNetWork.h"
#include <thread>
#include <string>
#include <iostream>
#include <vector>
#include <signal.h>

// 字体颜色
// 红 绿 黄 蓝 紫 青
std::vector<std::string> colors = {"\033[31m", "\033[32m", "\033[33m", "\033[34m", "\033[35m", "\033[36m"};
std::string def_col = "\033[0m";

void catch_ctrl_c(int sig);

class ChatRoomClient {
public:
    friend void catch_ctrl_c(int sig);

    LI::TcpClient tcp_client;    // 客户端对象
    bool is_connected;           // 连接标记
    const std::string m_ip;      // 服务端 ip 地址
    const int m_port;            // 服务端端口号
    std::thread thread_Input;    // 键入信息线程
    bool exit_flag;              // 退出标记
    std::string m_Username;      // 用户名
    int m_colorIndex;            // 字体颜色

    ChatRoomClient(const char* ip, const int port);
    // 接收信息
    void RecvMessage();
    // 输入要发送的信息
    void InputMessage();
    // 注册用户
    void RegisterUser();
    // 用户登陆
    void Login();
    // 连接客户端
    bool Connect();
    // 菜单
    void Menu();
    // 关闭连接
    void Close();
    // 运行接口
    void Run();
    // 删除输出的字符
    void EraseTextInTerminal(int cnt);

    ~ChatRoomClient();
};

ChatRoomClient::ChatRoomClient(const char* ip, const int port): is_connected(false), m_ip(ip), m_port(port), exit_flag(false) {}

ChatRoomClient::~ChatRoomClient() {
    Close();
    if (thread_Input.joinable()) {
        thread_Input.join();
    }
}

void ChatRoomClient::Close() {
    if (is_connected) {
        tcp_client.Close();
        is_connected = false;
    }
    return;
}
// 连接服务端并开始读线程
bool ChatRoomClient::Connect() {
    // 如果有先关闭
    if (is_connected) {
        Close();
    }
    // 连接
    if (tcp_client.ConnectToServer(m_ip.c_str(), m_port) == false) {
        return false;
    }
    is_connected = true;
    return true;
}

void ChatRoomClient::Menu() {
    std::cout << "============== Welcome ChatRoom ==============" << std::endl;
    std::cout << "=====          0. Register               =====" << std::endl;
    std::cout << "=====          1. Login                  =====" << std::endl;
    std::cout << "=====          2. Exit                   =====" << std::endl;
    std::cout << "==============================================" << std::endl;
    return;
}

// 接收信息线程函数
void ChatRoomClient::RecvMessage() {
    while (1) {

        char message_buffer[1024];
        memset(message_buffer, 0, sizeof(message_buffer));
        
        if (tcp_client.Read(message_buffer) == false) {
            break;
        }
        if (exit_flag) break;

        int code = -1;
        LI::GetStrFromXML(message_buffer, "code", code); // 获取信息类型
        std::string message;
        std::string other_name;
        int other_color;
        switch(code) {
            case 0: {
                    system("clear"); // 清屏
                    Menu();
                    std::cout << "Register Failed." << std::endl;  // 注册失败
                    return;}
            case 1: {  // 注册成功
                    system("clear"); // 清屏
                    Menu(); 
                    std::cout << "Register Success." << std::endl; 
                    return;}
            case 2: {
                    system("clear");
                    Menu();
                    std::cout << "Login Failed." << std::endl;  // 登录失败
                    return;}
            case 3: {std::cout << "Login Success." << std::endl;  // 登录成功
                    std::cout << "Select color(0 - 5): ";
                    std::cin >> m_colorIndex;
                    std::cout << "====== ChatRoom ======" << std::endl;
                    std::thread t(&ChatRoomClient::InputMessage, this);
                    thread_Input = std::move(t); // 创建输入线程
                    break;}
            case 4: {LI::GetStrFromXML(message_buffer, "message", message); // 收到信息
                    LI::GetStrFromXML(message_buffer, "name", other_name);
                    LI::GetStrFromXML(message_buffer, "color", other_color);
                    EraseTextInTerminal(5);
                    std::cout << colors[other_color] << other_name << ": " << def_col << message << std::endl;
                    std::cout << colors[m_colorIndex] << "You: " << def_col;
                    fflush(stdout);
                    break;
                    }
            default: return;
        }
        
    }

    Close();
    return;
}

// 发送信息线程函数
void ChatRoomClient::InputMessage() {
    std::string message;
    std::getline(std::cin, message); // 清空输入缓冲区
    while (1) 
    {
        std::cout << colors[m_colorIndex] << "You: " << def_col;
        fflush(stdout);
        std::getline(std::cin, message);

        if (message == "#exit") {
            exit_flag = true;
            // 形成格式
            std::string data("3"); // 退出登陆是 3 cmd
            LI::FormXML(data, "cmd");
            if (tcp_client.Write(data.c_str()) == false) {
                break;
            };
            catch_ctrl_c(SIGINT);
        } // 输入 exit 退出
        
        // 形成格式
        LI::FormXML(message, "message");
        std::string data("2"); // 发信息是 2 cmd
        LI::FormXML(data, "cmd");
        data.append("<name>");
        data.append(m_Username);
        data.append("</name>");
        data.append("<color>");
        data.append(std::to_string(m_colorIndex));
        data.append("</color>");
        data.append(message);

        // 发送
        if (tcp_client.Write(data.c_str()) == false) {
            break;
        };
    }

    Close();
    return;
}
// 注册用户
void ChatRoomClient::RegisterUser() {
    std::string message;
    // 输入信息
    std::string getStr;
    std::cout << "Create name: " ;
    std::cin >> getStr;
    message.assign(getStr);
    message += " ";
    std::cout << "Create password: " ;
    std::cin >> getStr;
    message.append(getStr);

    // 形成xml格式
    LI::FormXML(message, "message");
    // 添加 cmd label
    std::string data("0"); // 注册为 0 cmd
    LI::FormXML(data, "cmd");
    // 合成总的报文
    data.append(message);
    // 发送给客户端
    if (is_connected == false) {
        if(Connect() == false) {
            perror("connect.");
            return;
        }
    }
    if (tcp_client.Write(data.c_str()) == false) {
        perror("connect.");
        return;
    } 

    return;
}
// 登录
void ChatRoomClient::Login() {
    std::string message;
    // 输入信息
    std::string getStr;
    std::cout << "Your name: " ;
    std::cin >> getStr;
    m_Username = getStr; // 用户名
    message.assign(getStr);
    message += " ";
    std::cout << "Your password: " ;
    std::cin >> getStr;
    message.append(getStr);

    // 形成xml格式
    LI::FormXML(message, "message");
    // 添加 cmd label
    std::string data("1"); // 登录为 1 cmd
    LI::FormXML(data, "cmd");
    // 合成总的报文
    data.append(message);
    // 发送给客户端
    if (is_connected == false) {
        if(Connect() == false) {
            perror("connect.");
            return;
        }
    }
    if (tcp_client.Write(data.c_str()) == false) {
        perror("Write.");
        return;
    }

    return;
}

void ChatRoomClient::Run() {

    system("clear");
    Menu(); // 展示菜单

    while (1) {
        int choice = -1;
        std::cout << "Your Choice: ";
        std::cin >> choice;

        switch (choice) {
            case 0: {RegisterUser(); RecvMessage(); break;}
            case 1: {Login(); RecvMessage(); break;}

            case 2: {std::cout << "See you again." << std::endl; return;}
            
            default: {
                    std::cout << "Input error! Input again." << std::endl;
                    break;}
        }
    }
    return;
}

void ChatRoomClient::EraseTextInTerminal(int cnt) {
    char back_space = 8; //  backspace 字符
    for (int i = 0; i < cnt; ++i) {
        std::cout << back_space;
    }
}

std::shared_ptr<ChatRoomClient> crc_ptr;

int main(int argc, char const *argv[])
{
    if (argc != 3) {
        printf("No ip and port\nUsing example: ./client 192.168.32.116 5000");
        return -1;
    }
    signal(SIGINT, catch_ctrl_c);
    signal(SIGTERM, catch_ctrl_c);
    // ChatRoomClient crc(argv[1], atoi(argv[2]));
    crc_ptr = std::make_shared<ChatRoomClient>(argv[1], atoi(argv[2]));

    crc_ptr->Run();


    return 0;
}

// 线程退出函数
void catch_ctrl_c(int sig) {
    crc_ptr->Close();
    crc_ptr->exit_flag = true;
    if (crc_ptr->thread_Input.joinable()) {
        crc_ptr->thread_Input.detach();
    }
    exit(sig);
}
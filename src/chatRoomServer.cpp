#include "ThreadPool.hpp"
#include "cppNetWork.h"
#include <iostream>
#include <sys/epoll.h>
#include <set>
#include <sstream>
#include <signal.h>
#include <mysql/mysql.h>

// ---------------------- 用户信息文件类 ---------------------------

// 用户信息 sql 类
class UserSQL {
private:
    MYSQL mysql_conn;  // 数据库连接对象
    MYSQL* mysql_h;    // 连接句柄
    MYSQL_RES* result; // 查询结果
    bool is_connect;   // 连接标记
public:
    
    UserSQL();

    ~UserSQL();

    void Close();

    /// @brief 连接数据库
    /// @param user 连接数据库的用户名
    /// @param password 用户名对应的密码
    /// @param database 连接的数据库名
    /// @return 是否连接成功
    bool Connect(const std::string& user = "root", const std::string& password = "123456", const std::string& database = "account_information");

    /// @brief 查找用户名
    /// @param name 待查找的用户名
    /// @return 若用户名存在则返回密码, 不存在返回空字符串
    std::string SearchUser(const char* name);
    
    /// @brief 增加用户
    /// @param name 待增加的用户名
    /// @param password 待增加的密码
    /// @return true-成功, false-失败
    bool AddUser(const char* name, const char* password);
};

UserSQL::UserSQL() : is_connect(false){
    
}

UserSQL::~UserSQL() {
    Close();
}

void UserSQL::Close() {
    if (is_connect == true) {
        mysql_close(mysql_h); // 关闭连接
        is_connect = false;
    }
}

bool UserSQL::Connect(const std::string& user, const std::string& password, const std::string& database) {
    mysql_h = mysql_init(&mysql_conn);
    if (mysql_h == nullptr) {
        return false;
    }

    if (mysql_real_connect(mysql_h, "127.0.0.1", user.c_str(), password.c_str(), database.c_str(), 3306, nullptr, 0) == nullptr) {
        return false;
    }

    return true;
}

std::string UserSQL::SearchUser(const char* name) {
    std::string passwd;
    if (Connect() == false) {
        return passwd;
    }
    is_connect = true;

    std::string sql_sentence = "select password from infor where username = ";
    sql_sentence += "'";
    sql_sentence.append(name);
    sql_sentence += "'";

    if (mysql_query(mysql_h, sql_sentence.c_str()) != 0) {
        return passwd;
    }
    result = mysql_store_result(mysql_h); // 获取结果集
    if (result == nullptr) {
        return passwd;
    }
    
    MYSQL_ROW r = mysql_fetch_row(result); // 取出第一行记录
    passwd.append(r[0]);
    mysql_free_result(result);
    Close();
    return passwd;
}

bool UserSQL::AddUser(const char* name, const char* password) {
    if (Connect() == false) {
        return false;
    }
    is_connect = true;

    std::string sql_sentence = "insert into infor(username, password) value";
    sql_sentence.append("('");
    sql_sentence.append(name);
    sql_sentence.append("','");
    sql_sentence.append(password);
    sql_sentence.append("')");

    if (mysql_query(mysql_h, sql_sentence.c_str()) != 0) {
        return false;
    }
    Close();
    return true;
}


// ---------------------- /用户信息文件类 ---------------------------

void Catch_ctrl_c(int sig);

class ChatRoomServer {
private:
    LI::LogFile logfile;         // 日志文件
    LI::TcpServer tcp_server;    // 服务端对象
    LI::ThreadPool thread_pool;  // 线程池对象
    const size_t MAXENENTS;      // epoll一次能返回的最大的事件数
    std::set<int> set_connfd;    // 已连接的 connfd 容器
    int epollfd;                 // epollfd
    // 锁
    std::mutex set_lock;
    
public:
    friend void Catch_ctrl_c(int sig);

    ChatRoomServer(const size_t threads = 5 ,const size_t maxenents = 10);
    // 初始化服务端
    bool InitServer(const char* ip, const unsigned int port);
    // 初始化日志文件
    bool InitLogFile(const char* filename, std::ios::openmode openmode = std::ios::app, bool bBackup = true, bool bEnbuffer = false, const size_t MaxLogSize = 100);

    void runServer();

    ~ChatRoomServer();

private:
    // 接收并广播信息
    void broadcastMessage(const std::string& name, const std::string& str, int colorIndex, int sockfd);
    // 注册操作
    void Register(const std::string& str, int sockfd);
    // 登陆操作
    void LogIN(const std::string& str, int sockfd);
    // 退出登陆操作
    void LogOUT(int sockfd);
};

ChatRoomServer::ChatRoomServer(const size_t threads, const size_t maxenents): thread_pool(threads), MAXENENTS(maxenents), epollfd(-1) { }

bool ChatRoomServer::InitServer(const char* ip, const unsigned int port) {
    return tcp_server.InitServer(ip, port);
}

bool ChatRoomServer::InitLogFile(const char* filename, std::ios::openmode openmode, bool bBackup, bool bEnbuffer, const size_t MaxLogSize) {
    logfile.m_MaxLogSize = MaxLogSize;
    return logfile.Open(filename, openmode, bBackup, bEnbuffer);
}

ChatRoomServer::~ChatRoomServer() { 
    if (epollfd != -1) {
        close(epollfd);
    }
}

void ChatRoomServer::runServer() {
    // 创建一个 epoll 描述符
    epollfd = epoll_create(1);

    // 添加监听描述符事件
    struct epoll_event ev;
    ev.data.fd = tcp_server.m_listenfd;
    ev.events = EPOLLIN; // 读事件
    epoll_ctl(epollfd, EPOLL_CTL_ADD, tcp_server.m_listenfd, &ev); // 添加fd和对应的事件

    while (1) {
        struct epoll_event events[MAXENENTS]; // 存放发生事件的结构数组

        // 等待监视的 socket 有事件发生
        int infds = epoll_wait(epollfd, events, MAXENENTS, -1); // 无限等待不超时
        // 返回失败
        if (infds < 0) {
            perror("epoll_wait()");
            break;
        }
        else if (infds == 0) { // 无限等待不会超时
            printf("epoll_wait() timeout.\n");
            continue;
        }

        // 遍历所有发生事件的结构数组
        for (int i = 0; i < infds; ++i) {
            if ((events[i].data.fd == tcp_server.m_listenfd) && (events[i].events & EPOLLIN)) {
                // 如果发生的事件是 listenfd , 表示有新的客户端连上来
                if (tcp_server.Accept() == false) {
                    printf("accept() failed.\n");
                    continue;
                }

                // 把新的客户端添加到 epoll 中
                memset(&ev, 0, sizeof(struct epoll_event));
                ev.data.fd = tcp_server.m_connfd;
                ev.events = EPOLLIN;
                epoll_ctl(epollfd, EPOLL_CTL_ADD, tcp_server.m_connfd, &ev);

                logfile.Write(tcp_server.m_connfd, "connected.");

                continue;
            }
            else if (events[i].events & EPOLLIN) {
                // 客户端有数据过来或客户端的socket连接被断开
                char buffer[1024];
                memset(buffer,0,sizeof(buffer));

                int isize = 0;
                int sockfd = events[i].data.fd;
                LI::TcpRead(sockfd, buffer, &isize);

                if (isize <= 0) {
                    logfile.Write(events[i].data.fd, "disconnected.");
                    close(sockfd);
                    continue;
                }

                // 解析字符串
                int cmd = -1;
                LI::GetStrFromXML(buffer, "cmd", cmd);
                std::string message;
                std::string name;
                int colorInd;
                switch (cmd) {
                    // 注册账号
                    case 0: {LI::GetStrFromXML(buffer, "message", message); 
                            thread_pool.enqueue(&ChatRoomServer::Register, this, message, sockfd); break;}
                    // 登陆
                    case 1: {LI::GetStrFromXML(buffer, "message", message);
                            thread_pool.enqueue(&ChatRoomServer::LogIN, this, message, sockfd); break;}
                    // 发信息
                    case 2: {LI::GetStrFromXML(buffer, "message", message);
                            LI::GetStrFromXML(buffer, "name", name);
                            LI::GetStrFromXML(buffer, "color", colorInd);
                            thread_pool.enqueue(&ChatRoomServer::broadcastMessage, this, name, message, colorInd, sockfd); break;}
                    // 退出登陆
                    case 3: {thread_pool.enqueue(&ChatRoomServer::LogOUT, this, sockfd); break;}

                    // 其他
                    default:{
                        close(sockfd);
                        break;}
                }

                continue;
            }
        }
    }

    close(epollfd);
    epollfd = -1;
    return;
}
// 广播信息
void ChatRoomServer::broadcastMessage(const std::string& name, const std::string& str, int colorIndex, int sockfd) {
    {
        // 形成信息
        std::string message(str);
        LI::FormXML(message, "message");
        std::string message_name(name);
        LI::FormXML(message_name, "name");
        
        std::string data("4"); // 发送信息是 4 cmd
        LI::FormXML(data, "code");
        data.append(message_name);
        data.append("<color>");
        data.append(std::to_string(colorIndex));
        data.append("</color>");
        data.append(message); // 形成 xml
        std::unique_lock<std::mutex> lk(set_lock);
        for (const auto& connfd : set_connfd) {
            if (connfd == sockfd) continue; // 不广播给自己
            LI::TcpWrite(connfd, data.c_str(), data.size());
        }
    }
    return;
}

// 注册操作
void ChatRoomServer::Register(const std::string& str, int sockfd) {
    if (str.size() == 0) {
        // LI::TcpWrite(sockfd, "<code>0</code><message>Register Failed.</message>");
        LI::TcpWrite(sockfd, "<code>0</code>");
        return;
    }
    
    // 在用户信息文件添加用户
    int pos = str.find(' ');
    // 反馈信息
    UserSQL insert_obj;
    if (insert_obj.AddUser(str.substr(0, pos).c_str(), str.substr(pos + 1).c_str()) == true ) {
        LI::TcpWrite(sockfd, "<code>1</code>");
    }
    else {
        LI::TcpWrite(sockfd, "<code>0</code>");
    }

    return;
}
// 登陆操作
void ChatRoomServer::LogIN(const std::string& str, int sockfd) {
    if (str.size() == 0) {
        // LI::TcpWrite(sockfd, "<code>2</code><message>LogIN Failed.</message>");
        LI::TcpWrite(sockfd, "<code>2</code>");
        return;
    }
    int pos = str.find(' ');
    std::string name = str.substr(0, pos);
    std::string InPassword = str.substr(pos + 1);
    // 查找用户名
    UserSQL search_obj;
    std::string password = search_obj.SearchUser(name.c_str()); 
    // 密码正确
    if (password.size() > 0 && password == InPassword) {
        {
            std::unique_lock<std::mutex> lk(set_lock); // 上锁
            set_connfd.insert(sockfd); // 把 sockfd 插入 set
        }
        // LI::TcpWrite(sockfd, "<code>0</code><message>LogIN Success.</message>");
        LI::TcpWrite(sockfd, "<code>3</code>");
        return;
    }

    // LI::TcpWrite(sockfd, "<code>2</code><message>LogIN Failed.</message>");
    LI::TcpWrite(sockfd, "<code>2</code>");
    return;
}

// 退出登陆操作
void ChatRoomServer::LogOUT(int sockfd) {
    {
        std::unique_lock<std::mutex> lk(set_lock); // 上锁
        set_connfd.erase(sockfd); // 将 sockfd 删除
    }
    return; 
}

std::shared_ptr<ChatRoomServer> crs_ptr;


int main(int argc, char const *argv[])
{
    if (argc != 3) {
        std::cout << "No ip and port" << std::endl;
        std::cout << "Using example: ./chatRoomServer 192.168.1.101 5005 " << std::endl;
        return -1;
    }
    signal(SIGINT, Catch_ctrl_c);
    signal(SIGTERM, Catch_ctrl_c);

    crs_ptr = std::make_shared<ChatRoomServer>();

    crs_ptr->InitLogFile("../log/test.log", std::ios::app);
    crs_ptr->InitServer(argv[1], atoi(argv[2]));

    crs_ptr->runServer();


    return 0;
}


// 捕获信号
void Catch_ctrl_c(int sig) {
    if (crs_ptr->epollfd != -1) {
        close(crs_ptr->epollfd);
    }

    crs_ptr->logfile.Close();

    exit(sig);
}
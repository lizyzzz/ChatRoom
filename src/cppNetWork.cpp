// 自己网络库实现源码
#include "cppNetWork.h"
// 消息体长度
#define MSGBODYLEN 4

namespace LI {

// ------------------ 格式解析全局函数 ---------------------------
bool GetStrFromXML(const char* formBuffer, const char* labelname, std::string& RtnValue) {
    // 标签开始格式
    std::string startOflabelname("<");
    startOflabelname.append(labelname);
    startOflabelname += ">";
    // 标签结束格式
    std::string endOflabelname("</");
    endOflabelname.append(labelname);
    endOflabelname += ">";

    // 定位标签
    const char* start = strstr(formBuffer, startOflabelname.c_str());
    const char* end = nullptr;
    if (start != nullptr) {
        end = strstr(formBuffer, endOflabelname.c_str());
    }

    if (start == nullptr || end == nullptr) {
        return false;
    }

    int m_ValueLen = end - start - startOflabelname.size(); // 内容长度

    RtnValue.assign(start + startOflabelname.size(), m_ValueLen); // 拼接内容
    return true;
}
bool GetStrFromXML(const char* formBuffer, const char* labelname, int& RtnValue) {
    std::string strTmp;
    // 调用重载版本
    if ( GetStrFromXML(formBuffer, labelname, strTmp) == true ) {
        RtnValue = atoi(strTmp.c_str());
        return true;
    }

    return false;
}

void FormXML(std::string& message, const char* labelname) {
    std::string start("<");
    start.append(labelname);
    start.append(">");

    message.insert(0, start);

    message.append("</");
    message.append(labelname);
    message.append(">");
    return;
}


// ------------------ /格式解析全局函数 ---------------------------


// ------------------ LogFile 类成员函数 ---------------------------

LogFile::LogFile(const size_t MaxLogSize): m_bBackup(true), m_bEnbuffer(false), m_MaxLogSize(MaxLogSize) {
    // 最小是 10MB
    if (m_MaxLogSize < 10) {
        m_MaxLogSize = 10;
    }
}

bool LogFile::Open(const char* filename, std::ios::openmode openmode, bool bBackup, bool bEnbuffer) {
    // 如果上次打开的文件还没关闭
    Close();
    
    m_filename.assign(filename);
    m_openmode = openmode;
    m_bBackup = bBackup;
    m_bEnbuffer = bEnbuffer;
    

    m_os.open(m_filename, m_openmode); // 打开文件
    if (m_os.is_open()) {
        return true;
    }
    else {
        return false;
    }
}

void LogFile::Close() {
    // 关闭文件
    if (m_os.is_open()) {
        m_os.close();
    }
    m_filename.clear();
    m_openmode = std::ios::app;
    m_bBackup = true;
    m_bEnbuffer = false;
}

LogFile::~LogFile() {
    Close();
}


bool LogFile::BackupLogFile() {
    if (!m_os.is_open()) return false;
    // 不备份
    if (m_bBackup == false) return true;

    // 以下获取文件大小
    m_os.seekp(0, std::ios::end); // 从 end 向前偏移 0 个字符(字节)
    size_t cur_FileSize = (size_t)m_os.tellp(); // 返回当前标记的字符位置
    // Bytes to MB
    if (cur_FileSize > m_MaxLogSize * 1024 * 1024) {
        m_os.close(); // 关闭原来的文件
        // 获取系统时间
        char strLocalTime[21];
        memset(strLocalTime, 0, sizeof(strLocalTime));
        LocalTime(strLocalTime);

        // 设置新的名字 ../logdb.yyyy-mm-dd hh:mi:ss
        std::string bak_filename(m_filename);
        bak_filename += '.';
        bak_filename.append(strLocalTime);

        rename(m_filename.c_str(), bak_filename.c_str()); // 更改名字, 改名之前一定要先关闭文件

        // 打开新的文件
        m_os.open(m_filename, m_openmode);
        if (!m_os.is_open()) {
            return false;
        }
    }
    return true;
}
// ------------------ /LogFile 类成员函数 ---------------------------

// ------------------ 获取系统时间全局函数 ---------------------------
void LocalTime(char* stime, const int timetvl) {
    if (stime == nullptr) return;
    time_t timer;
    time(&timer); // 获取距离1970年1月1日0时0分0秒到当前的秒数
    timer += timetvl;

    // 转化为日历时间
    struct tm* now_time = localtime(&timer);
    now_time->tm_year += 1900;
    now_time->tm_mon += 1;

    // 存进 stime 格式"yyyy-mm-dd hh:mi:ss" 共19位
    snprintf(stime, 20, "%04u-%02u-%02u %02u:%02u:%02u", 
                    now_time->tm_year, now_time->tm_mon, now_time->tm_mday,
                    now_time->tm_hour, now_time->tm_min, now_time->tm_sec);
    
    return;
}

// ------------------ /获取系统时间全局函数 ---------------------------


















// ------------------ TcpClient 类成员函数 ---------------------------
TcpClient::TcpClient(): m_sockfd(-1), 
                        m_port(0), 
                        m_btimeout(false), 
                        m_buflen(0)
{
    memset(m_ip, 0, sizeof(m_ip));
}

bool TcpClient::ConnectToServer(const char* ip, const int port) {
    // 如果上一个连接还存在就先close
    if (m_sockfd != -1) {
        close(m_sockfd);
        m_sockfd = -1;
    }

    strcpy(m_ip, ip);
    m_port = port;

    struct hostent* h; // 不仅可以直接给ip地址, 还可以进行主机名/域名解析
    struct sockaddr_in serveraddr;
    // 创建socket
    if ( (m_sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
        perror("socket");
        return false;
    }
    // ip 地址解析
    if ( !(h = gethostbyname(m_ip)) ) {
        close(m_sockfd);
        m_sockfd = -1;
        perror("gethostbyname");
        return false;
    }

    memset(&serveraddr, 0, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(m_port); // 服务端通信端口
    memcpy(&serveraddr.sin_addr, h->h_addr, h->h_length); // 服务端 ip 地址

    // 连接
    if (connect(m_sockfd, (struct sockaddr*)&serveraddr, sizeof(serveraddr)) != 0) {
        close(m_sockfd);
        m_sockfd = -1;
        perror("connect");
        return false;
    }

    return true;
}

bool TcpClient::Read(char* buffer, const int itimeout) {
    // 连接不可用
    if (m_sockfd == -1) {
        return false;
    }

    // 用 select 实现超时机制
    if (itimeout > 0) {
        fd_set tmpfd;

        FD_ZERO(&tmpfd); // 清空socket集合
        FD_SET(m_sockfd, &tmpfd); // 添加 sockfd到集合

        struct timeval timeout;
        timeout.tv_sec = itimeout;
        timeout.tv_usec = 0;
        // timeout 时间内没有读事件则返回false, 有读事件则下一步
        int i;
        if ((i = select(m_sockfd + 1, &tmpfd, nullptr, nullptr, &timeout)) <= 0) {
            if (i == 0) {
                m_btimeout = true;
            }
            return false;
        }
    }

    // 接收数据
    m_buflen = 0;
    return (TcpRead(m_sockfd, buffer, &m_buflen));
}

bool TcpClient::Write(const char* buffer, const int ibuflen) {
    // socket 连接无效
    if (m_sockfd == -1) {
        return false;
    }

    fd_set tmpfd;

    FD_ZERO(&tmpfd); // 清空集和
    FD_SET(m_sockfd, &tmpfd); // 添加 sockfd 到集合

    struct timeval timeout;
    timeout.tv_sec = 5; // 默认是 5 s (一般不会写超时)
    timeout.tv_usec = 0;
    m_btimeout = false;
    // 使用 select 实现超时机制
    int i;
    if ( (i = select(m_sockfd + 1, nullptr, &tmpfd, nullptr, &timeout)) <= 0) {
        if (i == 0) {
            m_btimeout = true;
        }
        return false;
    }

    int ilen = ibuflen;
    if (ibuflen == 0) {
        ilen = strlen(buffer);
    }

    // 发送数据
    return (TcpWrite(m_sockfd, buffer, ilen));
}

void TcpClient::Close() {
    // 关闭连接并恢复变量状态
    if (m_sockfd > 0) {
        close(m_sockfd);
    }
    m_sockfd = -1;
    memset(m_ip, 0, sizeof(m_ip));
    m_port = 0;
    m_btimeout = false;
}

TcpClient::~TcpClient() {
    Close(); // 关闭连接
}

// ------------------ /TcpClient 类成员函数 -----------------------------

// ------------------ TcpServer 类成员函数 -----------------------------
TcpServer::TcpServer(): m_listenfd(-1),
                        m_connfd(-1),
                        m_btimeout(false),
                        m_buflen(0),
                        m_socklen(0)
{ }

bool TcpServer::InitServer(const char* ip, const unsigned int port) {
    // 关闭上次未关闭的socket
    if (m_listenfd > 0) {
        close(m_listenfd);
        m_listenfd = -1;
    }

    if ( (m_listenfd = socket(AF_INET, SOCK_STREAM, 0)) <= 0) {
        perror("socket");
        return false;
    }

    // 把socket设为可以重用的端口, 即已关闭的socket处于 Time_Wait 状态时就可以被使用
    int opt = 1;
    setsockopt(m_listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    // 服务端信息
    memset(&m_serveraddr, 0, sizeof(m_serveraddr));
    m_serveraddr.sin_family = AF_INET;
    m_serveraddr.sin_addr.s_addr = inet_addr(ip);
    m_serveraddr.sin_port = htons(port);

    // 绑定 socket 与 IP 和 端口
    if (bind(m_listenfd, (struct sockaddr*)&m_serveraddr, sizeof(m_serveraddr)) != 0) {
        perror("bind");
        CloseListen();
        return false;
    }

    // 监听socket
    if (listen(m_listenfd, 5) != 0) {
        perror("listen");
        CloseListen();
        return false;
    }

    m_socklen = sizeof(struct sockaddr_in);

    return true;
}

bool TcpServer::Accept() {
    if (m_listenfd == -1) {
        return false;
    }
    // accept
    if ( (m_connfd = accept(m_listenfd, (struct sockaddr*)&m_clientaddr, (socklen_t*)&m_socklen)) < 0) {
        return false;
    }
    return true;
}

char* TcpServer::GetIP() {
    // 返回 客户端 ip 地址的 字符串
    return (inet_ntoa(m_clientaddr.sin_addr));
}

bool TcpServer::Read(char* buffer, const int itimeout) {
    if (m_connfd == -1) {
        return false;
    }

    // 用 select 实现超时机制
    if (itimeout > 0) {
        fd_set tmpfd;

        FD_ZERO(&tmpfd); // 清空socket集合
        FD_SET(m_connfd, &tmpfd); // 添加 sockfd到集合

        struct timeval timeout;
        timeout.tv_sec = itimeout;
        timeout.tv_usec = 0;

        m_btimeout = false;
        int i;
        // timeout 时间内没有读事件则返回false, 有读事件则下一步
        if ( (i = select(m_connfd + 1, &tmpfd, nullptr, nullptr, &timeout)) <= 0) {
            if (i == 0) {
                m_btimeout = true;
            }
            return false;
        }
    }
    // 接受数据
    m_buflen = 0;
    return TcpRead(m_connfd, buffer, &m_buflen);
}

bool TcpServer::Write(const char* buffer, const int ibuflen) {
    if (m_connfd == -1) {
        return false;
    }

    fd_set tmpfd;

    FD_ZERO(&tmpfd); // 清空集和
    FD_SET(m_connfd, &tmpfd); // 添加 sockfd 到集合

    struct timeval timeout;
    timeout.tv_sec = 5; // 默认是 5 s (一般不会写超时)
    timeout.tv_usec = 0;
    m_btimeout = false;
    // 使用 select 实现超时机制
    int i;
    if ( (i = select(m_connfd + 1, nullptr, &tmpfd, nullptr, &timeout)) <= 0) {
        if (i == 0) {
            m_btimeout = true;
        }
        return false;
    }

    int ilen = ibuflen;
    if (ibuflen == 0) {
        ilen = strlen(buffer);
    }
    
    // 发送数据
    return (TcpWrite(m_connfd, buffer, ilen));
}

void TcpServer::CloseListen() {
    if (m_listenfd > 0) {
        close(m_listenfd);
        m_listenfd = -1;
    }
}

void TcpServer::CloseClient() {
    if (m_connfd > 0) {
        close(m_connfd);
        m_connfd = -1;
    }
}

TcpServer::~TcpServer() {
    CloseClient();
    CloseListen();
}
// ------------------ /TcpServer 类成员函数 -----------------------------

// ------------------ 全局函数 ------------------------------------------
bool TcpRead(const int sockfd, char* buffer, int* ibuflen, const int itimeout) {
    // socket 连接无效
    if (sockfd == -1) {
        return false;
    }

    // 用 select 实现超时机制
    if (itimeout > 0) {
        fd_set tmpfd;

        FD_ZERO(&tmpfd); // 清空socket集合
        FD_SET(sockfd, &tmpfd); // 添加 sockfd到集合

        struct timeval timeout;
        timeout.tv_sec = itimeout;
        timeout.tv_usec = 0;
        // timeout 时间内没有读事件则返回false, 有读事件则下一步
        if (select(sockfd + 1, &tmpfd, nullptr, nullptr, &timeout) <= 0) {
            return false;
        }
    }

    *ibuflen = 0;

    // 为解决 TCP 粘包和分包 的问题
    // 报文组成为: 报文长度 + 报文体
    
    // 先读取前 MSGBODYLEN 个字节确定消息的长度
    if (Readn(sockfd, (char*)(ibuflen), MSGBODYLEN) == false) {
        return false;
    }

    *ibuflen = ntohl(*ibuflen); // 把网络字节序转换为主机字节序

    // 读取 ibuflen 个字节到 buffer
    if (Readn(sockfd, buffer, (*ibuflen)) == false) {
        return false;
    }

    return true;
}


bool TcpWrite(const int sockfd, const char* buffer, const int ibuflen) {
    // socket 连接无效
    if (sockfd == -1) {
        return false;
    }

    fd_set tmpfd;

    FD_ZERO(&tmpfd); // 清空集和
    FD_SET(sockfd, &tmpfd); // 添加 sockfd 到集合

    struct timeval timeout;
    timeout.tv_sec = 5; // 默认是 5 s (一般不会写超时)
    timeout.tv_usec = 0;
    // 使用 select 实现超时机制
    if ( select(sockfd + 1, nullptr, &tmpfd, nullptr, &timeout) <= 0) {
        return false;
    }

    int ilen = 0;
    // 如果长度为0(缺省值) 则使用字符串长度
    if (ibuflen == 0) {
        ilen = strlen(buffer);
    }
    else {
        ilen = ibuflen;
    }

    int ilenn = htonl(ilen); // 把主机字节序转换为网络字节序

    // 为解决 TCP 粘包和分包 的问题
    // 报文组成为: 报文长度 + 报文体

    // 组合报文
    char strSendBuffer[ilen + 4];
    memset(strSendBuffer, 0, sizeof(strSendBuffer));
    memcpy(strSendBuffer, &ilenn, 4);
    memcpy(strSendBuffer + 4, buffer, ilen);
    // 发送数据
    if (Writen(sockfd, strSendBuffer, ilen + 4) == false) {
        return false;
    }

    return true;
}

bool Readn(const int sockfd, char* buffer, const size_t n) {
    int nLeft = n;
    int nread = 0, idx = 0;

    // 循环读取 n 个字节
    // 这里读取的结果可能 > n ?(待测试)
    while (nLeft > 0) {
        // 没有数据准备好会被阻塞
        if ( (nread = recv(sockfd, buffer + idx, nLeft, 0)) <= 0) {
            return false;
        }
        idx += nread;
        nLeft -= nread;
    }

    return true;
}

bool Writen(const int sockfd, const char* buffer, const size_t n) {
    int nLeft = n;
    int nwrite = 0, idx = 0;

    // 循环发送 n 个字节
    while (nLeft > 0) {
        // 发送缓冲区满时也可能被阻塞
        if ( (nwrite = send(sockfd, buffer + idx, nLeft, 0)) <= 0) {
            return false;
        }
        idx += nwrite;
        nLeft -= nwrite;
    }

    return true;
}
// ------------------ /全局函数 ------------------------------------------


}


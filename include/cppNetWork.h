// 封装自己的网络库


#ifndef CPP_NETWORK_H_
#define CPP_NETWORK_H_

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <cstring>
#include <unistd.h>
#include <netdb.h>
#include <string.h>
#include <string>
#include <fstream>
#include <stdio.h>
#include <stdarg.h>
#include <sys/select.h>
#include <sys/time.h>
#include <arpa/inet.h>

namespace LI {

/// @brief 从格式字符串里解析内容
/// @param formBuffer 待解析的字符串
/// @param labelname 字段的标签名
/// @param RtnValue 解析结果
bool GetStrFromXML(const char* formBuffer, const char* labelname, std::string& RtnValue);
bool GetStrFromXML(const char* formBuffer, const char* labelname, int& RtnValue);

/// @brief 把字符串形成 xml 格式
/// @param message 字符串内容(引用原地修改)
/// @param labelname 标签名字
void FormXML(std::string& message, const char* labelname);



/// @brief 获取系统当前日历时间的
/// @param stime 存放获取时间结果的字符串 格式"yyyy-mm-dd hh24:mi:ss"
/// @param timetvl 时间偏移量, 单位: s, 表示要将当前时间偏移的时间量, 如: 30表示当前时间+30s, 缺省 0
void LocalTime(char* stime, const int timetvl = 0);


// 日志文件操作类
class LogFile {
public:
    std::ofstream m_os;     // 流对象 
    std::string m_filename; // 日志文件名, 建议采用绝对路径
    std::ios::openmode m_openmode;    // 日志文件打开方式, 缺省值为 append
    bool m_bEnbuffer;       // 写日志时是否启用缓冲机制, 缺省不启用
    size_t m_MaxLogSize;      // 最大入值文件的大小, 单位 MB, 缺省100MB
    bool m_bBackup;         // 是否自动切换, 当文件大小超过m_MaxLogSize 将自动切换, 缺省启用

    /// @brief 构造函数
    /// @param MaxLogSize 最大日志文件的大小, 单位 MB, 缺省100MB
    LogFile(const size_t MaxLogSize = 100);

    /// @brief 打开日志文件
    /// @param filename 日志文件名, 建议采用绝对路径名, 如果文件名中的目录不存在, 就先创建目录
    /// @param openmode 文件打开方式, 缺省值为 append
    /// @param bBackup 是否自动切换, true-切换, false-不切换. 多进程程序必须为false
    /// @param bEnbuffer 是否启动文件缓冲机制, true-启用, false-不启用
    /// @return 
    bool Open(const char* filename, std::ios::openmode openmode = std::ios::app, bool bBackup = true, bool bEnbuffer = false);

    /// @brief 如果日志文件大于m_MaxLogSize的值, 就把当前的日志文件名改为历史日志文件名, 再创建新的当前日志文件.备份后的文件会在日志文件名后加上日期时间.
    /// @return true-成功, false-失败 
    bool BackupLogFile();
    
    /// @brief 把内容写到日志文件, 包含时间(可变参数函数模板, 不要分文件实现)
    /// @tparam ...Args 可变参数类型模板
    /// @param ...args 可变参数
    /// @return true-成功; false-失败
    template<class... Args>
    bool Write(const Args&... args);
    
    /// @brief 关闭日志文件
    void Close();

    // 析构函数
    ~LogFile();

private:
    // 实现可变参数调用打印
    template<class T>
    bool Fprint(const T& t) {
        if (!m_os.is_open()) return false;
        m_os << t;
        return true;
    }

    template<class T, class... Args> 
    bool Fprint (const T& t, const Args&... rest) {
        if (!m_os.is_open()) return false;
        m_os << t << " ";
        Fprint(rest...); // 递归调用
        return true;
    }
};

template<class... Args>
bool LogFile::Write(const Args&... args) {
    if (!m_os.is_open()) return false;
    // 备份文件
    if (BackupLogFile() == false) return false;

    // 获取系统时间
    char strLocalTime[21];
    memset(strLocalTime, 0, sizeof(strLocalTime));
    LocalTime(strLocalTime);
    m_os << strLocalTime << " "; // 写入时间

    // 递归写入可变参数
    Fprint(args...);

    if (m_bEnbuffer == false)
    m_os << std::endl;
    return true;
}





// socket通信的客户端类
class TcpClient {
public:
    int m_sockfd;    // 客户读的 socket
    char m_ip[21];   // 服务端的 ip 地址
    int m_port;      // 与服务端通信的端口
    bool m_btimeout; // 调用 Read 和 Write 的超时标志
    int m_buflen;    // 调用 Read 方法后, 接收到的报文大小, 单位: bytes

    TcpClient(); // 构造函数

    /// @brief 向服务端发起连接请求
    /// @param ip 服务端 ip 地址
    /// @param port 服务端 的端口
    /// @return 是否成功连接
    bool ConnectToServer(const char* ip, const int port); 

    /// @brief 接收服务端发送过来的数据
    /// @param buffer 接受数据缓冲区的地址, 数据的长度存放在 m_buflen 成员中
    /// @param itimeout 等待数据的超时时间, 单位: s, 缺省值是0-无限等待
    /// @return true-成功; false-失败; 失败有两种情况: 1) 等待超时, 成员变量m_btimeout的值被设置为true; 2) socket不可用
    bool Read(char* buffer, const int itimeout = 0);

    /// @brief 向服务端发送数据
    /// @param buffer 待发送数据缓冲区地址
    /// @param ibuflen 待发送数据缓冲区的大小, 单位: bytes, 缺省值为0. 如果发送的是ascii字符串, ibuflen为0, 如果是二进制流, ibuflen为二进制数据块大小
    /// @return true-成功; false-失败; 如果失败 则socket不可用
    bool Write(const char* buffer, const int ibuflen = 0);

    /// @brief 断开与服务器的连接
    void Close();

    ~TcpClient(); // 析构函数

};

// socket 通信的服务端类
class TcpServer{
private:
    int m_socklen;                   // 结构体 sockaddr_in 的长度
    struct sockaddr_in m_clientaddr; // 客户端的地址
    struct sockaddr_in m_serveraddr; // 服务端的地址

public:
    int m_listenfd;  // 服务端用于监听的 socket
    int m_connfd;    // 客户端连接上来的 socket
    bool m_btimeout; // 调用 Read 和 Write 的超时标志
    int m_buflen;    // 调用 Read 方法后, 接收到的报文大小, 单位: bytes

    TcpServer();  //构造函数

    /// @brief 服务端初始化
    /// @param ip 指定服务端的ip地址
    /// @param port 指定服务端用于监听的端口
    /// @return true-成功; false-失败, 一般情况下, 只要port设置正确, 没有被占用, 初始化都会成功.
    bool InitServer(const char* ip, const unsigned int port);

    /// @brief 阻塞等待客户端的连接请求
    /// @return true-有新的客户端连接上来; false-失败, Accept被阻塞, 可以重新 Accept.
    bool Accept();

    /// @brief 获取客户端的ip地址
    /// @return 客户端的ip地址的字符串形式, 如"192.168.1.101" 
    char* GetIP();

    /// @brief 接收客户端发送过来的数据
    /// @param buffer 接收数据缓冲区的地址, 数据的长度存放在m_buflen成员变量中
    /// @param itimeout 等待数据的超时时间, 单位: s, 缺省值是0-无限等待
    /// @return true-成功; false-失败; 失败有两种情况: 1) 等待超时, 成员变量m_btimeout的值被设置为true; 2) socket不可用
    bool Read(char *buffer, const int itimeout = 0);

    /// @brief 向客户端发送数据
    /// @param buffer 待发送数据缓冲区地址
    /// @param ibuflen 待发送数据缓冲区的大小, 单位: bytes, 缺省值为0. 如果发送的是ascii字符串, ibuflen为0, 如果是二进制流, ibuflen为二进制数据块大小
    /// @return true-成功; false-失败; 如果失败 则socket不可用
    bool Write(const char* buffer, const int ibuflen = 0);

    /// @brief 关闭监听的socket, 即m_listenfd, 常用于多进程服务程序的子进程代码中 
    void CloseListen();

    /// @brief 关闭客户端的socket, 即m_connfd, 常用于多进程服务程序的父进程代码中
    void CloseClient();

    ~TcpServer(); // 析构函数

};

/// @brief 接收socket的另一端发送过来的数据
/// @param sockfd 可用的socket连接
/// @param buffer 接收数据缓冲区的地址
/// @param ibuflen 成功接收数据的字节数
/// @param itimeout 接收等待超时的时间, 单位: s, 缺省值是0-无限等待
/// @return true-成功; false-失败, 如果失败有两种情况: 1)等待超时; 2)socket连接已不可用
bool TcpRead(const int sockfd, char* buffer, int* ibuflen, const int itimeout = 0);

/// @brief 向socket的另一端发送数据
/// @param sockfd 可用的socket连接
/// @param buffer 待发送数据缓冲区的地址
/// @param ibuflen 待发送数据的字节数, 如果发送的是ascii字符串, ibuflen取0, 如果是二进制流数据, ibuflen为二进制数据块的大小
/// @return true-成功；false-失败，如果失败，表示socket连接已不可用
bool TcpWrite(const int sockfd, const char* buffer, const int ibuflen = 0);

/// @brief 从已经准备好的socket中读取数据
/// @param sockfd 已经准备好的socket连接
/// @param buffer 接收数据缓冲区的地址
/// @param n 本次接收数据的字节数
/// @return true-接收到n字节的数据; false-socket连接不可用
bool Readn(const int sockfd, char* buffer, const size_t n);

/// @brief 向已经准备好的socket中写入数据
/// @param sockfd 已经准备好的socket连接
/// @param buffer 待发送数据缓冲区的地址
/// @param n 待发送数据的字节数
/// @return true-发送完n字节的数据; false-socket连接不可用
bool Writen(const int sockfd, const char* buffer, const size_t n);


}



#endif

# ChatRoom  
一个用C++11实现的多人网络聊天室

## 使用方法：
### 在工作空间目录下编译： 
```
mkdir build  
cd build  
cmake ..  
make
```
### 数据库配置方法：
```
CREATE DATABASE account_information;
USE account_information;
CREATE TABLE infor(
id INT PRIMARY KEY AUTO_INCREMENT,
username VARCHAR(20) UNIQUE,
`password` VARCHAR(20) NOT NULL
);
```
### 运行方法：  
服务端：  
  >./chatRoomServer 192.168.xxx.xxx yyyy  
  >
  其中 192.168.xxx.xxx 服务端主机 ip 地址, yyyy 服务端主机端口  
客户端：  
  >./chatRoomClient 192.168.xxx.xxx yyyy  
  >
  其中 192.168.xxx.xxx 服务端主机 ip 地址, yyyy 服务端主机端口  
## 展示画面
![github](https://github.com/lizyzzz/ChatRoom/blob/main/Show.jpg)
# 代码说明
## cppNetWork.h和cppNetWork.cpp
### TcpServer and TcpClient服务端和客户端类
&emsp;&emsp;该头文件封装了TcpServer，TcpClient类用于TCP的C/S通信模式。其中自定义了 4 Bytes长度的报文长度头部信息，用于解决TCP的粘包和分包问题。形成自定义的Readn和Writen函数。
### LogFile日志文件类
&emsp;&emsp;使用可变参数函数模板，实现多格式兼并写入文件，同时带有备份功能，可以限制文件的最大空间。
### XML系列函数
&emsp;&emsp;封装三个函数用来解析XML格式文件和形成XML格式文件。
## ThreadPool.hpp线程池
&emsp;&emsp;以函数模板的形式添加工作任务task。线程在构造函数初始化运行。
### 任务队列
&emsp;&emsp;使用函数enqueue()把任务加到任务队列，工作线程从任务队列中取任务执行。使用条件变量和互斥锁实现多线程同步。
### 注意事项
&emsp;&emsp;任务队列中的任务类型需要采用function<void()>的形式，以保证任务函数的类型统一。在入任务队列时统一使用packaged_task进行封装。
## 服务端和客户端实现逻辑
### UserSQL类
&emsp;&emsp;实现了保证线程安全的文件读写数据库功能  
&emsp;&emsp;&emsp;&emsp;新增用户：把用户名和密码写到数据库中的表。  
&emsp;&emsp;&emsp;&emsp;查找用户：给定用户名查找其密码，查询数据库。
### ChatRoomServer类
&emsp;&emsp;使用epoll实现IO多路复用模型，即使用epoll监听事件，事件发生后解析xml格式报文使用线程池执行任务。  
&emsp;&emsp;任务类型有：注册账号请求，登录请求，退出登录请求，发信息（广播信息服务）。  
>接受服务端消息的主线程会在read函数阻塞，当收到登录失败或注册失败的信息时，应该结束接受消息函数。返回到上一级重新选择功能。  
>注意网络编程close函数的功能，最后一次是发送size为==0的。
### ChatRoomClient类
&emsp;&emsp;使用两个线程作为客户端：  
&emsp;&emsp;&emsp;&emsp;主线程实现功能选择，登录后主线程接受来自服务端的消息，并根据服务端的消息进行相应的处理。  
&emsp;&emsp;&emsp;&emsp;子线程只有在登录成功的时候才启动，用来读取终端的输入消息并发送消息给服务端。  
&emsp;&emsp;主要功能有：注册用户，登录用户，退出  
&emsp;&emsp;当注册用户或登录用户时，需要和服务端进行TCP连接，该连接会保持至客户端退出。

# server

## 项目介绍

~~~
1.基于muduo库实现  
2.注释教学 doxygen风格注释
3.代码风格 函数下划线 变量驼峰 
~~~

## 项目构建

~~~makefile
#直接使用makefile 构建
make -j
~~~~


~~~cmake
#或者使用cmake
mkdir build && cd build
cmake ..
make -j

~~~



## 一切的基础  Channel介绍

~~~
1.channel 管理了文件描述符 并且通过io多路复用来监听这个描述符  注册感兴趣的事件  和 返回具体发生了什么事件

2.成员变量和函数 含义见注释
~~~



### 不同类内channel的作用

| 名称          | 所属模块      | 功能场景                         | 关键实现类    |
| ------------- | ------------- | -------------------------------- | ------------- |
| acceptChannel | Acceptor      | 监听新连接请求                   | Acceptor      |
| wakeupChannel | EventLoop     | 跨线程唤醒事件循环               | EventLoop     |
| connChannel   | TcpConnection | 管理单个连接的读写事件及生命周期 | TcpConnection |

### channel类内回调的具体作用
| Channel类型   | 所属模块      | 事件类型         | 绑定回调函数                | 核心功能               |
| ------------- | ------------- | ---------------- | --------------------------- | ---------------------- |
| acceptChannel | Acceptor      | EPOLLIN（读）    | Acceptor::handle_read       | 接收新连接             |
| wakeupChannel | EventLoop     | EPOLLIN（读）    | EventLoop::handle_read      | 跨线程唤醒事件循环     |
| connChannel   | TcpConnection | EPOLLIN（读）    | TcpConnection::handle_read  | 读取数据并回调用户逻辑 |
| connChannel   | TcpConnection | EPOLLOUT（写）   | TcpConnection::handle_write | 处理数据发送完成       |
| connChannel   | TcpConnection | EPOLLHUP（关闭） | TcpConnection::handle_close | 关闭连接并释放资源     |
| connChannel   | TcpConnection | EPOLLERR（错误） | TcpConnection::handle_error | 处理socket错误         |



更多的回调和 函数调用在代码中给出


# TCP/UDP 传输层信息系统

本项目实现了基于 TCP 和 UDP 的多线程聊天服务器与客户端，支持基本的聊天功能和服务器统计信息查询。TCP 和 UDP 版本均支持多客户端并发连接，UDP 版本实现了基本的可靠性机制（ACK/重传）。

## 项目成员
- 组长 Xinhua Wang
- 开发 Peng Zhou
- 测试 Mengmeng Xu

## 代码管理
使用Git管理项目文件
## 目录结构

- `tcp_server/`  
  - `TCPServer.cpp`：TCP多线程聊天服务器  
  - `TCPClient.cpp`：TCP聊天客户端  
  - `TCPCommon.h`：TCP消息结构及工具  
- `udp_server/`  
  - `UDPServer.cpp`：UDP多线程聊天服务器  
  - `UDPClient.cpp`：UDP聊天客户端  
  - `UDPCommon.h`：UDP消息结构及工具  
- `lecture_code/`：教学示例代码  

## 编译方法

```sh
cd ./tcp_server
# 编译 TCP 服务器和客户端
g++ TCPServer.cpp -o tcp_server
g++ TCPClient.cpp -o tcp_client

cd ./udp_server
# 编译 UDP 服务器和客户端
g++ UDPServer.cpp -o udp_server
g++ UDPClient.cpp -o udp_client
```
## 运行方法

### TCP 聊天服务器

```sh
./tcp_server [端口号]
```
默认端口为 5000

### TCP 聊天客户端

```sh
./cp_client [服务器IP] [端口号]
```
默认服务器IP为 127.0.0.1，端口为 5000

### UDP 聊天服务器

```sh
./udp_server [端口号]
```
默认端口为 5001

### UDP 聊天客户端

```sh
./udp_client [服务器IP] [端口号]
```
默认服务器IP为 127.0.0.1，端口为 5001

## 功能说明

- 支持 `/say <消息>` 发送聊天内容
- 支持 `/stats` 查询服务器统计信息（在线人数、运行时间等）
- 支持 `/quit` 断开连接
- TCP/UDP 均为多线程实现，支持多个客户端并发
- UDP 客户端实现了基本的可靠性（ACK/重传）


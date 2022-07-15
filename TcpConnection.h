#pragma once

#include "noncopyable.h"
#include "InetAddress.h"
#include "Callbacks.h"
#include "Buffer.h"
#include "Timestamp.h"


#include <memory>
#include <string>
#include <atomic>

class Channel;
class EventLoop;
class Socket;

/**
 * TcpServer =>Acceptor => 有一个新用户连接，通过accept函数得到connfd
 * => 然后打包成TcpConnection 设置回调 => channel => 注册到poller上 => channel的回调操作
 * 
 */
class TcpConnection : noncopyable, public std::enable_shared_from_this<TcpConnection>
{
private:
    enum StateE {kDisconnected, kConnecting, kConnected, kDisconnecting};
    

    void handleRead(Timestamp receiveTime);
    void handleWrite();
    void handleClose();
    void handleError();

    void sendInLoop(const void* message, size_t len);
    void shutdownInLoop();

    EventLoop *loop_; // 这里绝对不是baseloop，因为TcpConnection都是在subLoop里面管理的
    const std::string name_;
    std::atomic_int state_;
    bool reading_;

    // 这里和Acceptor类似 Acceptor <= mainLoop 监听连接事件  TcpConnection<=subLoop 监听数据读写事件
    std::unique_ptr<Socket> socket_;
    std::unique_ptr<Channel> channel_;

    const InetAddress localAddr_;  // 记录当前主机的ip地址端口号
    const InetAddress peerAddr_;   // 记录客户端的ip地址端口号

    // 用户通过TcpServer提供的接口设置回调函数，Tcpserver=>TcpConnection=>Channel，poller监听channel,有时间发生就通过loop执行相应的回调函数
    ConnectionCallback connectionCallback_; // 有连接时的回调
    MessageCallback messageCallback_; //有读写时的回调
    WriteCompleteCallback writeCompleteCallback_; // 消息发送完成以后的回调
    HighWaterMarkCallback highWaterMarkCallback_;
    CloseCallback closeCallback_;
    size_t highWaterMark_;

    Buffer inputBuffer_; // 接收数据的缓冲区
    Buffer outputBuffer_; // 发送数据的缓冲区

public:
    TcpConnection(EventLoop *loop,
                const std::string &nameArg,
                int sockfd,
                const InetAddress& localAddr_,
                const InetAddress& peerAddr_);
    ~TcpConnection();

    EventLoop* getLoop() const { return loop_;}
    const std::string& name() const { return name_; }
    const InetAddress& localAddress() const { return localAddr_; }
    const InetAddress& peerAddress() const { return peerAddr_; }
    
    bool connected() const { return state_ == kConnected; }

    // 发送数据
    void send(const std::string &buf);
    // 关闭连接
    void shutdown();

    void setConnectionCallback(const ConnectionCallback& cb) 
    {
        connectionCallback_ = cb;
    }

    void setMessageCallback(const MessageCallback& cb)
    {
        messageCallback_ = cb;
    }

    void setWriteCompleteCallback(const WriteCompleteCallback& cb)
    {
        writeCompleteCallback_ = cb;
    }
    void setHighWaterMarkCallback(const HighWaterMarkCallback& cb, size_t highWaterMark)
    {
        highWaterMarkCallback_ = cb;
        highWaterMark_ = highWaterMark;
    }

    void setCloseCallback(const CloseCallback& cb)
    {
        closeCallback_ = cb;
    }

    // 连接建立
    void connectEstablished();
    // 连接销毁
    void connectDestoryed();

    void setState(StateE state) { state_ = state; }


};

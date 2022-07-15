#pragma once
#include "noncopyable.h"
#include "Socket.h"
#include "Channel.h"

#include <functional>

class EventLoop;
class InetAddress;

class Acceptor : noncopyable
{
public:
    using NewConnectionCallback = std::function<void(int sockfd, const InetAddress&)>;
    Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reuseport);
    ~Acceptor();

    void setNewConnectionCallback(const NewConnectionCallback &cb)
    {
        newConnectionCallback_ = cb;
    }

    bool listenning() const { return listenning_; }
    void listen();
private:
    void handleRead();
    
    EventLoop *loop_; // Acceptor用的就是用户定义的那个baseloop，也称作mainloop
    Socket acceptSocket_; 
    Channel acceptChannel_;
    NewConnectionCallback newConnectionCallback_; // 来了一个新连接的回调，把fd打包成channel，通过getNextLoop唤醒一个subLoop, 再把channel分发给相应的loop去监听已连接用户的读写事件
    bool listenning_;
};
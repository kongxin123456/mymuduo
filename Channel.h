#pragma once

# include "noncopyable.h"
# include "Timestamp.h"

# include <functional>
# include <memory>

class EventLoop;
/**
 * @brief 
 * Channel 理解为通道，封装了sockfd和其感兴趣的event，如EPOLLIN、EPOLLOUT事件
 * 还绑定了poller返回的具体时间
 */
class Channel:noncopyable
{
public:
    // typedef std::function<void()> EventCallback;
    // c++11写法
    using EventCallback = std::function<void()>;
    using ReadEventCallback = std::function<void(Timestamp)>;

    Channel(EventLoop *Loop, int fd);
    ~Channel();
    
    // fd得到poller通知以后，处理事件的。调用相应的回调方法
    void handleEvent(Timestamp receiveTime);

    // 设置回调函数对象
    void setReadCallback(ReadEventCallback cb){ readCallback_ = std::move(cb);}
    void setWriteCallback(EventCallback cb) {writeCallback_ = std::move(cb);}
    void setCloseCallback(EventCallback cb) {closeCallback_ = std::move(cb);}
    void setErrorCallback(EventCallback cb) {errorCallback_ = std::move(cb);}
    
    // 防止当channel被手动remove掉，channel还在执行回调操作
    void tie(const std::shared_ptr<void>&);

    int fd() const { return fd_;}
    int events() const { return events_; }
    int set_revents(int revt) { revents_ = revt; }

    // 设置fd相应的事件
    //update就是在调用epoll_ctl添加感兴趣的事件 
    void enableReading() { events_ |= kReadEvent; update(); }
    void disableReading() { events_ &= ~kReadEvent; update(); }
    void enableWriting() { events_ |= kWriteEvent; update(); }
    void disableWriting() { events_ &= ~kWriteEvent; update(); } 
    void disableAll() { events_ = kNoneEvent; update(); }

    // 返回fd当前的事件状态
    bool isNoneEvent() const { return events_ == kNoneEvent;}
    bool isWriting() const { return events_ & kWriteEvent; }
    bool isreading() const {return events_ & kReadEvent; }

    // for Poller
    int index() { return index_; }
    void set_index(int idx) { index_ = idx; }

    // one loop per thread 一个线程有一个EventLoop, 一个EventLoop有一个poller，一个poller上可以监听很多个channel
    // 每个channel都是属于一个EventLoop，一个EventLoop有很多个channel
    EventLoop* ownerLoop() { return loop_; }
    void remove();  //删除channel用的

private:
    
    void update();
    void handleEventWithGuard(Timestamp receiveTime);

    static const int kNoneEvent;
    static const int kReadEvent;
    static const int kWriteEvent;

    EventLoop *loop_;  // 事件循环
    const int fd_;     // fd, Poller监听的对象
    int events_;       // 注册fd感兴趣的事件
    int revents_;      // Poller返回的具体发生的事件
    int index_;
    
    // std::weak_ptr 是一种智能指针，它对被 std::shared_ptr 管理的对象存在非拥有性（“弱”）引用。在访问所引用的对象前必须先转换为 std::shared_ptr
    // 弱智能指针只会观察资源，不能使用资源；弱智能指针没有提供*和->运算符重载，不能将弱智能指针当成裸指针看待。
    // 将弱智能指针提升！使用lock()进行提升，提升之后它也是一个强智能指针
    std::weak_ptr<void> tie_;
    bool tied_;

    // 因为channel通道里面能够获知fd最终发生的具体的事件revents，所以它负责调用具体事件的回调操作
    ReadEventCallback readCallback_;
    EventCallback writeCallback_;
    EventCallback closeCallback_;
    EventCallback errorCallback_;
};
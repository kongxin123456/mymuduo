#include "Channel.h"
#include "EventLoop.h"
#include "Logger.h"

#include <sys/epoll.h>

const int Channel::kNoneEvent = 0;
const int Channel::kReadEvent = EPOLLIN | EPOLLPRI;
const int Channel::kWriteEvent = EPOLLOUT;

Channel::Channel(EventLoop *loop, int fd) : loop_(loop),
                                            fd_(fd),
                                            events_(0),
                                            revents_(0),
                                            index_(-1),
                                            tied_(false)
{
}

Channel::~Channel()
{    
}


// channel的tie什么时候调用？ 一个TcpConnection新连接创建的时候
// TcpConnection和Channel是绑定的，channel收到相应的事件调用的回调都是TcpConnection对象中的方法，
// channel绑定的回调函数中的对象都是当前对象this
// 如果channel在poller上还注册着，但对应的TcpConnection不存在了，就会出错
// 因此对应的TcpConnection对象先用弱智能指针记录着，处理事件之前先提升成强智能指针，如果提升失败就不做任何的回调调用
void Channel::tie(const std::shared_ptr<void>& obj)
{
    tie_ = obj;
    tied_ = true;
}

/**
 * @brief 
 * 当改变channel所表示的fd的events事件后，update负责在poller里面更改fd相应的事件epoll_ctl
 * EventLoop => ChannelList Poller
 */
void Channel::update()
{
    // 通过channel所属的EventLoop调用poller的相应方法，注册events事件
    // add code...
    loop_->updateChannel(this);
}

// EventLoop里面包含了一个vector，vector里面都是channel
// 在channel所属的EventLoop中，把当前的channel删除掉
void Channel::remove()
{
    // add code...
    loop_->removeChannel(this);
}

void Channel::handleEvent(Timestamp receiveTime)
{
    if(tied_){
        std::shared_ptr<void> guard = tie_.lock();
        if(guard)
        {
            handleEventWithGuard(receiveTime);
        }
    }else{
        handleEventWithGuard(receiveTime);
    }
}

// 根据poller通知的channel发生的具体事件，由channel负责调用具体的回调操作
void Channel::handleEventWithGuard(Timestamp receiveTime)
{
    LOG_INFO("channel handleEvent revents:%d\n", revents_);

    if((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN)){
        if(closeCallback_)
        {
            closeCallback_();
        }
    }

    if(revents_ & EPOLLERR){
        if(errorCallback_)
        {
            errorCallback_();
        }
    }

    if(revents_ & (EPOLLIN | EPOLLPRI)){
        if(readCallback_){
            readCallback_(receiveTime);
        }
    }

    if(revents_ & EPOLLOUT){
        if(writeCallback_){
            writeCallback_();
        }
    }


}
# include "EPollPoller.h"
# include "Logger.h"
# include "Channel.h"

# include <errno.h>
# include <unistd.h>
# include <string.h>

// channel的成员变量index表示它在epoll中的状态，初始化为-1
const int kNew = -1; //表示一个channel还没有添加到poller里面
const int kAdded = 1; // 表示一个channel已经添加到poller里面
const int kDeleted = 2; // 表示一个channel已经删除

// 默认情况下子进程会继承父进程所有打开的fd资源
//但是当父进程创建了一个子进程并切换进子进程的时候，设置了EPOLL_CLOEXEC标志的fd会被关闭
EPollPoller::EPollPoller(EventLoop *loop) 
    : Poller(loop)
    , epollfd_(::epoll_create1(EPOLL_CLOEXEC))
    , events_(kInitEventListSize)    // vector<epoll_event>
{
    if (epollfd_ < 0){
        LOG_FATAL("epoll_create error:%d \n", errno);
    }
}

EPollPoller::~EPollPoller(){
    ::close(epollfd_);
}

// EventLoop事件循环 调用 Poller里面的poll方法
// EventLoop里面的ChannelList管理着所有的注册的和未注册的channel，Poller的成员变量ChannelMap里管理着注册了的channel
// EventLoop会创建一个channellist，即activeChannels(地址)传给poll，poll通过epoll_wait监听到那些channel发生了事件，把发生了事件的channel填入传入的activeChannels中
Timestamp EPollPoller::poll(int timeoutMs, ChannelList *activeChannels)
{
    // 实际上用LOG_DEBUG输出日志更为合理，只在调试模式下生效，用LOG_INFO会影响程序效率
    LOG_INFO("func=%s => fd total count:%lu\n", __FUNCTION__, channels_.size());

    int numEvents = ::epoll_wait(epollfd_, &*events_.begin(), static_cast<int>(events_.size()), timeoutMs);
    int saveErrno = errno;
    Timestamp now(Timestamp::now()); 

    if(numEvents > 0){
        LOG_INFO("%d events happened \n", numEvents);
        fillActiveChannels(numEvents, activeChannels);
        if(numEvents == events_.size()){
            events_.resize(events_.size()*2);
        }
    }
    else if(numEvents == 0){
        LOG_DEBUG("%s timeout! \n", __FUNCTION__);
    }
    else{
        if(saveErrno != EINTR)
        {
            errno = saveErrno;
            LOG_ERROR("EPollPoller::poll() err!");
        }
    }
    return now;
}

// channel的update和remove 调用 EventLoop的updateChannel和removeChannel 调用Poller的updateChannel和removeChannel
void EPollPoller::updateChannel(Channel *channel){
    const int index = channel->index();
    LOG_INFO("func=%s => fd=%d events=%d index=%d \n", __FUNCTION__, channel->fd(), channel->events(), index);
    
    if(index == kNew || index == kDeleted)
    {
        if(index == kNew){
            int fd = channel->fd();
            channels_[fd] = channel;
        }

        channel->set_index(kAdded);
        update(EPOLL_CTL_ADD, channel);
    }
    else //channel已经在poller上注册过了
    {
        int fd = channel->fd();
        if (channel->isNoneEvent())
        {
            update(EPOLL_CTL_DEL, channel);
            channel->set_index(kDeleted);
        }
        else
        {
            update(EPOLL_CTL_MOD, channel);
        }

    }
}

// 从poller中删除channel
void EPollPoller::removeChannel(Channel *channel)
{
    int fd = channel->fd();
    channels_.erase(fd);

    LOG_INFO("func=%s => fd=%d \n", __FUNCTION__, fd);

    int index = channel->index();
    if(index == kAdded){
        update(EPOLL_CTL_DEL, channel);
    }
    channel->set_index(kNew);
}

// 填写活跃的连接
void EPollPoller::fillActiveChannels(int numEvents, ChannelList *activeChannels) const
{
    for(int i=0; i < numEvents; i++){
        Channel *channel = static_cast<Channel*>(events_[i].data.ptr);
        channel->set_revents(events_[i].events);
        activeChannels->push_back(channel); // EventLoop就拿到了它的poller给他返回的所有发生事件的channel列表了
    }
}

// 更新channel通道
void EPollPoller::update(int operation, Channel *channel){
    epoll_event event;
    bzero(&event, sizeof event);
    int fd = channel->fd();
    event.events = channel->events();
    event.data.fd = fd;
    event.data.ptr = channel;
    

    if (::epoll_ctl(epollfd_, operation, fd, &event) < 0)
    {
        if(operation == EPOLL_CTL_DEL){
            LOG_ERROR("epoll_ctl del error:%d\n",errno);
        }
        else
        {
            LOG_FATAL("epoll_ctl add/mod error:%d\n", errno);
        }
    }

}
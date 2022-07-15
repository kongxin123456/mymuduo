# pragma once

# include "Poller.h"
# include "Timestamp.h"

# include <vector>
# include <sys/epoll.h>

/**
 * @brief 
 * epoll的使用
 * epoll_create
 * epoll_ctl add/mod/del
 * epoll_wait
 */

class Channel;

class EPollPoller : public Poller
{
public:
    EPollPoller(EventLoop* loop);
    ~EPollPoller() override;  // override表示覆写，让编译器去检查基类里面对应的类是不是虚函数

    // 重写基类Poller的抽象方法
    Timestamp poll(int timeoutMs, ChannelList *activeChannels) override;
    void updateChannel(Channel *channel) override;
    void removeChannel(Channel *channel) override;

private:
    static const int kInitEventListSize = 16; // EventListSize初始的长度

    // 填写活跃的连接
    void fillActiveChannels(int numEvents, ChannelList *activeChannels) const;
    // 更新channel通道
    void update(int operation, Channel *channel);

    // 用来传入epoll实例中的
    // struct epoll_event epevs[1024];
    // int ret = epoll_wait(epfd, epevs, 1024, -1);
    // 这里使用的是vector而不是固定数组，方便扩容，满足实际需求
    using EventList = std::vector<epoll_event>;

    int epollfd_;   // epoll实例的fd
    EventList events_;
};
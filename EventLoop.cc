#include "EventLoop.h"
#include "Logger.h"
#include "Poller.h"
#include "Channel.h"

#include <sys/eventfd.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

// 防止一个线程创建多个EventLoop  thread_local
__thread EventLoop *t_loopInthisThread = nullptr;

// 定义默认的Poller IO复用接口的超时时间
const int kPollTimeMs = 10000;

// 创建wakeup,用来notify唤醒subReactor处理新来的channel
int createEventfd()
{
    int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (evtfd < 0)
    {
        LOG_FATAL("eventfd error:%d \n", errno);
    }
    return evtfd;
}

EventLoop::EventLoop()
    : looping_(false)
    , quit_(false)
    , callingPendingFunctors_(false)
    , threadId_(CurrentThread::tid())
    , poller_(Poller::newDefaultPoller(this))
    , wakeupFd_(createEventfd())
    , wakeupChannel_(new Channel(this, wakeupFd_))
    // , currentActiveChannel_(nullptr)
{
    LOG_DEBUG("EventLoop create %p in thread %d \n", this, threadId_);
    if (t_loopInthisThread) {
        LOG_FATAL("Another EventLoop %p exists in this thread %d \n", t_loopInthisThread, threadId_);
    }
    else{
        t_loopInthisThread = this;
    }

    /**
     * @brief 
     * 1. bind绑定类成员函数时，第⼀个参数表⽰对象成员函数的指针，第⼆个参数表⽰对象的地址。
     * 2. 必须显⽰的指定&Fun::print，因为编译器不会将对象的成员函数隐式转换成函数指针，所以必须在
   Fun::print前⾯添加&。
     * 3. 使⽤对象成员函数的指针时，必须知道该指针属于哪个对象，因此第⼆个参数为对象的地址&fun。
     */
    // 设置wakeupfd的事件类型以及发生事件后的回调函数，wakeupfd最主要的作用就是唤醒subReactor(EventLoop) 
    // 主Loop通过wakeupfd唤醒子Loop
    wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead, this));
    // 每一个eventloop都将监听wakeupchannel的EPOLLIN读事件
    wakeupChannel_->enableReading();
}

EventLoop::~EventLoop()
{
    wakeupChannel_->disableAll();
    wakeupChannel_->remove();
    ::close(wakeupFd_);
    t_loopInthisThread = nullptr;
}

// 开启事件循环
void EventLoop::loop()
{
    looping_ = true;
    quit_ = false;

    LOG_INFO("EventLoop %p start looping\n", this);

    while(!quit_)
    {
        activeChannels_.clear();
        pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_);
        for (Channel *channel : activeChannels_)
        {
            // Poller监听哪些channel发生事件了，然后上报给EventLoop,通知channel处理相应的事件
            channel->handleEvent(pollReturnTime_);
        }
        // 执行当前EventLoop事件循环需要处理的回调操作
        /**
         * @brief Construct a new do Pending Functors object
         * IO 线程mainloop主要做的是接受新用户的连接，即accept的工作，accept会返回一个专门跟客户端通信的fd,然后用一个channel来打包fd.
         * mainLoop会事先注册一个回调cb（需要subloop来执行），唤醒subloop后执行之前mainloop注册的cb操作，这个cb操作有可能是多个，因此是一个vector容器
         */
        doPendingFunctors();
    }

    LOG_INFO("EventLoop %p stop looping. \n", this);
    looping_ = false;
}

// 退出事件循环  1.loop在自己的线程中调用quit  2.在非loop的线程中，调用loop的quit
/**
 *              mainLoop
 * 
 *                                             no ==================== 生产者-消费者的线程安全的队列
 * 
 *  subLoop1     subLoop2     subLoop3
 */ 
void EventLoop::quit()
{
    quit_ = true;

    // 如果是在其它线程中，调用的quit   在一个subloop(woker)中，调用了mainLoop(IO)的quit
    if (!isInLoopThread())  
    {
        wakeup();
    }
}


// 在当前loop中执行cb
void EventLoop::runInLoop(Functor cb)
{
    if (isInLoopThread()) // 在当前的loop线程中，执行cb
    {
        cb();
    }
    else // 在非当前loop线程中执行cb , 就需要唤醒loop所在线程，执行cb
    {
        queueInLoop(cb);
    }
}
// 把cb放入队列中，唤醒loop所在的线程，执行cb
void EventLoop::queueInLoop(Functor cb)
{
    {
        std::unique_lock<std::mutex> lock(mutex_);
        pendingFunctors_.emplace_back(cb);
    }

    // 唤醒相应的，需要执行上面回调操作的loop的线程了
    // || callingPendingFunctors_的意思是：当前loop正在执行回调，但是loop又有了新的回调
    if (!isInLoopThread() || callingPendingFunctors_) 
    {
        wakeup(); // 唤醒loop所在线程
    }
}

void EventLoop::handleRead()
{
    uint64_t one = 1;
    size_t n = read(wakeupFd_, &one, sizeof one);
    if (n != sizeof one)
    {
        LOG_ERROR("EventLoop::handleRead() reads %lu bytes instead of 8", n);
    }
}

// 用来唤醒loop所在的线程的 向wakeupfd_写一个数据, wakeupChannel就发生读事件，当前loop线程就会被唤醒
void EventLoop::wakeup() 
{
    uint64_t one = 1;
    ssize_t n = write(wakeupFd_, &one, sizeof one);
    if (n != sizeof one)
    {
        LOG_ERROR("EventLoop::wakeup() writes %lu bytes instead of 8 \n", n);
    }
}

// EventLoop的方法 调用 Poller的方法
void EventLoop::updateChannel(Channel *channel)
{
    poller_->updateChannel(channel);
}

void EventLoop::removeChannel(Channel *channel)
{
    poller_->removeChannel(channel);
}

bool EventLoop::hasChannel(Channel *channel)
{
    return poller_->hasChannel(channel);
}

void EventLoop::doPendingFunctors() // 执行回调
{
    std::vector<Functor> functors;
    callingPendingFunctors_ = true;

    // 把pendingFunctors_里的回调函数转移到functors里面，
    // 出了括号锁就被释放了，后续回调执行不需要持续占用pendingFunctors_，主loop可以继续分发channel
    {
        std::unique_lock<std::mutex> lock(mutex_);  
        functors.swap(pendingFunctors_);
    }

    for (const Functor &functor : functors)
    {
        functor();  // 执行当前loop需要执行的回调操作
    }

    callingPendingFunctors_ = false;
}

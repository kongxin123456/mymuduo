#include "EventLoopThread.h"
#include "EventLoop.h"

EventLoopThread::EventLoopThread(const ThreadInitCallback &cb,
        const std::string &name)
        : loop_(nullptr)
        , exiting_(false)
        , thread_(std::bind(&EventLoopThread::threadFunc, this), name)
        , mutex_()
        , cond_()
        , callback_(cb)
{

}

EventLoopThread::~EventLoopThread()
{
    exiting_ = true;
    if (loop_ != nullptr){
        loop_->quit();
        thread_.join();
    }
}

EventLoop* EventLoopThread::startLoop()
{
    thread_.start();  // 启动底层的新线程

    EventLoop *loop = nullptr;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        while( loop_ == nullptr)
        {
            cond_.wait(lock);
        }
        loop = loop_;
    }
    return loop;
}

// 下面这个方法是在单独的新线程里面运行的
/**
 * 条件变量都用互斥锁进行保护，条件变量状态的改变都应该先锁住互斥锁，
 * pthread_cond_wait()需要传入一个已经加锁的互斥锁，该函数把调用线程加入等待条件的调用列表中，然后释放互斥锁，
 * 在条件满足从而离开pthread_cond_wait()时，mutex将被重新加锁，这两个函数是原子操作。
 * 
 * 为了避免因条件判断语句与其后的正文或wait语句之间的间隙而产生的漏判或误判，用一个mutex来保证。对于某个cond的判断,修改等操作某一时刻只有一个线程在访问。
 * 条件变量本身就是一个竞争资源，这个资源的作用是对其后程序正文的执行权，于是用一个锁来保护。
 */
void EventLoopThread::threadFunc()
{
    EventLoop loop; // 创建一个独立的eventloop, 和上面的线程是一一对应的 ==》 one loop per thread
    
    if(callback_)
    {
        callback_(&loop);
    }
    {
        std::unique_lock<std::mutex> lock(mutex_);
        loop_ = &loop;
        cond_.notify_one();
    }

    loop.loop(); // 通过EventLoop的loop函数启用Poller.poll()，开启事件循环
    std::unique_lock<std::mutex> lock(mutex_);
    loop_ = nullptr;
}
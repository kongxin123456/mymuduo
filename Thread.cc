#include "Thread.h"
#include "CurrentThread.h"

#include <semaphore.h>

std::atomic_int Thread::numCreated_(0);

Thread::Thread(ThreadFunc func, const std::string &name)
    : started_(false)
    , joined_(false)
    , tid_(0)
    , func_(std::move(func))
    , name_(name)
{
    setDefaultName();
}

void Thread::setDefaultName()
{
    int num = ++numCreated_;
    if(name_.empty()){
        char buf[32] = {0};
        snprintf(buf, sizeof buf, "Thread%d",num);
        name_ = buf;
    }
}


// phread_detach 将线程设置成一个分离线程（守护线程），当主线程结束时守护线程会自动结束
// 与join只能是一个
Thread::~Thread()
{
    if(started_ && !joined_){
        thread_->detach(); // thread类提供的设置分离线程的方法
    }
}

// ==>runInThread
void Thread::start()  // 一个thread对象，记录的就是一个新线程的详细信息
{ 
    started_ = true;
    sem_t sem;
    sem_init(&sem, false, 0);
    // 开启线程
    // [&](){} 以引用的方式接受外部的线程对象
    thread_ = std::shared_ptr<std::thread>(new std::thread([&](){
        // 获取线程的tid值
        tid_ = CurrentThread::tid();
        sem_post(&sem);
        // 开启一个新线程，专门执行该线程函数
        func_();
    }));

    //这里必须等待获取上面新创建的线程的tid值
    sem_wait(&sem);
}

void Thread::join()
{
    joined_ = true;
    thread_->join();
}

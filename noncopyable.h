#pragma once


/**
 * @brief 
 * noncopyable被继承以后，派生类对象可以正常的构造和析构，但是派生类对象无法进行拷贝构造和赋值操作
 * class默认是私有继承，struct默认是公有继承
 */

class noncopyable{
public:
    noncopyable(const noncopyable&) = delete;
    noncopyable& operator=(const noncopyable&) = delete;  
protected:
    noncopyable() = default;
    ~noncopyable() = default;

};
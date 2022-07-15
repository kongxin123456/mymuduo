# pragma once

# include <iostream>

// 时间类
class Timestamp
{
public:
    Timestamp();
    // 声明为explicit的构造函数不能在隐式转换中使用。
    explicit Timestamp(int64_t microSecondsSinceEpoch);
    static Timestamp now();
    std::string toString() const;
private:
    int64_t microSecondsSinceEpoch_;
};
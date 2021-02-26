#pragma once

#include <iostream>
#include <string>
#include <stdint.h>

class TimeStamp{
public:
    TimeStamp();
    //禁止 uint64_t 隐式转换为 TimeStamp
    explicit TimeStamp(uint64_t microSecondsSinceEpochArg);

    static TimeStamp now();
    std::string toString() const;

private:
    uint64_t microSecondsSinceEpoch_;

};
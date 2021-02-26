#include "TimeStamp.h"
#include <time.h>
#include <cstdio>

/**
 * 内部维护了一个 uint64 microSecondsSinceEpoch_
 * 函数：提供static的获取当前时间TimeStamp的函数
 *      提供把内部维护的microSecondsSinceEpoch_转为字符串时间的函数
 * 
*/

TimeStamp::TimeStamp() : microSecondsSinceEpoch_(0){}

TimeStamp::TimeStamp(uint64_t microSecondsSinceEpochArg): microSecondsSinceEpoch_(microSecondsSinceEpochArg){}

TimeStamp TimeStamp::now(){
    return TimeStamp(time(NULL));
}
std::string TimeStamp::toString() const{
    char buff[128] = {0};
    tm *tm_time = localtime((time_t*)&microSecondsSinceEpoch_);
    sprintf(buff, "%4d:%02d%02d %02d:%02d:%02d", 
        tm_time->tm_year + 1900,
        tm_time->tm_mon + 1,
        tm_time->tm_mday,
        tm_time->tm_hour,
        tm_time->tm_min,
        tm_time->tm_sec
        );
    return buff;
}
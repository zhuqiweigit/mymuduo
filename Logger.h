#pragma once

#include "noncopyable.h"
#include <string>

enum LogLevel{
    DEBUG,
    INFO,
    ERROR,
    FATAL
};

class Logger : noncopyable{
public:
    //懒汉式单例模式
    static Logger& getInstance();

    void setLogLevel(LogLevel);

    void log(std::string msg);

private:
    LogLevel logLevel_;
    Logger() = default;
};



#define LOG_INFO(logmsgFormat, ...) \
    do \
    { \
        Logger &logger = Logger::getInstance(); \
        logger.setLogLevel(INFO); \
        char buf[1024] = {0}; \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__); \
        logger.log(buf); \
    } while(0) 

#define LOG_DEBUG(logmsgFormat, ...) \
    do\
    {\
        Logger &logger = Logger::getInstance(); \
        logger.setLogLevel(DEBUG); \
        char buf[1024] = {0}; \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__); \
        logger.log(buf); \
    } while(0)

#define LOG_ERROR(logmsgFormat, ...) \
    do\
    {\
        Logger &logger = Logger::getInstance(); \
        logger.setLogLevel(ERROR); \
        char buf[1024] = {0}; \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__); \
        logger.log(buf); \
    } while(0)

#define LOG_FATAL(logmsgFormat, ...) \
    do\
    {\
        Logger &logger = Logger::getInstance(); \
        logger.setLogLevel(FATAL); \
        char buf[1024] = {0}; \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__); \
        logger.log(buf); \
        exit(1); \
    } while(0)



#include "Logger.h"
#include "TimeStamp.h"
#include <iostream>

/**
 * 内部维护了一个static的logger变量，一个logLevel变量
 * 
 * 使用logger：先获取静态logger对象，然后低啊用Logger::setLogLevel设置日志级别，然后调用log函数打印string信息
 * 
 * 将上述使用logger的流程定义为宏，然后写在.h文件中，方便用户使用
*/


Logger& Logger::getInstance(){
    static Logger logger;
    return logger;
}

void Logger::setLogLevel(LogLevel level){
    logLevel_ = level;
}

void Logger::log(std::string msg){
    switch(logLevel_){
        case INFO: 
            std::cout << "[INFO]"; break;
        case DEBUG:
            std::cout << "[DEBUG]"; break;
        case FATAL:
            std::cout << "[FATAL]"; break;
        case ERROR:
            std::cout << "[ERROR]"; break;
        default:
             break;
    }

    std::cout << TimeStamp::now().toString() << " : " << msg << std::endl;
}
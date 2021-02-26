#pragma once

#include "noncopyable.h"
#include <thread>
#include <memory>
#include <functional>
#include <string>
#include <atomic>

class Thread : noncopyable{
public:
    using ThreadFunction = std::function<void()>;

    explicit Thread(ThreadFunction, const std::string &name = std::string());
    ~Thread();

    void start();
    void join();

    bool isStart()const{
        return started_;
    }
    pid_t tid()const{
        return tid_;
    }
    const std::string& name()const{
        return name_;
    }
    static int numCreate(){
        return numCreated_;
    }

private:
    void setDefaultName();

    bool started_;
    bool joined_;
    std::shared_ptr<std::thread> thread_;
    pid_t tid_;
    ThreadFunction func_;
    std::string name_;
    static std::atomic_int numCreated_;
};
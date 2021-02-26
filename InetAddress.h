#pragma once
#include "noncopyable.h"
//sockAddr_in struct
#include <netinet/in.h>
#include <string>

class InetAddress {

public:
    explicit InetAddress(uint16_t port = 0, std::string ip = "127.0.0.1");
    explicit InetAddress(const sockaddr_in& addr): addr_(addr){};

    std::string toIp() const;
    std::string toIpPort() const;
    uint16_t toPort() const;

    const sockaddr_in* getSockaddr()const {
        return &addr_;
    }
    void setSockaddr(const sockaddr_in& addr){
        addr_ = addr;
    }

private:
/**
 * 匿名union：
 * 1. 可以两个struct共用一块内存
 * 2. 匿名union，可以直接用union内的成员，不用union的名字
 * 
*/
union{
    sockaddr_in addr_;
    sockaddr_in6 addr6_;
};

};
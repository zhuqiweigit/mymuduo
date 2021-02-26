#include "InetAddress.h"
#include <cstring>
//字符串解析
#include <arpa/inet.h>

/**
 * 维护了一个sockaddr_in内部变量
 * 设置了InetAddress转Ip或者Ip:Port字符串，或者转port的函数，实质是由内部的sockaddr_in调用系统函数转换
*/


InetAddress::InetAddress(uint16_t port, std::string ip){
    bzero(&addr_, sizeof addr_);
    addr_.sin_family = AF_INET;
    addr_.sin_addr.s_addr = inet_addr(ip.c_str());
    addr_.sin_port = htons(port);
}

std::string InetAddress::toIp() const{
    char buff[64] = {0};
    ::inet_ntop(AF_INET, &addr_.sin_addr, buff, sizeof buff);
    return buff;
}
    
std::string InetAddress::toIpPort()const{
    char buff[64] = {0};
    ::inet_ntop(AF_INET, &addr_.sin_addr, buff, sizeof buff);
    size_t len = strlen(buff);
    uint16_t port = ntohs(addr_.sin_port);
    sprintf(buff + len, ":%u", port);
    return buff;
};

uint16_t InetAddress::toPort()const{
    return ntohs(addr_.sin_port);
}
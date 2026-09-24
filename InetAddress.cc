#include "InetAddress.h"

#include <strings.h>
#include <string.h>

InetAddress::InetAddress(uint16_t port, std::string ip)
{
    bzero(&addr_, sizeof addr_);
    addr_.sin_family = AF_INET;
    addr_.sin_port = htons(port);
    // inet_addr 要求参数为 char* 类型，而ip是string，需要转换
    addr_.sin_addr.s_addr = inet_addr(ip.c_str());
}

std::string InetAddress::toIP() const
{
    char buf[64] = {0};
    ::inet_ntop(AF_INET, &addr_.sin_addr.s_addr, buf, sizeof buf);
    return buf;
}
std::string InetAddress::toIpPort() const
{
    char buf[64] = {0};
    ::inet_ntop(AF_INET, &addr_.sin_addr.s_addr, buf, sizeof buf);
    // 计算 ip 的长度，例如ip == 127.0.0.1,那么buf的内容为“172.0.0.1\0”
    // end == 9
    size_t end = strlen(buf);
    uint16_t port = ntohs(addr_.sin_port);
    // 从 buf+end 指向字符串的末尾 \0 位置开始写入:8080
    sprintf(buf+end, ":%u", port);
    return buf;
}
uint16_t InetAddress::toPort() const
{
    return ntohs(addr_.sin_port);
}

// #include <iostream>
// int main(){
//     InetAddress addr(8080);
//     std::cout << addr.toIpPort() << std::endl;
//     return 0;
// }
/**
 * 基础工具类的实现
 */

#ifndef UTILS_H
#define UTILS_H

#include <vector>
#include <string>
#include <sstream>
#include <random>

/**
 * @brief 通用工具类和功能
 * 提供项目中常用的工具函数，包括字符串处理、时间获取、随机数生成等
 */

namespace Utils
{
    inline std::vector<std::string> split(const std::string &str, char delimit)
    {
        std::vector<std::string> tokens;
        std::string token;
        std::istringstream iss {str};

        while (std::getline(iss, token, delimit))
        {
            // 过滤空字符串
            if(!token.empty())
                tokens.push_back(token);
        }
        
        return tokens;
    }
}

/**
 * @brief 生成指定长度的随机字符串
 * @bug 对于头文件中定义的函数，必须带有inline
 */
inline std::string generateRandomString(size_t length)
{
    // 局部静态变量，字符集
    static const char charset[] = 
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
    
    // 使用线程安全的随机数生成器
    static thread_local std::mt19937 rng { std::random_device{}() };
    // std::random_device{}生成真随机数设备，()调用生成真随机数种子，才初始化随机数引擎，mt19937是一种高效地伪随机数生成算法
    static thread_local std::uniform_int_distribution<size_t> dist { 0, sizeof(charset)-2 };
    // 这是均匀分布的随机数分布器

    std:: string result;
    result.reserve(length);

    for(size_t i = 0; i < length; ++i)
    {
        result += charset[dist(rng)];   // 需要使用随机数分布器+随机数引擎才能得到随机数
    }

    return result;
}

#endif // UTILS_H
/**
 * 基础工具类的实现
 */

#ifndef UTILS_H
#define UTILS_H

#include <vector>
#include <string>
#include <sstream>
#include <random>
#include <chrono>

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

inline std::string randomString(size_t length)
{
    static const char charsets[] = 
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
    
    // 我需要定义C++11线程安全的随机数生成器
    // 伪随机数生成器，需要使用设备随机数提供的随机数种子进行实例化
    static thread_local std::mt19937 random_engine(std::random_device{}()); 
    // 定义生成器范围
    static thread_local std::uniform_int_distribution<size_t> dist(0, sizeof(charsets)-2);

    std::string result;
    result.reserve(length);

    for(size_t idx = 0; idx < length; ++idx)
    {
        result += charsets[dist(random_engine)];
    }

    return result;
}

/**
 * @brief 获取当前时间戳（毫秒）
 * 用途：记录连接创建时间、计算连接使用时长等
 */
inline int64_t currentTimeMillis() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

/**
 * @brief 获取当前时间戳
 * 用途：精确地性能测量
 * 
 * @question 什么时候使用system_clock，什么时候使用steady_clock
 */
inline int64_t currentTimeMicros() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();

}

// 判断是否超时
#if 0
bool waitForCondition(int timeoutMs)
{
    auto start = std::chrono::steady_clock().now();
    // 定义为毫秒为单位的时间段
    auto timeout = std::chrono::milliseconds(timeoutMs);

    while(!metCondition())
    {
        if(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start) > timeout)
            return false;
    }

    return true;
}
#endif

/**
 * @brief 将任意类型的值转换为字符串
 * 这里需要使用到<< >> 进行转换
 */
template<typename T>
inline std::string toString(const T& value)
{
    std::ostringstream oss;
    oss << value;
    return oss.str();
}

/**
 * @brief MySQL专用的字符串转义函数
 * 主要就是转义字符需要注意
 */
inline std::string escapeMySQLString(const std::string &str)
{
    // 为了提高性能，都应该提前预留足够的内存空间
    std::string escaped;
    escaped.reserve(str.size() * 2);

    // 一个一个浏览字符，将需要进行转换的字符都进行修改
    for(char c : str)
    {
        switch (c)
        {
        /**
         * 理解：
         * 1）O'relly ---SQL---> O\'relly ---C++---> O\\'relly
         * case '\''，这样在C++中才能表示字符'
         * 2) O"relly ---SQL---> O\"relly ---C++---> O\\\"relly
         * 3) \n ---SQL---> \n ---C++---> \\n
         * 4) \ ---SQL---> \\ ---C++---> \\\\
         */
        case '\0': escaped += "\\0"; break;     // NULL字符
        case '\n': escaped += "\\n"; break;     // 换行符
        case '\r': escaped += "\\r"; break;     // 回车符
        case '\\': escaped += "\\\\"; break;    // 反斜杠
        case '\'': escaped += "\\'"; break;     // 单引号；最重要的
        case '"': escaped += "\\\""; break;     // 双引号，这一行是为什么呢？
        case '\x1a': escaped += "\\Z"; break;   // Ctrl + Z
        case '\t': escaped += "\\t"; break;     // 制表符
        case '\b': escaped += "\\b"; break;     // 退格符
        default: escaped += c; break;
        }
        /**
         * 之所以转义，就是因为这些转义后的字符串先通过C++输出为最初的SQL语句
         * 再由SQL解析器解释成具体的SQL命令，因此需要在最开始的时候进行转义
         */
    }

    return escaped;
}

/**
 * @brief 构建安全的SQL查询字符串
 */
inline std::string quoteMySQLString(const std::string &value)
{
    return "'" + escapeMySQLString(value) + "'";
}

/**
 * 格式化字节大小为人类可读的字符串
 * 1.5 KB   2.3MB
 */
inline std::string formatBytes(uint64_t bytes)
{
    static const char *units[] = {"B", "KB", "MB", "GB", "TB"};
    size_t unit = 0;
    double size = static_cast<double>(bytes);

    while(size >= 1024.0f && unit < 4)
    {
        size /= 1024;
        ++unit;
    }

    std::ostringstream oss;
    oss.precision(1);
    oss << std::fixed << size << " " << units[unit];
    return oss.str();
}

/**
 * @brief 去除字符串的首尾空白字符
 */
inline std::string trim(const std::string &str)
{
    auto first = str.find_first_not_of(" \t\n\r\f\v");
    if(first == std::string::npos)
    {
        return "";
    }

    size_t end = str.find_last_not_of(" \n\t\f\v\r");
    return str.substr(first, end - first + 1);
}

}   // namespace Utils

#endif // UTILS_H
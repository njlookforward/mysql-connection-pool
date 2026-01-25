/**
 * @brief 实现logger.h日志系统
 */
#ifndef LOGGER_H
#define LOGGER_H

#include <iostream>
#include <fstream>
#include <sstream> // 一旦涉及到格式化，自动添加时间戳和日志级别，说明需要使用ostringstream进行格式化
#include <mutex>
#include <string>
#include <chrono> // 只要涉及到时间戳，cpp中肯定是用到的就是chrono
#include <exception>
#include <iomanip>
#include <ctime>

/**
 * @brief 日志级别枚举
 * 级别越高，输出的日志越少
 */
enum class LogLevel
{
    DEBUG = 0,   // 调试信息：详细的程序执行信息
    INFO = 1,    // 一般信息：程序正常运行信息
    WARNING = 2, // 警告信息：潜在问题，但是不影响运行
    ERROR = 3,   // 错误信息：出现错误，但是程序可以继续运行
    FATAL = 4    // 致命错误：严重错误，程序可能崩溃
};

/**
 * @brief 日志器的定义，根据日志级别，将日志信息输出到指定位置，包括控制台和文件；
 *
 * 特点：线程安全的日志类，同时使用单例模式
 * 1）单例模式：避免资源重复，例如保证每一个日志文件被一个日志器对象绑定，一个ofstream输出到一个日志文件中；
 *  不使用单例模式，可能导致多个日志器实例输出到一个文件资源中。两个独立的ofstream同时写同一个文件，会导致数据竞争、文件指针冲突
 * claude: 单例模式保证只有一个Logger实例，避免多个Logger实例写同一个文件；换句话说，单例模式避免多个Logger实例竞争同一个文件资源
 *
 * 2）线程安全：这是通过互斥锁保证的，互斥锁保证每个操作是线程安全的，防止日志内容交错；避免的是多个线程同时调用同一个Logger导致日志内容混乱的问题
 *
 * 官方文档
 * 1）单例模式：保证全局唯一实例，避免资源冲突
 * 2）线程安全：多线程环境下安全使用，不会出现输出内容交错
 * 3）灵活输出：可以设置同时输出到文件跟控制台
 * 4）格式化：自动添加时间戳跟日志级别
 */
class Logger
{
public:
    /**
     * @brief 静态成员函数，得到Logger对象的唯一实例
     */
    static Logger &getInstance()
    {
        // C++11之后保证静态数据成员在系统层面上保证线程安全，不会创建出多个实例
        static Logger instance;
        return instance;
    }

    /**
     * @brief 初始化日志器实例
     * 因为按照单例模式设计日志器，所以仅仅是定义了日志器，但是没有完成初始化工作
     * 所以需要单独进行初始化设置日志级别，输出文件的路径，是否同时输出到控制台
     *
     * @note 只要是静态资源的设置或者访问，都应该加锁互斥访问，而且这里面应该采用双重判断
     * @param 对于拥有默认参数的函数来说，越需要自己指定的参数，越要放在左面；越是固定的基本不变的参数，越要放在右边 ###
     */
    void init(const std::string &filename = "", LogLevel level = LogLevel::INFO, bool toConsole = true)
    {
        // 判断m_initilized是否为true
        if (m_initialized)
        {
            return;
        }
        std::unique_lock<std::mutex> lock(m_mutex);
        if (m_initialized)
        {
            lock.unlock();
            return;
        }
        // 设置日志级别，必须是枚举值中所拥有的枚举值，否则的话日志级别是非法的，应该报错
        switch (level)
        {
        case LogLevel::DEBUG:
        case LogLevel::INFO:
        case LogLevel::WARNING:
        case LogLevel::ERROR:
        case LogLevel::FATAL:
            m_level = level;
            break;
        default:
            // 这里直接使用cerr打印报错就行，然后直接返回；我应该进行抛出异常，这能够帮助定位错误发生的地方
            throw std::logic_error(std::to_string(static_cast<int>(level)) + " don't belong to LogLevel! Please input correct LogLevel!");
            return;
        }
        // 打开日志文件，绑定到对应的文件输出流中
        // ### BUG 这里需要验证日志文件名字是否为空，如果非空，才能打开文件路径
        if (!filename.empty())
        {
            m_fileStream.open(filename, std::ios::app);
            // 验证是否成功打开日志文件，否则需要抛出运行时异常
            if (!m_fileStream.is_open())
            {
                throw std::runtime_error(filename + " cannot open normally, please check filePath is right or not!");
            }
        }
        // 控制台输出控制
        m_toConsole = toConsole;
        // 完成初始化
        m_initialized = true;

        // 在控制台记录初始化信息
        // std::cout << "Logger init complete! logger level is " << levelToString(level)
        //           << ", logfile is " << (filename.empty() ? "none" : filename) << std::endl;

        // 可以利用string的加法，日志多使用键值对形式，这样更加清晰
        std::cout << "Logger initilized! level = " + levelToString(level) +
                         ", logFile = " + (filename.empty() ? "none" : filename)
                  << std::endl;
        return;
    }

    /**
     * 如果在初始化函数中打开了文件资源，那么在析构函数中应该正确关闭文件
     */
    ~Logger()
    {
        if (m_fileStream.is_open())
        {
            // 因为Logger创建在全局静态区，是一个静态局部变量，因此他的生命周期是与整个程序一致的
            // 只有当程序结束的时候，才会调用析构函数，而且只会被调用一次，因此不存在多线程竞争问题，因此不需要加锁
            m_fileStream.close();
        }
    }

    /**
     * @brief 记录调试日志
     */
    void debug(const std::string &message)
    {
        log(LogLevel::DEBUG, message);
    }

    /**
     * @brief 记录信息日志
     */
    void info(const std::string &message)
    {
        log(LogLevel::INFO, message);
    }

    /**
     * @brief 记录警告日志
     */
    void warning(const std::string &message)
    {
        log(LogLevel::WARNING, message);
    }

    /**
     * @brief 记录错误日志
     */
    void error(const std::string &message)
    {
        log(LogLevel::ERROR, message);
    }

    /**
     * @brief 记录致命错误日志
     */
    void fatal(const std::string &message)
    {
        log(LogLevel::FATAL, message);
    }

    /**
     * @brief 更改日志级别
     * 对于共享资源的访问或者修改，必须加锁访问
     */
    void setLevel(LogLevel level)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_level = level;
    }

    /**
     * @brief 更改是否输出到控制台
     * @note 需要加锁线程安全地修改
     */
    void setToConsole(bool toConsole)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_toConsole = toConsole;
    }

    /**
     * @brief 读取当前的日志级别
     * 只要加锁，那么m_mutex的值就已经发生了改变，因此在const成员函数中，m_mutex必须是mutable属性
     */
    LogLevel getLevel() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_level;
    }

    /**
     * @brief 日志的数字到字符串的转换
     * 我认为这个函数也应该定义为public类型，万一之后会用到呢
     */
    std::string levelToString(LogLevel level) const
    {
        // 如果可以使用switch的话，说明强枚举类型可以直接使用operator=运算符进行比较
        switch (level)
        {
        case LogLevel::DEBUG:
            return "DEBUG";
        case LogLevel::INFO:
            return "INFO";
        case LogLevel::WARNING:
            return "WARNING";
        case LogLevel::ERROR:
            return "ERROR";
        case LogLevel::FATAL:
            return "FATAL";
        default:
            return "UNKNOWN";
        }
    }

private:
    /**
     * 单例模式需要删除拷贝构造函数和拷贝赋值运算符函数
     */
    Logger(const Logger &) = delete;
    Logger &operator=(const Logger &) = delete;

    /**
     * @bug 因为先定义了拷贝构造函数，即使是delete拷贝构造函数，也已经定义了构造函数，因此此时没有默认构造函数了，需要自己定义
     * 否则，在getInstance函数中就会出错，因为无法找到默认构造函数的定义，编译器没有自动生成
     */
    Logger() = default;
    
    /**
     * @brief 统一记录日志的方法
     * 
     * @note
     * 1）判断日志级别
     * 2）多线程安全，需要加锁记录
     * 3) 需要格式化日志信息，添加时间戳与日志级别
     * 4）需要根据需求是否输出到控制台和日志文件
     */
    void log(LogLevel level, const std::string &message)
    {
        // 判断日志级别，编译器针对枚举类型应该重载了运算符
        // @note 我认为这里不需要加锁，因为仅仅是读，基本不会影响判断结果，因此可以减少临界区
        if(level < m_level)
            return;
        // 满足日志级别，格式化日志内容
        std::string formattedMsg = formatMessage(level, message);
        // 这里开始临界区，因为需要更改共享资源，所以需要加锁
        std::unique_lock<std::mutex> lock(m_mutex);
        // 判断是否输出到日志文件
        if(m_fileStream.is_open())
        {
            m_fileStream << formattedMsg << std::endl;
        }
        // 判断是否输出到控制台
        if(m_toConsole)
        {
            // 如果是ERROR或者FATAL级别，说明是错误，那么就需要使用cerr
            if(level == LogLevel::ERROR || level == LogLevel::FATAL)
                std::cerr << formattedMsg << std::endl;
            else 
                std::cout << formattedMsg << std::endl;
        }
        // 解锁
        lock.unlock();
        return;
    }

    /**
     * @brief 格式化日志信息
     * @param level 日志级别
     * @param message 原始信息
     * @retval 格式化后的信息
     * 添加时间戳，日志级别等信息
     */
    std::string formatMessage(LogLevel level, const std::string &message)
    {
        // 格式化信息要用到ostringstream
        std::ostringstream oss;
        // 添加时间戳信息
        // 1. 根据系统时钟得到当前时间点
        auto now = std::chrono::system_clock::now();    // 这是时间点time_point
        // 时间戳就是当前时间点距离1970.1.1的秒数，根据静态成员函数将time_point类型转换为time_t类型
        time_t now_time_t = std::chrono::system_clock::to_time_t(now);
        // 得到当前时间的微秒数，这样符合chrono库的设计理念，保证类型安全与一致性，返回的是milliseconds数据类型
        // 因为duration重载了运算符，1000会被隐式转换为milliseconds类型，因此这样得到的值保留了时间的单位，如果
        // 后面仍然需要与milliseconds数据进行运算更方便
        auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()
        ) % 1000;

        // localtime将纪元时间转换为timestamp数据类型，方便输出为日历时间
        // 但是输出运算符没有重载milliseconds数据类型，因此需要转换得到整数类型
        oss << "[" << std::put_time(std::localtime(&now_time_t), "%Y-%m-%d %H:%M:%S");
        // 这里需要对微秒数的输出格式进行设置
        oss << "." << std::setfill('0') << std::setw(3) << now_ms.count() << "]";
        // 添加日志级别
        oss << " [" << levelToString(level) << "] ";
        // 添加原始日志信息
        oss << message;
        // 返回
        return oss.str();
    }

private: 
/**
    * @attention 这里一定要使用关键字mutable
    * 因为这样在const成员函数中，仍然可以使用mutex互斥访问共享资源，一旦加锁或者解锁，那么mutex的值必然发生改变，那么在const成员函数中，数据成员定义为mutable才能发生改变，否则会报错
    */
    mutable std::mutex m_mutex;
    LogLevel m_level = LogLevel::INFO; // C++11之后数据成员可以提供默认值
    std::ofstream m_fileStream;        // 输出文件流到指定的日志文件    
    bool m_toConsole = true;           // 是否同步输出到控制台
    bool m_initialized = false;        // 是否完成初始化，对于使用单例模式的类来说，如果需要进行初始化，那么应该添加是否进行初始化的标志位
};

// 一般定义为宏函数来使用日志器
// ### 因为是宏函数，是文本替换，因此不需要添加;作为语句的结束，之后在使用的过程中会添加;的
#define LOG_DEBUG(msg) Logger::getInstance().debug(msg)
#define LOG_INFO(msg) Logger::getInstance().info(msg)
#define LOG_WARNING(msg) Logger::getInstance().warning(msg)
#define LOG_ERROR(msg) Logger::getInstance().error(msg)
#define LOG_FATAL(msg) Logger::getInstance().fatal(msg)

#endif // LOGGER_H
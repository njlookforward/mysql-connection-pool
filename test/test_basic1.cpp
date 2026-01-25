#include <cassert>
#include <thread>
#include "utils.h"
#include "logger.h"


/**
 * @brief 第一天基础功能测试
 * 测试工具类和日志系统的基本功能
 */
void testMySQLEscape()
{
    // @note 第一部分的测试使用===开始；内部的子部分的测试使用---开始，这样方便区分
    std::cout << "\n--- 测试MySQL字符串转义 ---" << std::endl;
    // 将普通的mysql语句转换为MySQL的合格的输入语句，然后再按照C++的方式转义

    // 函数内部定义测试用例格式
    struct TestCase
    {
        std::string input;          // 输入语句
        std::string description;    // 语句描述，包含什么特征
    };

    // 测试用例集合
    // 转义的顺序：普通文本字符串 --> 转义后的SQL字符串 --> 转义后的C++字符串
    std::vector<TestCase> testCases = {
        {"Normal text", "普通文本"},
        {"It's a 'test' with \"quotes\"", "混合引号"},
        {"'; DROP TABLE Users; --", "SQL尝试注入"},
        {"C:\\Programs\\MySQL", "Windows路径"},
        {"Line1\nLine2\tTabbed", "特殊字符"},
        {"用户名：张三", "中文字符"},
        {"", "空字符串"}
    };

    std::cout << "根据不同类型的字符串，打印出转义后的SQL字符串:" << std::endl;
    for(const TestCase &tc : testCases)
    {
        std::cout << "  " << tc.description << " : " << Utils::quoteMySQLString(tc.input) << std::endl;
    }

    std::cout << "  MySQL字符串转义测试通过!" << std::endl;
}

/**
 * @brief 进行Utils工具类的基准测试
 */
void testUtils()
{
    std::cout << "\n=== 测试Utils工具类 ===" << std::endl;

    // 测试字符串分割函数
    std::vector<std::string> tokens = Utils::split("hello,world,test", ',');
    // 多多使用assert断言进行测试，更加方便
    assert(tokens.size() == 3);
    assert(tokens[0] == "hello");
    assert(tokens[1] == "world");
    assert(tokens[2] == "test");
    std::cout << "Utils::split 字符串分割函数测试通过" << std::endl;

    // 测试随机字符串生成函数
    size_t randomLen = 10;
    std::string randomStr1 = Utils::generateRandomString(randomLen);
    std::string randomStr2 = Utils::generateRandomString(randomLen);
    assert(randomStr1.length() == randomLen);
    assert(randomStr2.length() == randomLen);
    assert(randomStr1 != randomStr2);
    std::cout << "Utils::generateRandomString 随机字符串生成函数测试通过: " 
              << "str1=" << randomStr1 << ", str2=" << randomStr2
              << std::endl;

    // 测试获取时间戳函数
    int64_t timestamp1 = Utils::currentTimeMillis();
    std::this_thread::sleep_for(std::chrono::milliseconds(10)); // 延长10ms，确保前后时间戳不同
    int64_t timestamp2 = Utils::currentTimeMillis();
    assert(timestamp2 > timestamp1);
    std::cout << "Utils::currentTimeMills 获取时间戳函数测试通过："
              << "timestamp1=" << timestamp1 << "   --->    "
              << "timestamp2=" << timestamp2 << std::endl;
    
    // 测试类型转换函数
    double num = 123.45;
    std::string numStr = Utils::toString(num);
    assert(numStr == "123.45");
    std::cout << "Utils::toString 类型转换函数测试通过：" << numStr << std::endl;

    // 测试SQL字符串转移函数
    testMySQLEscape();
    // 测试字节数格式化函数
    std::string formatted = Utils::formatBytes(1536);   // 1.5KB
    std::cout << "字节数格式化函数测试通过：" << "1536B = " << formatted << std::endl;


    // 测试字符串修剪函数
    std::string trimmed = Utils::trim("  hello world  ");
    assert(trimmed == "hello world");
    std::cout << "Utils::trim 字符串修剪函数测试通过：'" << trimmed << "'" << std::endl;
}

/**
 * @brief 日志类所有功能的基准测试
 */
void testLogger()
{
    std::cout << "\n=== 测试Logger日志系统 ===" << std::endl;
    Logger &logger = Logger::getInstance();
    // ###BUG 这里的相对路径应该相对的是工作目录
    logger.setLevel(LogLevel::DEBUG);

    logger.debug("这是一条调试信息");
    logger.info("这是一条普通信息");
    logger.warning("这是一条警告信息");
    logger.error("这是一条错误信息");
    logger.fatal("这是一条致命错误信息");

    std::cout << "日志基本输出测试通过！" << std::endl;

    std::cout << "\n--- 设置日志级别为INFO，DEBUG信息不会显示 ---" << std::endl;
    logger.setLevel(LogLevel::INFO);
    logger.debug("调试信息不会显示");
    logger.info("普通信息会显示");
    
    std::cout << "日志级别过滤测试通过" << std::endl;

    std::cout << "\n--- 测试日志宏定义 ---" << std::endl;
    LOG_INFO("使用宏定义记录日志");
    LOG_WARNING("使用宏定义记录告警信息");

    std::cout << "日志宏定义测试通过" << std::endl;
}

/**
 * @brief 多线程环境下日志类的基准测试
 */
void testMultiThreadLogger()
{
    std::cout << "\n=== 测试多线程日志系统的安全性，共5个线程，每个线程输出3条语句 ===" << std::endl;
    // Logger &logger = Logger::getInstance();
    // 因为上面的testLogger函数已经完成了init初始化工作，因此这里的Init函数其实是没有任何作用的；
    // ###BUG 因此对于logger的初始化工作，应该在main函数就应该完成，而不是在各个被调用的函数中完成
    // 准确来说，logger的初始化工作，应该在预热阶段完成
    // logger.init("./docs/test_day1.log");    

    std::vector<std::thread> threads;
    for(int i = 0; i < 5; ++i)
    {
        threads.emplace_back([i](){
            for(int j = 0; j < 3; ++j)
            {
                LOG_INFO("线程 " + Utils::toString(i) + " 第 " + Utils::toString(j) + " 条日志");
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        });
    }

    // 回收线程池中的每一个线程
    for(auto &thread : threads)
    {
        thread.join();
    }

    std::cout << "多线程日志系统安全性测试通过" << std::endl;
}

/**
 * @brief 性能基准测试
 */
void testPerformance()
{
    std::cout << "\n=== 性能基准测试 ===" << std::endl;

    auto start = std::chrono::high_resolution_clock{}.now();
    for(int i = 0; i < 10000; ++i)
    {
        Utils::generateRandomString(16);
    }
    auto end = std::chrono::high_resolution_clock{}.now();
    // ###BUG 应该是duration_cast
    auto elapse = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << "生成10000个长度为16的随机字符串花费 " << elapse.count() << "us" << std::endl;

    // ###BUG 设计的不合理，的确路径应该是第一个参数，而不是日志级别，越需要自定义的越需要放在左边
    // Logger::getInstance().init("./docs/test_day1.log");
    Logger::getInstance().setToConsole(false);
    start = std::chrono::high_resolution_clock().now();
    for(int i = 0; i < 1000; ++i)
    {
        LOG_INFO("性能测试日志信息 " + Utils::toString(i));
    }
    end = std::chrono::high_resolution_clock().now();
    elapse = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << "同时输出1000条日志信息到日志文件与控制台共花费 " << elapse.count() << "us" << std::endl;

    std::cout << "性能基准测试通过" << std::endl;
}

int main()
{
    std::cout << "=== 开始第1天的基准测试 ===" << std::endl;

    // 使用try—catch来捕获异常，正确的测试态度
    try
    {
        testUtils();

        // 应该在主程序的一开始处就完成Logger的初始化操作,其中的相对路径相对的是开始执行程序的当前路径
        Logger::getInstance().init("./docs/test_day1.log");
        testLogger();
        testMultiThreadLogger();
        testPerformance();
    }
    catch(const std::exception &e)
    {
        std::cerr << e.what() << std::endl;
        return 1;
    }

    std::cout << "恭喜我自己！完成了第一天的学习计划！我已经成功搭建了项目基础框架，实现了工具类和日志系统，并完成了对应的测试" << std::endl
              << "明天我将实现数据库连接封装!" << std::endl;

    return 0;
}
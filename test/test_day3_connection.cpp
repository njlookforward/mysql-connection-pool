#include <iostream>
#include <string>
#include <vector>
#include <utility>
#include <thread>
#include "connection.h"
#include "db_exception.h"

/**
 * @brief 第3天重连功能核心测试
 *
 * 重点验证：
 * 1. 错误码识别功能
 * 2. 自动重连机制
 * 3. 带重连的查询系统
 * 4. 重连统计功能
 * 5. 异常处理
 */

// 连接参数
const std::string TEST_HOST = "localhost"; // 主机名
const std::string TEST_USER = "admin";
const std::string TEST_PASSWORD = "123456";
const std::string TEST_DATABASE = "testdb";
const unsigned int TEST_PORT = 3306;

// 打印每个测试的标题头
void printTestHeader(const std::string &title)
{
    std::cout << "\n"
              << std::string(50, '=') << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout << std::string(50, '=') << std::endl;
}

bool testErrorCodeRecognition()
{
    printTestHeader("测试错误码识别");

    try
    {
        // 我需要先建立连接，才能通过连接对象验证是不是连接错误
        Connection conn{TEST_HOST, TEST_USER, TEST_PASSWORD, TEST_DATABASE, TEST_PORT};

        // 这是一个未命名的类型的数组
        struct
        {
            unsigned int code;
            bool isConnectionError;
        } tests[] = {
            {2002, true},  // CR_CONNECTION_ERROR
            {2006, true},  // CR_SERVER_GONE_ERROR
            {2013, true},  // CR_SERVER_LOST
            {1045, false}, // ER_ACCESS_DENIED_ERROR
            {1146, false}, // ER_NO_SUCH_TABLE
            {1064, false}  // ER_PARSE_ERROR
        };

        for (const auto &test : tests)
        {
            bool result = conn.isConnectionError(test.code);
            std::cout << "错误码识别 " << test.code
                      << (result == test.isConnectionError ? " 正确" : " 错误") << std::endl;
        }

        std::cout << "初始重连统计 - 尝试：" << conn.getTotalReconnectAttempts()
                  << ", 成功 " << conn.getSuccessfulReconnects() << std::endl;

        return true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "错误码识别验证失败：" << e.what() << '\n';
        return false;
    }
}

bool testBasicReconnection()
{
    printTestHeader("测试基础重连功能");

    try
    {
        // 验证初始连接，我应该设置较短的重连间隔方便测试
        std::cout << "1. 建立初始连接" << std::endl;
        Connection conn{TEST_HOST, TEST_USER, TEST_PASSWORD, TEST_DATABASE, TEST_PORT, 300U, 3U};
        if (conn.connect())
        {
            std::cout << "初始连接建立成功" << std::endl;
        }
        else
        {
            std::cerr << "初始连接建立失败" << std::endl;
            return false;
        }

        // 验证连接有效性
        std::cout << "2. 验证连接有效性" << std::endl;
        if (conn.isValid(true)) // 带重连的验证有效性
        {
            std::cout << "连接有效性验证通过" << std::endl;
        }
        else
        {
            std::cout << "连接有效性验证失败" << std::endl;
            return false;
        }

        // 3. 验证主动重连，而且需要计算重连需要的时间
        std::cout << "3. 验证主动重连" << std::endl;
        auto start = std::chrono::steady_clock::now();
        if (conn.reconnect())
        {
            std::cout << "主动重连成功" << std::endl;
        }
        else
        {
            std::cout << "主动重连失败" << std::endl;
            return false;
        }
        auto end = std::chrono::steady_clock::now();
        std::chrono::microseconds duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        // ###BUG 这里又忘记了，duration.count才能转换成为整数进行输出，duration没有对应的输出运算符重载
        std::cout << "主动重连花费 " << duration.count() << " us" << std::endl;

        // 4. 验证重连后查询等功能有效
        std::cout << "4. 验证重连后可以有效执行重连操作" << std::endl;
        QueryResultPtr result = conn.executeQuery("SELECT 1 as test_value");
        // next -> getInt
        if (result->next() && result->getInt("test_value") == 1)
        {
            std::cout << "重连后可以有效执行查询操作" << std::endl;
        }
        else
        {
            std::cout << "重连后无法有效执行查询操作" << std::endl;
        }

        std::cout << "初始重连统计 - 尝试：" << conn.getTotalReconnectAttempts()
                  << ", - 成功：" << conn.getSuccessfulReconnects() << std::endl;

        return true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "基础重连功能测试失败：" << e.what() << '\n';
        return false;
    }
}

bool testQueryWithReconnect()
{
    printTestHeader("带重连的查询测试");

    // 0. 建立连接，仍然是缩短重连的间隔时间，应该在最外围使用try-catch
    try
    {
        Connection conn{TEST_HOST, TEST_USER, TEST_PASSWORD, TEST_DATABASE, TEST_PORT, 200U, 2U};
        if (conn.connect())
        {
            std::cout << "0. 建立初始连接" << std::endl;
        }
        else
        {
            std::cout << "建立初始连接失败" << std::endl;
            return false;
        }

        // 1. 进行正常查询，注意每个单元测试需要自已负责进行错误处理，之所以在这个测试单元，异常处理分为多个，是因为存在不同的异常类
        try
        {
            std::cout << "1. 带重连查询操作测试" << std::endl;
            // 查询连接ID跟当前时间点
            QueryResultPtr result = conn.executeQuery("SELECT CONNECTION_ID() as conn_id, NOW() as now");
            if (result->next())
            {
                std::cout << "MySQL连接Id=" << result->getInt("conn_id") << ", timestamp=" << result->getString("now") << std::endl;
            }
            else
            {
                std::cout << "普通查询失败" << std::endl;
            }
        }
        catch (const db::SQLExecutionError &e)
        {
            std::cerr << "查询失败：" << e.what() << "(错误码 " << e.getErrorCode() << ")" << '\n';
            return false;
        }

        // 2. 进行插入操作，创建一个测试表
        try
        {
            std::cout << "2. 带重连的update操作测试" << std::endl;
            const std::string createTableSql = R"(
            CREATE TABLE IF NOT EXISTS test_reconnect (
            id INT AUTO_INCREMENT PRIMARY KEY,
            name VARCHAR(50)
            )
        )";
            unsigned long long affected = conn.executeUpdate(createTableSql);
            std::cout << "创建测试表test_reconnect共影响 " << affected << " 行数据(应该是0行)" << std::endl;

            // 插入两行数据
            affected = conn.executeUpdate("INSERT INTO test_reconnect (name) VALUES ('test1'), ('test2')");
            std::cout << "插入" << affected << " 行数据(2行)" << std::endl;
            // 查询插入的行数，我不知道COUNT()应该传入什么参数
            QueryResultPtr result = conn.executeQuery("SELECT COUNT(*) as count FROM test_reconnect");
            if (result->next())
            {
                std::cout << "通过查询，验证共插入了" << result->getInt("count") << "行数据(4行)" << std::endl;
            }
        }
        catch (const db::SQLExecutionError &e)
        {
            std::cerr << "update测试失败：" << e.what() << "（错误码 " << e.getErrorCode() << ")" << '\n';
            return false;
        }

        // 3. 测试事务操作，update
        try
        {
            // 开始事务
            if (conn.beginTransaction())
            {
                // 更新名称
                unsigned long long affected = conn.executeUpdate("UPDATE test_reconnect SET name = 'update' WHERE id = 1");
                std::cout << "更新操作共影响" << affected << "行数据" << std::endl;
                // 提交事务
                if (conn.commit())
                {
                    std::cout << "提交事务成功" << std::endl;
                }
                else
                {
                    std::cerr << "提交事务失败" << std::endl;
                }
            }
            else
            {
                std::cerr << "开始事务失败" << std::endl;
                return false;
            }
        }
        catch (const db::SQLExecutionError &e)
        {
            std::cerr << "事务测试失败：" << e.what() << "（错误码" << e.getErrorCode() << "）" << std::endl;
            return false;
        }

        std::cout << "重连总次数：" << conn.getTotalReconnectAttempts()
                  << ", 成功重连总次数：" << conn.getSuccessfulReconnects() << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cerr << "测试失败：" << e.what() << '\n';
        return false;
    }

    return true;
}

bool testInvalidCredentials()
{
    printTestHeader("测试无效凭证");

    try
    {
        // 0. 创建连接，使用错误的密码
        Connection conn{TEST_HOST, TEST_USER, "wrong_password", TEST_DATABASE, TEST_PORT, 200U, 3};
        // 1. 尝试连接，应该失败
        if (!conn.connect())
        {
            std::cout << "连接失败正确" << std::endl;
        }
        else
        {
            std::cout << "连接成功，测试失败" << std::endl;
            return false;
        }
        // 2. 尝试重连，应该失败，计时
        auto start = std::chrono::steady_clock::now();
        bool reconnected = conn.reconnect();
        auto end = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        if (!reconnected)
        {
            std::cout << "重连失败，验证正确，花费 " << duration.count() << " ms" << std::endl;
        }
        else
        {
            std::cout << "重连成功，验证失败" << std::endl;
            return false;
        }

        std::cout << "初始重连：- 尝试" << conn.getTotalReconnectAttempts()
                  << ", - 成功" << conn.getSuccessfulReconnects() << std::endl;

        // ###BUG 当测试通过时，函数没有写返回值，导致未定义行为，程序返回了退出码1,出现makefile error
        return true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "无效凭证重连测试失败：" << e.what() << '\n';
        return false;
    }
}

bool testReconnectDelay()
{
    printTestHeader("测试重连延迟，故意传入无效主机");

    try
    {
        // 使用无效主机创建连接
        Connection conn{"invalid_host_12345", TEST_USER, TEST_PASSWORD, TEST_DATABASE, TEST_PORT, 50U, 3};
        // 尝试重连，并计时
        auto start = std::chrono::steady_clock::now();
        bool reconnected = conn.reconnect();
        auto end = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        // 只有重连失败，才是测试成功
        if (!reconnected)
        {
            std::cout << "重连失败，测试成功，重连花费" << duration.count() << "ms" << std::endl;

            // 打印重连总次数与成功次数
            std::cout << "无效主机重连测试：- 尝试" << conn.getTotalReconnectAttempts()
                      << ", - 成功" << conn.getSuccessfulReconnects() << std::endl;

            // 不要忘记写返回值
            return true;
        }
        else
        {
            std::cerr << "重连成功，但是测试失败" << std::endl;
            return false;
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "无效主机重连测试失败：" << e.what() << '\n';
        return false;
    }
}

bool testStatisticsReset()
{
    printTestHeader("重连统计重置测试");

    try
    {
        // 创建连接
        Connection conn{TEST_HOST, TEST_USER, TEST_PASSWORD, TEST_DATABASE, TEST_PORT, 100U, 3};
        // 建立连接
        if (conn.connect())
        {
            std::cout << "建立连接成功" << std::endl;

            // 重连2次
            conn.reconnect();
            conn.reconnect();
            // 统计重连次数
            std::cout << "重置前：重连总次数=" << conn.getTotalReconnectAttempts()
                      << ", 成功次数=" << conn.getSuccessfulReconnects() << std::endl;
            // 重置
            conn.resetReconnectStatus();
            // 统计重置后重连次数
            unsigned int newAttempts = conn.getTotalReconnectAttempts();
            unsigned int newSuccess = conn.getSuccessfulReconnects();
            std::cout << "重置后：重连总次数=" << newAttempts
                      << ", 成功次数=" << newSuccess << std::endl;
            // 验证重置是否正确
            if (newAttempts == 0 && newSuccess == 0)
            {
                std::cout << "重置测试通过" << std::endl;
                return true;
            }
            else
            {
                std::cout << "重置测试失败" << std::endl;
                return false;
            }
        }
        else
        {
            std::cout << "建立连接失败" << std::endl;
            return false;
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "测试失败：" << e.what() << '\n';
        return false;
    }
}

void printSummary(const std::vector<std::pair<std::string, bool>> &results)
{
    std::cout << "\n"
              << std::string(50, '*') << std::endl;
    std::cout << "      测试结果总结" << std::endl;
    std::cout << std::string(50, '*') << std::endl;

    unsigned int passed = 0;
    for (const auto &[testName, testResult] : results)
    {
        std::cout << testName << (testResult ? "成功" : "失败") << std::endl;
        if (testResult)
            ++passed;
    }

    std::cout << "共有" << passed << "/" << results.size() << "项测试通过" << std::endl;

    if (passed == results.size())
    {
        std::cout << "\n恭喜我自己！第3天重连功能测试全部通过！" << std::endl
                  << "\n我已经成功实现了：" << std::endl
                  << "智能错误码识别" << std::endl
                  << "自动重连机制" << std::endl
                  << "指数退避算法" << std::endl
                  << "重连统计监控" << std::endl
                  << "异常处理系统" << std::endl
                  << "\n明天我们将实现连接池核心逻辑!" << std::endl;
    }
    else
    {
        std::cout << "\n部分测试未通过，请检查：" << std::endl
                  << "1. MySQL服务是否正常运行" << std::endl
                  << "2. 连接参数是否正确" << std::endl
                  << "3. 用户权限是否足够" << std::endl;
    }
}

// @TODO 这里以后再看看，还有些不完美，继续做下面的day4
void testConcurrentReconnect()
{
    printTestHeader("测试并发重连安全性");

    // 要求多个线程并发执行建立连接，并执行查询操作，查看此时进行重连能够成功
    // 创建线程池
    std::vector<std::thread> threads;
    unsigned int threadSize = 3;
    // 创建结果集
    std::vector<bool> results(threadSize);
    // 创建每个单独的线程
    for (unsigned int i = 0; i < threadSize; ++i)
    {
        threads.emplace_back([i, &results]
                             {
                                 // 建立连接
                                 Connection conn{TEST_HOST, TEST_USER, TEST_PASSWORD, TEST_DATABASE, TEST_PORT, 100U, 2};

                                 std::cout << "线程" << i << "尝试建立MySQL连接..." << std::endl;
                                 if (conn.connect())
                                 {
                                     // 成功建立连接，循环执行3次查询操作
                                    try
                                    {
                                         for (unsigned int j = 0; j < 3; ++j)
                                         {
                                             QueryResultPtr queryResult = conn.executeQuery("SELECT " + std::to_string(10 * i + j) + " as test_value");
                                                if(queryResult->next())
                                                {
                                                    std::cout << "线程" << i << " 查询 " << j << " test_value: " << queryResult->getInt("test_value") << std::endl;
                                                }
                                            // 线程执行一次，需要等待一会
                                            std::this_thread::sleep_for(std::chrono::milliseconds(100));
                                        }  
                                        results[i] = true;  
                                    } catch(const db::SQLExecutionError &e)
                                    {
                                        std::cerr << e.what() << '\n';
                                        if(conn.isConnectionError(e.getErrorCode()))
                                            results[i] =false;
                                    }
                                 }
                                 else
                                 {
                                     // 如果建立连接失败，进行重连，重连的结果写入结果集
                                     std::cout << "线程" << i << "建立连接失败，尝试重连" << std::endl;
                                     bool reconnected = conn.reconnect();
                                     std::cout << "线程" << i << "重连" << (reconnected ? "成功" : "失败") << std::endl;
                                     results[i] = reconnected;
                                     return;
                                 }

                                 std::cout << "线程" << i << "重连数据统计：- 尝试" << conn.getTotalReconnectAttempts()
                                           << "次，- 成功" << conn.getSuccessfulReconnects() << std::endl; });
    }

    // 安全回收线程
    for (auto &thread : threads)
    {
        thread.join();
    }
    // 检测并发执行的最终结果，如果准确值大于等于2，说明成功
    unsigned int correctSize = 0;
    for (const auto &result : results)
    {
        if (result)
            correctSize++;
    }
    if (correctSize > 2)
        std::cout << "并发连接测试通过" << std::endl;
    else
        std::cout << "并发连接测试失败" << std::endl;
}

// 这次的测试函数改变方式了，每个单元测试返回的是是否测试成功
int main()
{
    std::cout << "开始第3天重连功能测试..." << std::endl;
    std::cout << "重连参数：" << TEST_USER + "@" + TEST_HOST + ":" + std::to_string(TEST_PORT) + "/" + TEST_DATABASE << std::endl;

    // 初始化日志系统 ### 日志系统的路径写错啦
    Logger::getInstance().init("./docs/test_day3_connection.log");

    std::vector<std::pair<std::string, bool>> results;

    // ### BUG 因为emplace是在原地构造对象，因此使用emplace应该指定位置，然后是对象的构造函数列表，或者直接使用empalce_back
    results.emplace(results.end(), "错误码识别测试", testErrorCodeRecognition());
    results.emplace_back("基础重连测试", testBasicReconnection());
    results.emplace_back("带重连查询测试", testQueryWithReconnect());
    results.emplace_back("无效凭证重连测试", testInvalidCredentials());
    results.emplace_back("重连延迟测试", testReconnectDelay());
    results.emplace_back("重置重连统计测试", testStatisticsReset());
    testConcurrentReconnect();

    // 显示测试结果
    int passed = 0;
    for (const auto &[_, result] : results)
    {
        if (result)
            ++passed;
    }

    printSummary(results);

    if (passed == results.size())
    {
        return 0;
    }
    else
    {
        return 1;
    }
}
#include <string>
#include <iostream>
#include <mysql/mysql.h>
#include <chrono>
#include "db_config.h"
#include "pool_config.h"
#include "logger.h"
#include "query_result.h"
#include "connection.h"

/**
 * @brief 第二天数据库连接功能测试
 *
 * 注意：运行此测试前需要：
 * 1. 安装并运行MySQL服务器
 * 2. 创建测试用数据库和表
 * 3. 修改连接参数，连接参数必须采用我自己的数据库用户名、密码、数据库等
 */

// 测试用数据库连接参数，全局常量
const std::string TEST_HOST = "localhost";
const std::string TEST_USER = "admin";
const std::string TEST_PASSWORD = "123456";
const std::string TEST_DATABASE = "testdb";
const unsigned int TEST_PORT = 3306;

// 创建测试数据库和表的SQL语句
const std::string CREATE_DB_SQL = "CREATE DATABASE IF NOT EXISTS " + TEST_DATABASE;
const std::string USE_DB_SQL = "USE " + TEST_DATABASE;
const std::string CREATE_TABLE_SQL = R"(
CREATE TABLE IF NOT EXISTS test_users (
id INT AUTO_INCREMENT PRIMARY KEY,
name VARCHAR(50) NOT NULL,
age INT NOT NULL,
email VARCHAR(100) NOT NULL,
created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
)
)"; // ### 需要使用R"()"包裹SQL语句

/**
 * @brief 打印每个新的测试模块的标题
 */
void printSeparator(const std::string &title)
{
    std::cout << '\n'
              << std::string(50, '=') << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout << std::string(50, '=') << std::endl;
}

void testConfigStructure()
{
    printSeparator("测试配置结构");

    std::cout << "1. 测试DBConfig结构..." << std::endl;

    DBConfig dbconfig1;
    std::cout << "默认构造成功，port=" << dbconfig1.port << ", weight=" << dbconfig1.weight << std::endl;

    DBConfig dbconfig2{"localhost", "admin", "123456", "testdb", 3306, 5};
    std::cout << "参数构造成功：" << dbconfig2.getConnectionStr() << std::endl;

    if (dbconfig2.isValid())
    {
        std::cout << "DBConfig配置验证通过" << std::endl;
    }

    std::cout << "\n2. 测试PoolConfig结构..." << std::endl;

    PoolConfig poolConfig1;
    std::cout << "默认构造成功：" << poolConfig1.getSummary() << std::endl;

    PoolConfig poolConfig2{TEST_HOST, TEST_USER, TEST_PASSWORD, TEST_DATABASE};
    poolConfig2.setConnectionLimits(4, 16, 10);
    poolConfig2.setTimeouts(3000, 300000, 30000); // 3秒 5分钟 30秒
    std::cout << "参数构造成功：" << poolConfig2.getSummary() << std::endl;

    if (poolConfig2.isValid())
    {
        std::cout << "单数据库模式池配置验证通过" << std::endl;
    }

    // 测试多数据库配置
    std::cout << "\n3. 测试多数据配置..." << std::endl;
    PoolConfig multiConfig;
    multiConfig.addDatabase(DBConfig("db1.example.com", "user1", "pass", "db1", 3306, 2));
    multiConfig.addDatabase(DBConfig("db2.example.com", "user2", "pass", "db2", 3306, 3));
    std::cout << "多数据库池配置共添加了" << multiConfig.getDatabaseCount() << "个数据库实例" << std::endl;
    if (multiConfig.isValid())
    {
        std::cout << "多数据库模式池配置验证通过" << std::endl;
    }
}

bool setupTestEnvironment()
{
    printSeparator("设置测试环境");

    std::cout << "正在尝试连接到MySQL服务器..." << std::endl
              << "连接参数：" << TEST_USER << "@" << TEST_HOST << ":" << TEST_PORT << std::endl
              << "\n注意：如果连接失败，请检查：" << std::endl
              << "  1. MySQL服务器是否启动" << std::endl
              << "  2. 用户名密码是否正确" << std::endl
              << "  3. 用户是否有足够的权限" << std::endl // 用户权限不要忘记
              << "  4. 防火墙设置是否正确" << std::endl;  // 防火墙设置不要忘记

    try
    {
        // 先不指定数据库，目前是要创建所需的数据库和表
        Connection setupConn{TEST_HOST, TEST_USER, TEST_PASSWORD, "", 3306};
        if (!setupConn.connect())
        {
            std::cout << "无法连接到MySQL服务器..." << std::endl;
            std::cout << "失败原因：" << setupConn.getLastError() << std::endl;

            return false;
        }

        std::cout << "成功连接到MySQL服务器" << std::endl;

        // 创建测试用数据库和表
        std::cout << "正在创建测试数据库..." << std::endl;
        setupConn.executeUpdate(CREATE_DB_SQL);
        setupConn.executeUpdate(USE_DB_SQL);
        setupConn.executeUpdate(CREATE_TABLE_SQL);
        std::cout << "测试环境设置完成" << std::endl;

        return true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "测试环境设置失败：" << e.what() << '\n';
        return false;
    }
}

void testBasicConnection()
{
    printSeparator("测试基础连接功能");

    // 因为代码中有很多抛出异常的地方，所以测试代码必须能够捕获异常
    try
    {
        std::cout << "\n1. 测试连接创建..." << std::endl;
        Connection conn(TEST_HOST, TEST_USER, TEST_PASSWORD, TEST_DATABASE, 3306);
        std::cout << "连接创建成功，id=" << conn.getConnectionId() << std::endl
                  << "creationTime=" << conn.getCreationTime() << std::endl;
        
        std::cout << "\n2. 测试连接建立..." << std::endl;
        if(conn.connect())
        {
            std::cout << "数据库连接建立成功" << std::endl;
        } else {
            std::cout << "数据库连接建立失败" << std::endl;
            return;
        }

        std::cout << "\n3. 测试连接有效性..." << std::endl;
        if(conn.isValid())
        {
            std::cout << "连接有效性检测通过" << std::endl;
        } else {
            std::cout << "连接无效" << std::endl;
        }

        std::cout << "\n4. 测试连接信息..." << std::endl;
        std::cout << "最后活动时间：" << conn.getLastActiveTime() << std::endl;

    }
    catch(const std::exception& e)
    {
        std::cerr << "基础连接功能测试失败：" << e.what() << '\n';
    }
    
}

void testQueryOperations()
{
    printSeparator("测试查询操作");

    try
    {
        Connection conn {TEST_HOST, TEST_USER, TEST_PASSWORD, TEST_DATABASE};
        if(!conn.connect())
        {
            std::cout << "连接失败，跳过查询测试" << std::endl;
            return;
        }

        std::cout << "\n1. 清空数据表" << std::endl;
        unsigned long long affected = conn.executeUpdate("DELETE FROM test_users");
        std::cout << "清空了" << affected << "行记录" << std::endl;

        std::cout << "\n2. 插入测试数据..." << std::endl;
        // ### 在一条INSERT语句中可以插入多行记录
        affected = conn.executeUpdate(
            "INSERT INTO test_users (name, age, email) VALUES "
            "('张三', 25, 'zhangsan@example.com'), "
            "('李四', 30, 'lisi@example.com'), "
            "('王五', 28, 'wangwu@example.com')"
        );
        std::cout << "成功插入了 " << affected << " 行记录" << std::endl;

        std::cout << "\n3. 开始查询..." << std::endl;
        QueryResultPtr result = conn.executeQuery("SELECT id, name, age, email FROM test_users ORDER BY age");
        std::cout << "查询成功，结果信息：" << std::endl;
        std::cout << "  - 字段数量：" << result->getFieldCount() << std::endl
                  << "  - 行数：" << result->getRowCount() << std::endl;
        
        auto fieldNames = result->getFieldNames();
        std::cout << "  - 字段名：";
        for(const auto &field : fieldNames)
        {
            std::cout << field << " ";
        }
        std::cout << std::endl;

        std::cout << "\n4. 遍历查询结果..." << std::endl;
        // 先按照字段名进行查询，而且要符合字段值的类型
        for(const auto &field : fieldNames)
        {
            std::cout << field << "\t";
        }
        std::cout << std::endl;
        std::cout << std::string(50, '-') << std::endl;
        int rowCount = 0;
        while (result->next())
        {
            int id = result->getInt("id");
            std::string name = result->getString("name");
            int age = result->getInt("age");
            std::string email = result->getString("email");
            std::cout << id << "\t" << name << "\t"
                      << age << "\t" << email << std::endl;
            ++rowCount;
        }

        std::cout << "成功遍历 " << rowCount << " 行数据" << std::endl;

        // 测试不同数据类型
        std::cout << "\n5. 测试数据类型转换..." << std::endl;
        result->reset();    // 重置到结果集的开头，重新开始
        if(result->next())
        {
            int id = result->getInt(0); // 根据字段索引查询id
            std::string name = result->getString(1);    // 根据字段索引查询name
            auto age = result->getLong(2);     // 根据字段索引查询age
            
            std::cout << "数据类型转换：id=" << id << ", name=" << name << ", age(long)=" << age << std::endl;
        }

        std::cout << "Connection查询操作验证通过" << std::endl;
    }
    catch(const std::exception& e)
    {
        std::cerr << "查询操作测试失败：" << e.what() << '\n';
    }
}

void testTransactionOperations()
{
    printSeparator("测试事务操作");

    try
    {
        // 1. 建立连接
        Connection conn {TEST_HOST, TEST_USER, TEST_PASSWORD, TEST_DATABASE, TEST_PORT};
        if(!conn.connect())
        {
            std::cout << "\n MySQL连接失败，跳过事务测试" << std::endl;
            std::cout << "检查MySQL连接参数是否正确并重新运行测试" << std::endl;
        }

        // 2. 事务测试
        if(conn.beginTransaction())
        {
            std::cout << "1. 开始进行事务测试..." << std::endl;

            // 插入两次数据，然后通过查询操作查看完成一次事务之后一共添加了几行测试数据
            unsigned long long affected = conn.executeUpdate("INSERT INTO test_users (name, age, email) VALUES "
                        "('事务测试', 20, 'transaction@test.com')");
            std::cout << "操作1添加了 " << affected << " 行数据" << std::endl;

            affected = conn.executeUpdate("INSERT INTO test_users (name, age, email) VALUES "
                            "('事务测试', 21, 'transaction@test.com')");
            std::cout << "操作2添加了 " << affected << " 行数据" << std::endl;

            // 提交事务，然后检查如果成功提交事务，此时是不是一共包含2行事务测试
            std::cout << "2. 提交事务..." << std::endl;
            if(conn.commit())
            {
                std::cout << "事务提交成功" << std::endl;
                auto result = conn.executeQuery("SELECT COUNT(*) as count FROM test_users WHERE name = '事务测试'");
                // ###BUG 我自己都没有搞懂查询结果的真正顺序，应该先next更新当前的结果行之后，再进行查询，没有办法直接查询
                // 我之前没有更新next导致是没有办法得到当前的结果的
                result->next();     // 应该执行next之后才能更新MYSQL_ROW，才能得到结果集的最新一行
                int queryCount = result->getInt("count");
                std::cout << "执行事务之后，共添加了 " << queryCount << " 行数据（应该是两行）" << std::endl;
            } else {
                std::cout << "事务提交失败" << std::endl;
            }
        }
        // 3. 回滚测试 start transaction --> commit --> rollback，所以测试事务回滚，直接在后面执行rollback就行
        // ### BUG 为什么我进行事务回滚之后，之前提交事务之后的操作没有撤销呢？？？ 难道是我的理解有问题
        // ### 疑问：意思是开始事务后，然后提交事务，只要提交完事务，执行事务回滚是不会撤销已经提交的事务操作的
        // ### 只有开始事务后，没有执行提交commit操作，执行rollback操作才能成功撤销开始事务后执行的操作
        std::cout << "3. 测试事务回滚" << std::endl;
        if(conn.beginTransaction())
        {
            std::cout << "开始事务执行成功" << std::endl;
        } else {
            std::cout << "开始事务执行失败" << std::endl;
        }

        unsigned long long inserted = conn.executeUpdate("INSERT INTO test_users (name, age, email) VALUES "
                                                    "('回滚测试', 22, 'rollback@test.com')");
        std::cout << "成功插入了 " << inserted << " 行数据" << std::endl;

        if(conn.rollback())
        {
            // 此时在查找'回滚测试'，应该返回0行
            auto result = conn.executeQuery("SELECT COUNT(*) as count FROM test_users WHERE name = '回滚测试'");
            result->next();
            std::cout << "事务回滚后，共找到 " << result->getInt("count") << " 行数据（应该0行）" << std::endl;
        }

        std::cout << "事务测试验证通过" << std::endl;
    }
    catch(const std::exception& e)
    {
        std::cerr << "事务操作测试失败：" << e.what() << '\n';
        return;
    }
}

void testErrorHandling()
{
    // 这个测试函数的目的是建立连接后，执行一些错误命令，我需要观察返回的错误字符串以及错误码是什么
    printSeparator("测试错误处理");

    try
    {
        Connection conn {TEST_HOST, TEST_USER, TEST_PASSWORD, TEST_DATABASE, TEST_PORT};
        if(!conn.connect())
        {
            std::cout << "\n  MySQL连接失败，跳过错误处理测试" << std::endl;
        }

        // 接下来就是执行一些错误的命令，然后查看返回的字符串，因为我是用了递归锁，所以不会发生死锁问题
        std::cout << "1. 测试SQL语法错误处理..." << std::endl;
        // 这里的细节就是，我的执行肯定会发生错误，所以需要使用try-catch来捕获异常，进行异常处理
        // ### BUG 这里出现了问题：getLastError无法打印任何信息，这是为什么呢？
        // 要么是对于无效语句，不会打印任何信息，要么就是我的写法有问题
        try
        {
            conn.executeQuery("SELECT * FROM non_existent_table");
            std::cout << "应该抛出异常" << std::endl;
        }
        catch(const std::exception& e)
        {
            std::cerr << "正确捕获异常：" << e.what() << '\n';
        }

        // 测试字符串转义
        std::cout << "\n2. 测试字符串转义" << std::endl;
        std::string dangerousInput = "Robert'); DROP TABLE test_users; --";
        std::string escaped = conn.escapeString(dangerousInput);
        std::cout << "原始字符串： " << dangerousInput << std::endl;
        std::cout << "转义后字符串：" << escaped << std::endl;

        // 直接获取错误信息和错误码
        std::cout << "\n3. 测试错误信息获取..." << std::endl;
        try
        {
            conn.executeQuery("INVALID SQL STATEMENT");
        }
        catch(const std::exception& e)
        {
            std::cout << "错误码：" << conn.getLastErrorCode() << std::endl
                      << "错误信息：" << conn.getLastError() << std::endl;
        }
    }
    catch(const std::exception& e)
    {
        std::cerr << "错误处理验证失败：" << e.what() << '\n';
    }
}

void testPerformance()
{
    printSeparator("测试基础性能");

    try
    {
        // 1. 建立连接
        Connection conn {TEST_HOST, TEST_USER, TEST_PASSWORD, TEST_DATABASE, TEST_PORT};
        if(!conn.connect())
        {
            std::cout << "\n MySQL连接建立失败，跳过基础性能测试阶段" << std::endl;
            return;
        }

        // 2. 清空数据表的全部测试数据
        conn.executeUpdate("DELETE FROM test_users");   // 删除全部的shuju

        // 3. 记录开始时间
        auto start = std::chrono::high_resolution_clock::now();
        // 4. 执行100次插入操作
        for(int i = 0; i < 100; ++i)
        {
            std::string insertSQL = "INSERT INTO test_users (name, age, email) VALUES "
                                    "('用户" + std::to_string(i) + "', " + std::to_string(20 + i % 30) +
                                    ", 'user" + std::to_string(i) + "@example.com')";
            conn.executeUpdate(insertSQL);
        }
        // 5. 记录结束时间
        auto end = std::chrono::high_resolution_clock::now();
        // 6. 计算持续的事件，并打印出来
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        // 差点又忘记，需要调用count才能打印出数字
        std::cout << "执行100次插入操作共花费 " << duration.count() << " ms" << std::endl;

        // 记录查询的初始时间
        start = std::chrono::high_resolution_clock::now();
        // 进行50次查询操作
        for(int i = 0; i < 50; ++i)
        {
            std::string querySQL = "SELECT * FROM test_users LIMIT 10";     // 限制只能查询10行
            auto result = conn.executeQuery(querySQL);
            int count = 0;
            while(result->next())
                ++count;
        }
        // 记录查询的结束时间
        end = std::chrono::high_resolution_clock::now();
        // 计算持续的时间并打印出来
        duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << "查询50次共花费" << duration.count() << "ms" << std::endl;
    }
    catch(const std::exception& e)
    {
        std::cerr << "基础性能测试失败：" << e.what() << '\n';
    }
}

int main()
{
    std::cout << "开始第2天数据库连接测试..." << std::endl;
    // 我需要指定我的日志文件的路径
    Logger::getInstance().init("./docs/test_day2_connection.log", LogLevel::INFO);

    try
    {
        testConfigStructure();

        if(!setupTestEnvironment())
        {
            std::cout << "\n 无法设置测试环境，跳过数据库相关测试" << std::endl;
            std::cout << "请检查数据库相关的连接参数并重新运行测试！" << std::endl;
            return 1;
        }

        testBasicConnection();
        testQueryOperations();
        testTransactionOperations();
        testErrorHandling();
        testPerformance();

        std::cout << "\n 恭喜我自己，我终于完成了第2天的所有测试任务！" << std::endl
                  << "我已经成功实现了：" << std::endl
                  << "  -- 灵活的配置管理系统" << std::endl
                  << "  -- 安全的查询结果封装" << std::endl
                  << "  -- 完整的数据库连接类" << std::endl
                  << "  -- 事务管理功能" << std::endl
                  << "  -- 完善的错误处理机制" << std::endl
                  << "\n明天我们将实现自定义重连机制，提高系统的可靠性！" << std::endl;

        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << "测试失败：" << e.what() << '\n';
        return 1;
    }
}
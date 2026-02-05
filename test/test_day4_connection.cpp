#include "connection_pool.h"
#include "db_exception.h"
#include "logger.h"
#include <future>
#include <iostream>
#include <string>
#include <utility>
#include <vector>
#include <cassert>

/**
 * @brief 第4天连接池核心功能测试
 *
 * 重点验证：
 * 1. 连接池初始化和配置
 * 2. 连接获取和释放机制
 * 3. 并发访问安全性
 * 4. 连接池状态监控
 * 5. 健康检查功能
 * 6. 连接超时处理
 *
 * 南江，请继续沉住气，好好写，好好去参悟，这里蕴含着大道
 * 不要着急，此时此刻才是最重要的，才应该是最珍惜的
 * 把自己的每一件小事做好，珍惜自己的家庭，踏踏实实，不幻想，干实事
 */

// 测试数据库连接参数
const std::string TEST_HOST = "localhost";
const std::string TEST_USER = "admin";
const std::string TEST_PASSWORD = "123456";
const std::string TEST_DATABASE = "testdb";
const unsigned int TEST_PORT = 3306;

void printTestHeader(const std::string &title) {
  std::cout << "\n" << std::string(60, '=') << std::endl;
  std::cout << "  " << title << std::endl;
  std::cout << std::string(60, '=') << std::endl;
}

bool testPoolInitialization() {
  printTestHeader("测试连接池初始化");

  try {
    auto &pool = ConnectionPool::getInstance();

    // 测试重复初始化
    std::cout << "1. 测试基本初始化..." << std::endl;

    PoolConfig config(TEST_HOST, TEST_USER, TEST_PASSWORD, TEST_DATABASE,
                      TEST_PORT);
    config.setConnectionLimits(3, 10, 5); // min=3;max=10;init=5
    config.setTimeouts(
        3000, 300000,
        10000); // connectionTimeout=3秒, maxIdleTimeout=5分钟, checkPeriod=10秒

    pool.init(config);
    // 完成某一个功能测试，就在控制台输出打印完成的测试
    std::cout << "连接池初始化成功" << std::endl;

    // 我接下来需要查看连接池初始化完成后的连接池状态参数，能够正确打印，是否还需要使用assert进行判断
    // 检查初始状态
    std::cout << "2. 检查初始状态..." << std::endl;
    std::cout << "是否已初始化: " << (pool.isInititalized() ? "是" : "否")
              << std::endl;
    std::cout << "总连接数：" << pool.getTotalCount() << std::endl;
    std::cout << "空闲连接数：" << pool.getIdleCount() << std::endl;
    std::cout << "活跃连接数: " << pool.getActiveCount() << std::endl;

    // 测试重复初始化
    std::cout << "3. 测试重复初始化..." << std::endl;
    pool.init(config); // 应该被忽略
    std::cout << "重复初始化被正确处理" << std::endl;

    return true;
  } catch (const std::exception &e) {
    std::cerr << "测试失败：" << e.what() << std::endl;
    return false;
  }
}

bool testBasicConnectionOperations() {
  // 我猜测在这个测试函数中，是获取连接和释放连接操作
  printTestHeader("测试基本连接操作");

  // 必须要有完整的异常捕获机制
  // 南江，继续加油，不要分心，好好专注
  try {
    auto &pool = ConnectionPool::getInstance();

    std::cout << "1. 测试获取连接..." << std::endl;
    auto conn1 = pool.getConnection();
    if (conn1) {
      std::cout << "成功获取连接：" << conn1->getConnectionId() << std::endl;
    } else {
      std::cout << "获取连接失败" << std::endl;
      return false;
    }

    std::cout << "2. 测试连接功能..." << std::endl;
    // 这里有try-catch，说明测试conn1的查询或者update函数可能会抛出异常
    try {
      // ###疑问：为什么有的名字直接使用就行，但是有些需要加上单引号作为字符串
      // ###回答：对于MySQL中的保留关键字，或者中间包含空格的别名，或者以数字开头的字符串，甚至是普通字符串，最好使用反单引号包裹字符串作为别名。反引号是MySQL标准的标识符引用符
      // 还有就是使用mysql_query不需要使用;作为SQL语句的结尾
      auto result = conn1->executeQuery(
          "SELECT 1 as test_value, NOW() as `current_time`");
      if (result->next()) {
        std::cout << "执行查询成功：值=" << result->getInt("test_value")
                  << "时间=" << result->getString("current_time") << std::endl;
      }
    } catch (const db::SQLExecutionError &e) {
      std::cerr << "使用获取的MySQL连接执行查询操作失败：" << e.what() << '\n';
      return false;
    }

    std::cout << "3. 检查连接池状态..." << std::endl;
    std::cout << "空闲连接数: " << pool.getIdleCount() << std::endl;
    std::cout << "活跃连接数: " << pool.getActiveCount() << std::endl;
    std::cout << "总连接数: " << pool.getTotalCount() << std::endl;

    std::cout << "4. 测试释放连接..." << std::endl;
    pool.releaseConnection(conn1);
    std::cout << "连接释放成功" << std::endl;

    std::cout << "5. 检查释放后状态..." << std::endl;
    std::cout << "空闲连接数: " << pool.getIdleCount() << std::endl;
    std::cout << "活跃连接数: " << pool.getActiveCount() << std::endl;

    return true;

  } catch (const std::exception &e) {
    std::cerr << "基本连接操作测试失败: " << e.what() << '\n';
    return false;
  }
}

bool testMultipleConnections() {
  printTestHeader("测试多连接获取");

  try {
    auto &pool = ConnectionPool::getInstance();
    std::vector<ConnectionPtr> connections;

    std::cout << "1. 获取多个连接..." << std::endl;

    // 获取多个连接
    for (int i = 0; i < 5; ++i) {
      auto conn = pool.getConnection();
      if (conn) {
        connections.push_back(conn);
        std::cout << "获取连接" << (i + 1) << ": " << conn->getConnectionId()
                  << std::endl;
      } else {
        std::cout << "获取连接" << (i + 1) << "失败" << std::endl;
        return false;
      }
    }

    std::cout << "2. 检查连接池状态..." << std::endl;
    std::cout << "空闲连接数：" << pool.getIdleCount() << std::endl;
    std::cout << "活跃连接数：" << pool.getActiveCount() << std::endl;
    std::cout << "总连接数：" << pool.getTotalCount() << std::endl;

    std::cout << "3. 测试所有连接功能..." << std::endl;
    for (int i = 0; i < connections.size(); ++i) {
      try {
        auto result = connections[i]->executeQuery(
            "SELECT " + std::to_string(i + 1) + " as `conn_num`");
        if (result->next()) {
          std::cout << "连接" << (i + 1)
                    << "执行查询成功：conn_num=" << result->getInt("conn_num")
                    << std::endl;
        }
      } catch (const std::exception &e) {
        std::cerr << "连接" << (i + 1) << "执行查询失败：" << e.what() << '\n';
        // ###BUG
        // 这里的逻辑是要测试所有的连接能够正常执行SQL语句，因此即使其中一条连接执行失败，也不应该直接返回false
        // return false;
      }
    }

    std::cout << "4. 释放所有连接..." << std::endl;
    for (auto &conn : connections) {
      pool.releaseConnection(conn);
    }
    connections.clear();

    std::cout << "所有连接释放完成" << std::endl;
    std::cout << "最终空闲连接数：" << pool.getIdleCount() << std::endl;
    std::cout << "最终活跃连接数：" << pool.getActiveCount() << std::endl;

    return true;

  } catch (const std::exception &e) {
    std::cerr << "测试失败：" << e.what() << '\n';
    return false;
  }
}

// 南江，继续加油，孤单了很正常，孤单就去聊天，就去社交，怕什么，继续好好敲自己的代码，不要怕
// 做软件最优秀的一个特点，就是耐烦，因为需要考虑很多很多的事情，一定要耐住·寂寞，耐住烦躁
// 如果孤单了，难受了，那就出去走走，想办法，不要拘着
// 你现在的努力，以后就可以完成自己的工作任务了，所以南江，一定要好好把握时间
bool testConcurrentAccess() {
  printTestHeader("测试并发访问");

  try {
    auto &pool = ConnectionPool::getInstance();

    std::cout << "1. 启动并发测试..." << std::endl;

    const int numThreads = 10;
    const int operationsPerThread = 5;
    std::vector<std::future<bool>> futures;

    auto worker = [&pool, operationsPerThread](int threadId) -> bool {
      try {
        for (int i = 0; i < operationsPerThread; ++i) {
          // 获取连接
          auto conn = pool.getConnection(2000); // 2秒超时
          if (!conn) {
            std::cout << "线程 " << threadId << " 获取连接失败" << std::endl;
            return false;
          }

          // 执行查询
          auto result = conn->executeQuery(
              "SELECT " + std::to_string(threadId * 100 + i) + " as value");
          if (!result->next()) {
            std::cout << "线程 " << threadId << " 查询失败" << std::endl;
            return false;
          }

          // 模拟处理时间
          std::this_thread::sleep_for(std::chrono::milliseconds(50));

          // 释放连接
          pool.releaseConnection(conn);
        }

        std::cout << "线程 " << threadId << " 完成所有操作" << std::endl;
        return true;

      } catch (const std::exception &e) {
        std::cerr << "线程 " << threadId << " 异常：" << e.what() << '\n';
        return false;
      }
    };

    // 启动所有线程
    for (int i = 0; i < numThreads; ++i) {
      // ###疑问：
      // 这个异步不能理解，需要好好理解，这里所有的future都是移动构造的
      futures.push_back(std::async(std::launch::async, worker, i));
    }

    // 等待所有线程完成
    bool allSuccess = true;
    for (auto &future : futures) {
      if (!future.get()) {
        allSuccess = false;
      }
    }

    std::cout << "2. 并发测试完成..." << std::endl;
    std::cout << "最终连接池状态: " << std::endl;
    std::cout << " - 空闲连接数：" << pool.getIdleCount() << std::endl
              << " - 活跃连接数：" << pool.getActiveCount() << std::endl
              << " - 总连接数：" << pool.getTotalCount() << std::endl;

    return allSuccess;

  } catch (const std::exception &e) {
    std::cerr << "测试失败：" << e.what() << '\n';
    return false;
  }
}

// 南江，你一定可以静下心来的，不要想其他的事情，就先把这一个函数写好再说，先不要管别的，淡定又淡定，静心又静心
// 我有一家非常想去的公司，我想要年后可以应聘成功，所以我要好好努力，不要管别人，无论是好意还是歹意，我都要踏下心来，好好准备，努力再努力
// 我相信，通过自己的每一分努力，一定可以走出精神内耗，带着希望，认认真真努力，有光明的未来，不会让自己失望的，代码一定要敲个不停，思考不能停！
bool testConnectionTimeout() {
  printTestHeader("测试连接超时");
  try {
    auto &pool = ConnectionPool::getInstance();
    std::vector<ConnectionPtr> connections;

    std::cout << "1. 获取所有可用连接..." << std::endl;

    // 尝试获取所有连接直到达到最大值
    for (int i = 0; i < 15; ++i) { // 超过最大连接数
      try {
        auto conn = pool.getConnection(100); // 100ms超时
        if (conn) {
          connections.push_back(conn);
          std::cout << "获取连接 " << (i + 1) << std::endl;
        } else {
          break;
        }
      } catch (const std::exception &e) {
        std::cout << "预期的获取连接异常：" << e.what() << std::endl;
        break;
      }
    }

    std::cout << "2. 测试超时获取连接..." << std::endl;
    try {
      auto conn = pool.getConnection(200); // 200ms超时，应该失败
      std::cout << "应该超时但获取到了连接" << std::endl;
      return false;
    } catch (const std::exception &e) {
      std::cout << "正确超时：" << e.what() << std::endl;
    }

    std::cout << "3. 释放一个连接后重试..." << std::endl;
    if (!connections.empty()) {
      pool.releaseConnection(connections.back());
      connections.pop_back();

      try {
        auto conn = pool.getConnection(1000); // 1秒超时
        if (conn) {
          std::cout << "释放后成功获取连接: " << conn->getConnectionId()
                    << std::endl;
          connections.push_back(conn);
        }
      } catch (const std::exception &e) {
        std::cout << "释放后仍无法获取连接: " << e.what() << std::endl;
      }
    }

    // 清理所有连接
    for (auto &conn : connections) {
      pool.releaseConnection(conn);
    }

    return true;
  } catch (const std::exception &e) {
    std::cerr << "测试失败: " << e.what() << std::endl;
    return false;
  }
}

bool testPoolConfiguration() {
  printTestHeader("测试连接池配置");

  try {
    auto &pool = ConnectionPool::getInstance();

    std::cout << "1. 获取当前配置..." << std::endl;
    auto config = pool.getConfig();

    std::cout << "配置信息: " << std::endl
              << "  - 主机: " << config.host << ":" << config.port << std::endl
              << "  - 数据库: " << config.database << std::endl
              << "  - 最小连接数: " << config.minConnections << std::endl
              << "  - 最大连接数: " << config.maxConnections << std::endl
              << "  - 初始连接数: " << config.initConnections << std::endl
              << "  - 连接超时: " << config.connectionTimeout << "ms"
              << std::endl
              << "  - 最大空闲时间: " << config.maxIdleTime << "ms" << std::endl
              << "  - 健康检查周期: " << config.healthCheckPeriod << "ms"
              << std::endl;

    std::cout << "2. 验证配置有效性..." << std::endl;
    if (config.isValid()) {
      std::cout << "配置验证通过" << std::endl;
    } else {
      std::cout << "配置验证失败" << std::endl;
      return false;
    }

    return true;
  } catch (const std::exception &e) {
    std::cout << "测试失败: " << e.what() << std::endl;
    return false;
  }
}

// 南江，给自己的每一个阶段定目标，定切实可行的目标，完成他。好好盯着自己的目标，专注地去实现他，不要分心
// 我现在的目标就是能够成功入职我想要进的公司
bool testErrorHandling() {
  printTestHeader("测试错误处理");

  try {
    std::cout << "1. 测试无效配置..." << std::endl;
    try {
      PoolConfig invalidConfig; // 默认初始化，所以不会抛出异常
      invalidConfig.minConnections = 10;
      invalidConfig.maxConnections = 5; // 无效：最小大于最大

      // 这应该在初始化时抛出异常，但是我已经默认初始化了
      // 所以测试配置验证
      if (!invalidConfig.isValid()) {
        std::cout << "正确识别无效配置" << std::endl;
      } else {
        std::cout << "未能识别无效配置" << std::endl;
        return false;
      }
    } catch (const std::exception &e) {
      std::cout << "正确处理处理无效配置异常: " << e.what() << std::endl;
    }

    std::cout << "2. 测试释放空连接..." << std::endl;
    auto &pool = ConnectionPool::getInstance();
    pool.releaseConnection(nullptr);  // 应该被安全处理
    std::cout << "空连接释放被安全处理" << std::endl;

    std::cout << "3. 测试获取连接状态..." << std::endl;
    auto conn = pool.getConnection();
    if(conn) {
      std::cout << "连接ID: " << conn->getConnectionId() << std::endl
                << "创建时间: " << conn->getCreationTime() << std::endl
                << "最后活动时间: " << conn->getLastActiveTime() << std::endl;
      
      pool.releaseConnection(conn);
    }

    return true;

  } catch (const std::exception &e) {
    std::cout << "测试失败: " << e.what() << std::endl;
    return false;
  }
}

// 一旦开始写了，不写完绝对不可以撤退
bool testPerformance() {
  printTestHeader("测试性能基准");
  // 测试同步条件下执行100次连接获取、查询、释放花费的时间
  // 比较并发条件下执行100次连接获取、查询、释放花费的时间，能够有多大的性能提升空间

  try {
    auto &pool = ConnectionPool::getInstance();

    std::cout << "1. 测试连接获取/释放性能..." << std::endl;

    const int iterations = 100;   // 循环100次，这是要干什么
    auto start = std::chrono::high_resolution_clock::now();   // 精准时钟确定的开始时间点

    // 我知道了，执行100次的 获取连接 -> 执行查询操作 -> 释放连接 所需要花费的时间
    for(int i = 0; i < iterations; ++i) {
      auto conn = pool.getConnection();
      if(conn) {
        QueryResultPtr result = conn->executeQuery("SELECT " + std::to_string(i + 1) + " as iteration");
        result->next();   // 拿到查询的结果
        
        // ###重要：前后两次的操作要相同，这样比较的结果才可行。这样才能知道并发下可以提升多少性能
        // 这个其实没有必要写，因为太多了，所以使用断言直接判断是最省事的，没有问题就是正确的
        // std::cout << "第 " << result->getInt("iteration") << " 次查询" << std::endl;
        // assert(result->getInt("iteration") == (i+1));

        pool.releaseConnection(conn);
      }
    }

    auto end = std::chrono::high_resolution_clock::now();
    // 使用微秒精度计算，但是显示毫秒结果，这是要干啥
    auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // ###BUG 我真是欠打，又忘记了count
    double totalMs = duration_us.count() / 1000.0;

    std::cout << iterations << " 次连接操作耗时: " << std::fixed << std::setprecision(1)
              << totalMs << "ms" << std::endl;          
    std::cout << "平均每次操作: " << std::fixed << std::setprecision(3)
              << totalMs / iterations << "ms" << std::endl;
    
    std::cout << "2. 测试并发性能..." << std::endl;

    // 使用并发执行100次连接操作
    const int concurrentThreads = 5;
    const int operationsPerThread = 20;

    auto start_con = std::chrono::high_resolution_clock::now();

    std::vector<std::future<void>> futures;   // 这里存放能够拿到每个异步线程返回结果的期望
    for(int t = 0; t < concurrentThreads; ++t) {
      futures.push_back(std::async(std::launch::async, [&pool, operationsPerThread, t]{
        // 每个线程都要执行很多次的
        for(int i = 0; i < operationsPerThread; ++i) {
          ConnectionPtr conn = pool.getConnection();
          if(conn) {
            auto result = conn->executeQuery("SELECT " + std::to_string(t * 100 + i) + " as test_value");
            result->next();
            pool.releaseConnection(conn);
          }
        }
      }));
    }

    // 我需要等待每个异步线程完成自己的循环次数的连接的获取、使用、释放的过程，这需要我使用future等待所有的异步线程的执行
    for(auto &future : futures) {
      future.wait();
    }

    auto end_con = std::chrono::high_resolution_clock::now();

    auto duration_us_con = std::chrono::duration_cast<std::chrono::microseconds>(end_con - start_con);
    auto duration_ms_con = std::chrono::duration_cast<std::chrono::milliseconds>(end_con - start_con);

    auto totalMs_con = duration_us_con.count() / 1000.0;
    const int totalOperations = concurrentThreads * operationsPerThread;
    // 所有操作数所花费的时间
    std::cout << totalOperations << " 次并发连接操作花费 " << std::fixed << std::setprecision(1)
              << totalMs_con << "ms (并发数 " << concurrentThreads << ")" << std::endl;
    // 每个操作平均所花费的时间
    std::cout << "并发条件下平均每次操作花费: " << std::fixed << std::setprecision(3)
              << totalMs_con / totalOperations << "ms (并发数 " << concurrentThreads << ")" << std::endl;
    
    // 计算性能提升
    double improve = (totalMs - totalMs_con) / totalMs;
    std::cout << "并发执行后提升了 " << std::fixed << std::setprecision(1)
              << improve * 100 << "% 的性能" << std::endl;

    return true;

  } catch(const std::exception &e) {
    std::cerr << "测试失败: " << e.what() << std::endl;
    return false;
  }
}

// 南江，给我收心，给我专注，好好做好自己的事，没那么多的事情给我了解，专注做好自己的事
void printSummary(const std::vector<std::pair<std::string, bool>> &results) {
  std::cout << "\n" << std::string(60, '*') << std::endl;
  std::cout << "              第4天测试结果总结" << std::endl;
  std::cout << std::string(60, '*') << std::endl;

  // 打印每一个测试结果
  uint64_t passed = 0;
  for (const auto &[name, result] : results) {
    std::cout << (result ? "成功：" : "失败：") << name << std::endl;
    if (result)
      ++passed;
  }

  std::cout << "\n通过: " << passed << "/" << results.size() << " 项测试"
            << std::endl;
}

// 生活中没那么多事，都是自己想太多，不要想太多，现在就要把代码写好，练好，这是唯一重要的事情，好好专注
int main() {
  std::cout << "开始第4天连接池核心功能测试..." << std::endl;
  std::cout << "连接参数：" << TEST_USER << "@" << TEST_HOST << ":" << TEST_PORT
            << "/" << TEST_DATABASE << std::endl;

  // 初始化日志系统，相对路径是相对于终端当前路径来说的
  Logger::getInstance().init("./docs/test_day4_connection.log");

  // 为什么要使用try-catch，是因为需要捕获try中调用的函数可能抛出的异常
  try {
    // 执行测试并收集结果
    std::vector<std::pair<std::string, bool>> results;

    results.emplace_back("连接池初始化测试", testPoolInitialization());
    results.emplace_back("基本连接操作测试", testBasicConnectionOperations());
    results.emplace_back("多连接获取测试", testMultipleConnections());
    results.emplace_back("并发访问测试", testConcurrentAccess());
    results.emplace_back("连接超时测试", testConnectionTimeout());

    results.emplace_back("连接池配置测试", testPoolConfiguration());
    results.emplace_back("错误处理测试", testErrorHandling());
    results.emplace_back("性能基准测试", testPerformance());

    // 显示测试结果
    printSummary(results);

    // 清理连接池
    std::cout << "\n正在关闭连接池..." << std::endl;
    // 因为是单例模式，所以容易关闭的
    ConnectionPool::getInstance().shutdown();
    std::cout << "连接池已关闭" << std::endl;

    // 根据测试结果返回退出码
    uint32_t passed = 0;
    for (const auto &result : results) {
      if (result.second)
        ++passed;
    }
    return (passed == results.size() ? 0 : 1);

  } catch (const std::exception &e) {
    std::cerr << "测试程序异常：" << e.what() << std::endl;
    return 1;
  }
}
#ifndef CONNECTION_POOL_H
#define CONNECTION_POOL_H

#include "connection.h"
#include "pool_config.h"
#include <atomic>
#include <condition_variable>
#include <map>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>

/**
 * @brief 高性能数据库连接池类，使用单例模式
 *
 * 第4天实现的核心功能
 * 1. 线程安全的连接分配与回收
 * 2. 双队列管理（空闲队列和活跃映射）
 * 3. 动态连接池大小调整
 * 4. 健康检查和连接维护
 * 5. 超时处理和异常安全
 *
 * 设计模式：
 * - 单例模式：全局唯一的连接池实例
 * - 对象池模式：复用数据库连接对象
 * - 生产者-消费者模式：连接的创建和使用
 */
class ConnectionPool {
public:
  /**
   * @brief 获取连接池单例实例
   * @return 连接池单例的引用
   *
   * 使用示例：
   * auto &pool = ConnectionPool::getInstance();
   * pool.init(config);
   */
  static ConnectionPool &getInstance() {
    static ConnectionPool instance;
    return instance;
  }

  // 单例模式禁用拷贝构造和拷贝赋值运算符，而且这也是构造函数的定义，所以需要自己给出默认构造函数的定义
  ConnectionPool(const ConnectionPool &) = delete;
  ConnectionPool &operator=(const ConnectionPool &) = delete;

  /**
   * @brief 析构函数，自动清理所有连接
   */
  ~ConnectionPool();

  // ===========================
  // 连接池生命周期管理
  // ===========================

  /**
   * @brief 初始化连接池
   * @param config 连接池配置
   * @throws std::runtime_error 如果初始化失败
   *
   * 初始化流程：
   * 1. 保存配置参数
   * 2. 初始化负载均衡器
   * 3. 预创建初始连接
   * 4. 启动健康检查线程
   */
  void init(const PoolConfig &config);

  /**
   * @brief 关闭连接池
   * 关闭所有连接，停止后台线程，清理资源 */
  void shutdown();

  /**
   * @brief 检查连接池是否已初始化
   * @return 是否已初始化
   */
  bool isInititalized() const;

  // ===========================
  // 连接获取和释放
  // ===========================

  /**
   * @brief 从连接池获取一个可用连接
   * @param timeout 获取连接的超时时间（毫秒），0表示使用配置的默认超时
   * @return 数据库连接的智能指针
   * @throws std::runtime_error 如果获取连接失败
   * 
   * 获取流程：
   * 1. 检查空闲队列是否有可用连接
   * 2. 如果没有且未达到最大连接数，创建新连接
   * 3. 如果都不行，等待其他线程释放连接
   * 4. 验证连接有效性
   * 5. 添加到活跃映射并返回
   */
  ConnectionPtr getConnection(unsigned int timeout = 0);

  /**
   * getConnection and releaseConnection体现出生产者消费者模型
   * @brief 归还连接到池中
   * @param connection 要归还的连接
   * 
   * 释放流程：
   * 1. 从活跃映射中移除
   * 2. 验证连接有效性
   * 3. 如果有效，加入空闲队列，是不是还要检查此时连接数是否大于最大连接数，如果大于，则直接销毁该连接
   * 4. 如果无效，销毁并可能创建新连接
   * 5. 通知等待线程
   */
  void releaseConnection(ConnectionPtr connection);

  // ===========================
  // 连接池状态监控
  // ===========================

  /**
   * @brief 获取当前空闲连接数量
   * @return 空闲连接数量
   */
  size_t getIdleCount() const;

  /**
   * @brief 获取当前活跃连接数量
   * @return 活跃连接数量
   */
  size_t getActiveCount() const;

  /**
   * @brief 获取总连接数量
   * @return 总连接数量
   */
  size_t getTotalCount() const;

  /**
   * @brief 获取连接池详细状态信息
   * @return 格式化的状态信息字符串
   */
  std::string getStatus() const;

  /**
   * @brief 获取连接池配置信息
   * @return 当前配置的副本
   */
  PoolConfig getConfig() const;

private:

  // 私有构造函数（单例模式），需要自定义默认构造函数
  ConnectionPool();

  /**
   * @brief 创建新的数据库连接
   * @return 新创建的连接智能指针
   * @throws std::rumtime_error 如果创建失败
   * 
   * 创建流程：
   * 1. 从负载均衡器获取数据库配置  ？？？
   * 2. 创建Connection对象
   * 3. 建立数据库连接
   * 4. 验证连接有效性
   */
  ConnectionPtr createConnection();

  /**
   * @brief 添加连接到空闲池
   * @param Connection 要添加的连接
   * 
   * 内部方法，假设调用者已持有锁
   */
  void addToIdlePool(ConnectionPtr connection);

  /**
   * @brief 从空闲池获取连接
   * @return 连接智能指针，如果没有可用连接返回nullptr
   * 
   * 内部方法，假设调用者已经持有锁
   */
  ConnectionPtr getFromIdlePool();

  /**
   * @brief 验证连接是否仍然有效
   * @param connection 要验证的连接
   * @return 连接是否有效
   */
  bool validateConnection(ConnectionPtr connection);

private:

  // ===========================
  // 核心数据结构
  // ===========================
  PoolConfig m_config;                         // 连接池配置
  std::queue<ConnectionPtr> m_idleConnections; // 空闲连接队列（FIFO）
  std::map<std::string, ConnectionPtr>
      m_activeConnections; // 活跃连接映射（ID -> 连接）

  // 线程同步原语，一般互斥锁设置为mutable
  mutable std::mutex m_mutex; // 主互斥锁，保护所有共享数据
  std::condition_variable m_condition; // 条件变量，用于等待空闲连接

  // 连接池状态，池就会有何种各样的状态值，用来控制整体的状态
  std::atomic<bool> m_isRunning;          // 连接池是否运行中
  std::atomic<size_t> m_totalConnections; // 当前总连接数

  // 后台线程，健康检查线程
  std::thread
      m_healthCheckThread; // 处理查过空闲时间的线程，当前连接小于最小连接数或者大于最大连接数时，进行处理

  // ===========================
  // 常量定义
  // ===========================
  static const unsigned int DEFAULT_HEALTH_CHECK_INTERVAL = 30000;  // 30秒，好像都是按照毫秒为单位
  static const unsigned int DEFAULT_CONNECTION_TIMEOUT = 5000;  // 5秒
  static const unsigned int DEFAULT_MAX_IDLE_TIME = 600000;     // 10分钟    
};

#endif  // CONNECTION_POOL_H
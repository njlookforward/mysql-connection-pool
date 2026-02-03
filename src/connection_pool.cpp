#include "connection_pool.h"
#include "utils.h"
#include <algorithm>
#include <stdexcept>

// ===========================
// 构造和析构函数
// ===========================

ConnectionPool::ConnectionPool() : m_isRunning(false), m_totalConnections(0) {
  LOG_DEBUG("ConnectionPool instance created");
}

ConnectionPool::~ConnectionPool() {
  LOG_INFO("ConnectionPool destructor called");
  shutdown();
}

// ===========================
// 连接池生命周期管理
// ===========================
void ConnectionPool::init(const PoolConfig &config) {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (m_isRunning) {
    LOG_WARNING("Connection Pool is already initialized");
    return;
  }

  // 验证配置
  if (!config.isValid()) {
    throw std::runtime_error("Invalid pool configuration");
  }

  m_config = config;

  LOG_INFO("Initializing connection pool with config: " +
           m_config.getSummary());

  try {
    // 处理数据库配置
    if (!m_config.dbInstances.empty()) {
      // 多数据库配置，但是按照第一个数据库来使用，也就是转换成一个数据库配置来使用，目前暂时不支持多数据库模式
      LOG_WARNING("Multi-database configuration detected but not supported in "
                  "Day 4 version. Using first database instance.");
      auto &firstDb = m_config.dbInstances[0];
      m_config.host = firstDb.host;
      m_config.user = firstDb.user;
      m_config.password = firstDb.password;
      m_config.database = firstDb.database;
      m_config.port = firstDb.port;
    }

    LOG_INFO("Using database: " + m_config.user + "@" + m_config.host + ":" +
             std::to_string(m_config.port) + "/" + m_config.database);

    LOG_INFO("Connection pool initializing with min=" +
             std::to_string(m_config.minConnections) +
             ", max=" + std::to_string(m_config.maxConnections));

    // 预创建连接 - 修复这里的bug （注意这里有作者留下的BUG）
    size_t targetConnections =
        std::min(m_config.initConnections, m_config.maxConnections);
    size_t createdConnections = 0;

    for (size_t i = 0; i < targetConnections; ++i) {
      try {
        // 目前的疑问：为什么在这个函数中没有考虑线程安全?南江，你眼瞎了吗？这个函数的第一行就是加锁lock_guard<mutex>
        ConnectionPtr conn = createConnection();
        if (conn) {
          addToIdlePool(conn);
          m_totalConnections++;
          createdConnections++;
        }
      } catch (const std::exception &e) {
        LOG_ERROR("Failed to create initial connection " + std::string(i + 1) +
                  ": " + e.what());
        // 继续创建其他连接，不因为一个失败而整体失败
      }
    }

    // 检查是否至少创建了一些连接
    if (createdConnections == 0 && targetConnections > 0) {
      throw std::runtime_error("Failed to create any initial connections");
    }

    if (createdConnections < m_config.minConnections) {
      LOG_WARNING("Created " + std::to_string(createdConnections) +
                  "connections, less than minimum required(" +
                  std::to_string(m_config.minConnections))
    }

    LOG_INFO("Successful created " + std::to_string(m_totalConnections) +
             " out of " + std::to_string(targetConnections) +
             " requested initial connections.");

    m_isRunning = true;

    // 启动健康检查线程
    // TODO: 暂时禁用健康检查线程，后续实现
    // m_healthCheckThread = std::thread(&ConnectionPool::healthCheckWorker,
    // this);
    // 之所以工业级的项目很复杂，就是因为会插入很多的日志记录和异常处理，就会让整体的逻辑非常地割裂
    // LOG_INFO("Health check thread started");

    // 我希望随着我的练习，我能够很放松地去编写代码，去享受编写代码，不要怕写错，不要怕写得丑，随着时间，随着练习的增多，我会越来越强的
    LOG_INFO("Health check thread disabled in current vision");

    LOG_INFO("Connection pool initialization completed successfully");

  } catch (const std::exception &e) {
    // 初始化失败，清理已经创建的资源
    m_isRunning = false;

    // 清理已经创建的连接
    while (!m_idleConnections.empty()) {
      auto conn = m_idleConnections.front();
      m_idleConnections.pop();
      if (conn) {
        conn->close(); // 如果失败，外层catch会处理
      }
    }
    m_totalConnections = 0;

    LOG_ERROR("Connection pool initializetion failed: " +
              std::string(e.what()));
    throw; // ###疑问：还是那个问题，单独的throw是什么意思，将捕获到的exception重新抛出去吗？
  }
}

void ConnectionPool::shutdown() {
  {
    // 在这个作用域内保证线程安全
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_isRunning) {
      return; // 没有运行连接池直接返回
    }

    m_isRunning = false;

    LOG_INFO("Shutting down connection pool");

    // 通知所有等待的线程
    m_condition.notify_all();
  }
#if 0
  // 等待健康检查线程结束
  if(m_healthCheckThread.joinable())
  {
    m_healthCheckThread.join();   // 健康检查线程可以直接结束
  }
#endif

  // 线程安全下关闭所有的连接
  std::lock_guard<std::mutex> lock(m_mutex);

  // 关闭空闲连接
  while (!m_idleConnections.empty()) {
    auto conn = m_idleConnections.front();
    m_idleConnections.pop();
    if (conn) {
      conn->close();
    }
  }

  // 关闭活跃连接
  for (auto &pair : m_activeConnections) {
    pair.second->close();
  }
  m_activeConnections.clear();

  // 只有将空闲与活跃的连接都关闭了，才能说明所有连接都关闭了
  m_totalConnections = 0;
  LOG_INFO("All connections closed");

  LOG_INFO("Connection pool shutdown completed");
}

bool ConnectionPool::isInititalized() const {
  // m_isRunning是是否完成初始化的标志
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_isRunning;
}

// ===========================
// 连接获取与释放
// ===========================

// 为什么获取连接不需要加锁？是因为默认调用getConnection函数的时候，已经加锁了吗
// 回答：要加锁的，只是加锁的时机在后面，这样缩小临界区，可以减少开销
ConnectionPtr ConnectionPool::getConnection(unsigned int timeout) {
  if (!m_isRunning) {
    throw std::runtime_error(
        "Connection pool is not initialized or has been shutdown");
  }

  // 使用默认配置的超时时间，如果参数为0
  if (timeout == 0) {
    timeout = m_config.connectionTimeout;
  }

  std::unique_lock<std::mutex> lock(m_mutex);

  // 计算超时时间点
  auto timeoutPoint =
      std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout);

  LOG_DEBUG("Attempting to acquire connection. timeout=" +
            std::to_string(timeout) + "ms");

  while (true) {
    // 检查连接池是否已关闭，每次获取连接的前提是连接池已经完成初始化
    if (!m_isRunning) {
      throw std::runtime_error("Connection pool has been shut down");
    }

    // 1. 尝试从空闲队列获取连接
    ConnectionPtr conn = getFromIdlePool();
    if (conn) {
      // 验证连接有效性
      if (validateConnection(conn)) {
        // 添加到活跃映射
        m_activeConnections[conn->getConnectionId()] = conn;
        conn->updateLastActiveTime();

        LOG_DEBUG("Connection acquired from idle pool: " +
                  conn->getConnectionId());
        return conn;
      } else {
        // 连接无效，销毁并继续寻找
        LOG_WARNING("Invalid connection removed from idle pool: " +
                    conn->getConnectionId());
        m_totalConnections--;
        // 继续循环尝试获取其他连接
        continue;
      }
    }
    std::cout << "=============" << m_totalConnections << " "
              << m_config.maxConnections << std::endl;
    // 2. 空闲队列为空，且总连接少于最多允许连接数，尝试创建新连接
    if (m_totalConnections < m_config.maxConnections) {
      try {
        // 释放锁，创建连接（避免阻塞其他操作）
        // 通过使用unqie_lock灵活的改变临界区大小
        lock.unlock();
        conn = createConnection();
        lock.lock();

        if (conn) {
          m_totalConnections++;
          m_activeConnections[conn->getConnectionId()] = conn;
          conn->updateLastActiveTime();

          LOG_DEBUG("New connection created and acquired: " +
                    conn->getConnectionId());
          return conn;
        }
      } catch (const std::exception &e) {
        // 重新获取锁（如果在创建过程中释放了）
        // ### 疑问owns_lock()是什么意思
        if (!lock.owns_lock()) {
          lock.lock();
        }

        LOG_ERROR("Failed to create new connection: " + std::string(e.what()));
        // 继续等待现有连接释放
      }
    }

    // 3. 无法获取连接，等待其他线程释放连接
    LOG_DEBUG("No available connections, waiting for release...");

    // 等待条件变量信号或超时
    // ### 疑问：这是带超时的因为条件变量的线程阻塞
    auto waitResult = m_condition.wait_until(lock, timeoutPoint);
    if (waitResult == std::cv_status::timeout) {
      throw std::runtime_error(
          "Timeout waiting for available connections after " +
          std::to_string(timeout) + "ms");
    }

    // 被唤醒，继续循环尝试获取连接
  }
}

// 南江，请相信自己，one
// more，再多写一个函数，再多设计一个函数，你一定会成为一个优秀的软件设计师
void ConnectionPool::releaseConnection(ConnectionPtr connection) {
  if (!connection) {
    LOG_WARNING("Attempted to release null connection");
    return;
  }

  std::lock_guard<std::mutex> lock(m_mutex);

  if (!m_isRunning) {
    // 连接池已关闭，直接关闭连接
    LOG_DEBUG("Connection pool shut down, closing released connection: " +
              connection->getConnectionId());
    connection->close();
    return;
  }

  std::string connId = connection->getConnectionId();
  LOG_DEBUG("Releasing connection: " + connId);

  // 从活跃映射中移除
  auto it = m_activeConnections.find(connId);
  if (it != m_activeConnections.end()) {
    m_activeConnections.erase(it);
    LOG_DEBUG("Connection removed from active map: " + connId);
  } else {
    // 这应该是不可能的事情，如果发生只能说明我的设计很有问题
    LOG_WARNING("Connection not found in active map: " + connId);
  }

  // 验证连接有效性
  if (validateConnection(connection)) {
    // 连接有效，添加回空闲队列
    addToIdlePool(connection);
    LOG_DEBUG("Connection returned to idle pool: " + connId);
  } else {
    // 连接无效，销毁
    LOG_WARNING("Invalid connection destroyed: " + connId);
    connection->close();
    m_totalConnections--;

    // 如果当前连接数少于最小值，尝试创建新连接
    if (m_totalConnections < m_config.maxConnections) {
      try {
        ConnectionPtr newConn = createConnection();
        if (newConn) {
          addToIdlePool(newConn);
          m_totalConnections++;
          LOG_DEBUG("Replacement connection created: " +
                    newConn->getConnectionId());
        }
      } catch (const std::exception &e) {
        LOG_ERROR("Failed to create replacement connection: " +
                  std::string(e.what()));
      }
    }
  }

  // 通知等待获取空闲连接的线程
  m_condition.notify_one();
}

// ===========================
// 连接池状态监控
// ===========================

size_t ConnectionPool::getIdleCount() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_idleConnections.size();
}

size_t ConnectionPool::getActiveCount() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_activeConnections.size();
}

// 南江，请继续加油，你一定可以将ConnectionPool这个类的设计学完，学会
// ### BUG 对于原子变量的读取，需要使用load，而不需要进行加锁
size_t ConnectionPool::getTotalCount() const {
  return m_totalConnections.load();
}

std::string ConnectionPool::getStatus() const {
  std::lock_guard<std::mutex> lock(m_mutex);

  std::string status = "ConnectionPool Status:\n";
  status += "  Running: " + std::string(m_isRunning ? "yes" : "no") + "\n";
  status +=
      "  Total Connections: " + std::to_string(m_totalConnections.load()) +
      "\n";
  status +=
      "  Idle Connections: " + std::to_string(m_idleConnections.size()) + "\n";
  status +=
      "  Active Connections: " + std::to_string(m_activeConnections.size()) +
      "\n";
  status +=
      "  Min Connections: " + std::to_string(m_config.minConnections) + "\n";
  status +=
      "  Max Connections: " + std::to_string(m_config.maxConnections) + "\n";
  status +=
      "  Connection Timeout: " + std::to_string(m_config.connectionTimeout) +
      "ms\n";
  status += "  Max Idle Time: " + std::to_string(m_config.maxIdleTime) + "ms\n";

  return status;
}

PoolConfig ConnectionPool::getConfig() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_config;
}

// ===========================
// 私有方法实现
// ===========================

ConnectionPtr ConnectionPool::createConnection() {
  // 注意：此方法通常在持有锁的情况下被调用，或者调用者需要处理线程安全，意思是调用者需要加锁后再调用该函数
  LOG_DEBUG("Creating new database connection");

  try {
    // 直接使用连接池配置中的数据库信息
    LOG_DEBUG("Using database: " + m_config.user + "@" + m_config.host + ":" +
              std::to_string(m_config.port) + "/" + m_config.database);

    // 创建连接对象，传入连接参数
    ConnectionPtr conn = std::make_shared<Connection>(
        m_config.host, m_config.user, m_config.password, m_config.database,
        m_config.port, m_config.reconnectInterval, m_config.reconnectAttemps);

    // 建立数据库连接
    if (!conn->connect()) {
      std::string error =
          std::string("Failed to establish database connection: " +
                      mysql_error(conn->getMysqlHandle()));
      LOG_ERROR(error);
      throw std::runtime_error(error);
    }

    LOG_DEBUG("New connection created successfully: " +
              conn->getConnectionId());
    return conn;

  } catch (const std::exception &e) {
    LOG_ERROR("Failed to create connection: " + std::string(e.what()));
    throw; // 继续向上抛出异常
  }
}

void ConnectionPool::addToIdlePool(ConnectionPtr connection) {
  // 注意：此方法假设调用者已经持有锁

  // 1. 先检验传入的连接参数是否有效
  if (!connection) {
    LOG_WARNING("Attempted to add null connection to idle pool");
    return;
  }

  // 验证连接有效性
  if (validateConnection(connection)) {
    m_idleConnections.push(connection);
    LOG_DEBUG("Connection added to idle pool: " +
              connection->getConnectionId());
  } else {
    LOG_WARNING("Invalid connection not added to idle pool: " +
                connection->getConnectionId());
    connection->close();
    // ###疑问：这里总的连接数减一，那么在外面是不是就不能连接数再减一了，这里一定要注意
    // ###回答：就应该是这样的：1）添加的是释放的空闲连接，如果已经失效，那么就应该m_totalConnections--
    // 2) 如果添加的是新创建的连接，那么如果是无效的连接，m_totalConnections--也是正确的，因为每一个调用（添加新创建的连接）的后面，都有一个m_totalConnections++
    // 所以无论添加的是什么连接到空闲队列中，如果无效都应该m_totalConnections--
    m_totalConnections--;
  }
}

// 南江，继续加油，不要被外界所动，坚持你的道，你的软件设计之路
// 私有方法基本都假设调用者已经持有锁
ConnectionPtr ConnectionPool::getFromIdlePool() {
  // 注意：此方法假设调用者已经持有锁

  if (m_idleConnections.empty()) {
    return nullptr; // 这里是隐式类型转换，转换成shared_ptr<Connection>
  }
  auto conn = m_idleConnections.front();
  m_idleConnections.pop();

  LOG_DEBUG("Connection retrieved from idle pool: " + conn->getConnectionId());
  return conn;
}

bool ConnectionPool::validateConnection(ConnectionPtr connection) {
  if (!connection) {
    return false;
  }

  try {
    // 使用重连的isValid方法，允许自动重连
    bool isValid = connection->isValid(true);

    if (isValid) {
      LOG_DEBUG("Connection validation passed: " +
                connection->getConnectionId());
    } else {
      LOG_WARNING("Connection validation failed: " +
                  connection->getConnectionId());
    }

    return isValid;

  } catch (const std::exception &e) {
    LOG_WARNING("Connection validation error for " + connection->getConnectionId() + ": " + std::string(e.what()));
    return false;
  }
}
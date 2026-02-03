#ifndef CONNECTION_H
#define CONNECTION_H

#include <string>
#include <mysql/mysql.h>
#include <memory>
#include "query_result.h"

/**
 * @brief 数据库连接类，负责管理单个数据库连接
 * 
 * 设计特点：
 * 1) RAII资源管理：构造时建立连接，析构时释放资源
 * 2) 线程安全：使用互斥锁保护并发访问
 * 3) 异常安全：所有操作都有适当的错误处理
 * 4）易用接口：提供简洁的数据库操作方法
 * 
 * 注意：这是第2天的基础版本，后续会添加重连功能
 */
/**
 * @brief 第三天：增强版数据库连接类，支持自动重连
 * 
 * 第3天新增功能：
 * 1. 智能错误识别：区分连接错误和业务错误
 * 2. 自定义重连逻辑：支持多次重试和指数退避
 * 3. 连接状态监控：实时监控连接健康状态
 * 4. 带重连的查询执行：查询失败时自动重连重试
 */
class Connection
{
public:
    /**
     * @brief 构造函数，使用给定参数进行初始化，创建连接
     * 新增重连间隔和最大重连尝试次数
     */
    Connection(const std::string &host, const std::string &user,
               const std::string &password, const std::string &database,
               unsigned int port = 3306,
               unsigned int reconnectInterval = 1000,
               unsigned int reconnectAttempts = 3);
     
    /**
     * @brief 析构函数
     * 遵守RAII，回收资源，释放连接
     */
    ~Connection();

    /**
     * @brief 因为连接资源是唯一绑定到连接器上，所以避免重复释放资源，应该删除拷贝语义
     */
    Connection(const Connection &) = delete;
    Connection &operator=(const Connection &) = delete;
    
    /**
     * @brief 资源可以转移，因此具备移动语义
     * ### 必须搞清楚为什么添加noexcept，什么时候添加noexcept，添加后有什么好处？
     */
    Connection(Connection &&other) noexcept;
    Connection &operator=(Connection &&other) noexcept;

    // =============================
    // 连接管理方法
    // =============================
    
    /**
     * @brief 这是连接管理的第一步，连接到数据库。根据给定的参数建立连接，并记录连接建立的时间
     * @return 连接是否建立成功
     * 
     * 使用示例：
     * Connection conn("Localhost", "user", "pass", "testdb");
     * if(conn.connect())
     * {
     *      // 可以执行建立MySQL连接后的操作
     * }
     */
    bool connect();
    
    /**
     * @brief 使用自定义重连逻辑尝试重新连接
     * @return 是否成功重连
     * 
     * 重连特点：
     * 1. 支持多次重试
     * 2. 使用指数退避算法
     * 3. 详细的日志记录
     * 4. 线程安全
     */
    bool reconnect();

    /**
     * @brief 关闭数据库连接
     * 通常不需要手动调用，析构函数会自动调用
     */
    void close();

    /**
     * @brief 检查连接是否有效
     * @param tryReconnect 如果连接无效是否进行重连
     * @return 返回连接是否有效且可用
     * 因为需要更新最新的活动时间，因此不能设置为const
     * @todo 到底要不要定义为const成员函数，TODO 需要继续看
     * 不能定义为const成员函数，因为如果可以尝试重连，那么很多的数据成员都会被修改，因此不能
     */
    bool isValid(bool tryReconnect = false);

    // =============================
    // 查询执行方法
    // day3 增强的查询执行方法
    // =============================

    /**
     * @brief 执行SELECT查询语句（带自动重连）
     * @return 查询结果的智能指针
     * @throws std::runtime_error，如果查询失败且无法重连
     * 
     * 执行流程：
     * 1. 执行查询操作
     * 2. 如果失败，且是连接错误，多次进行重连
     * 3. 重连成功后继续执行查询操作
     * 4. 如果多次重试失败后，抛出异常
     * 
     * 使用示例：
     * auto reuslt = conn.executeQuery("SELECT * FROM users WHERE age > 18");
     * while(result->next())
     * {
     *      std::cout << result->getString("name") << std::endl;
     * }
     */
    QueryResultPtr executeQuery(const std::string &sql);

    /**
     * @brief 执行更新操作（INSERT/DELETE/UPDATE)，带自动重连
     * @param sql语句
     * @return affectedRows受影响的行数
     * @throws std::runtime_error 如果执行失败且无法重连
     * 
     * 执行逻辑，与查询操作是一样的
     * 1. 执行更新操作（INSERT/UPDATE/DELETE)
     * 2. 如果失败且是连接错误，尝试重连
     * 3. 重连成功后继续执行更新操作
     * 4. 多次尝试失败后，抛出异常
     * 
     * 使用示例
     * auto affected = conn.executeUpdate("UPDATE users SET statu = 1 WHERE name = 'tom'")
     * ### 疑问：在使用mysqlclient时，这些sql语句最后不用使用;作为结尾吗？
     * std::cout << "Updated " << affected << " rows." << std::endl;
     */
    unsigned long long executeUpdate(const std::string &sql);

    // =============================
    // 事务管理方法 ### 重点
    // day3 事务管理方法（增强版）
    // =============================
    
    /**
     * @brief 开始执行事务
     * @return 是否成功开始事务
     * 
     * 使用示例：
     * conn.beginTransaction();
     * try
     * {
     *     conn.executeUpdate("INSERT INTO users...");
     *     conn.executeUpdate("UPDATE accounts...");
     *     conn.commit();   // 提交事务
     * } catch(...)
     * {
     *     conn.rollback(); // 事务具有原子性，任何其中一条语句没有执行成功，那么就需要回滚到事务执行前的状态
     * }
     */
    bool beginTransaction();

    /**
     * @brief 事务提交
     * @return 返回是否成功提交事务
     */
    bool commit();

    /**
     * @brief 事务回滚
     * @return 是否成功回滚
     */
    bool rollback();

    // =============================
    // 错误处理方法
    // =============================

    /**
     * @brief 获取上次操作的错误信息
     * @return 错误信息字符串
     */
    // std::string getLastError() const;

    /**
     * @brief 获取上次操作的错误代码
     * @return MySQL错误代码
     */
    // unsigned int getLastErrorCode() const;

    // day3 新增方法，为什么要将上面的得到错误信息和错误码进行注释呢？我认为不应该注释掉
    /**
     * @brief 检查是否是连接断开错误
     * @param errorCode 错误代码
     * @return 是否是连接断开错误
     * 
     * 支持的MySQL连接断开错误码
     * - 2002: CR_CONNECTION_ERROR  无法连接到MySQL服务器
     * - 2003: CR_CONN_HOST_ERROR   无法连接到指定主机
     * - 2006: CR_SERVER_GONE_ERROR MySQL服务器已经关闭连接
     * - 2013: CR_SERVER_LOST       ## 很奇怪：查询过程中与服务器连接丢失
     * - 2027：CR_MALFORMED_PACKET  收到格式错误的数据包
     * - 2055：CR_SERVER_LOST_EXTENDED  扩展的服务器连接丢失
     */
    bool isConnectionError(unsigned int errorCode) const;

    // =============================
    // 工具方法
    // =============================

    /**
     * @brief 转义字符串，防止SQL注入
     * @param inputSQL
     * @return 转义后的字符串
     */
    std::string escapeString(const std::string &sql);

    /**
     * @brief 获取连接创建时间
     * @return 毫秒级的时间戳(创建时间)
     */
    int64_t getCreationTime() const;

    /**
     * @brief 获取最后活动时间
     * @return 毫秒级的时间戳
     */
    int64_t getLastActiveTime() const;

    /**
     * @brief 更新最后活动时间
     * 每次使用连接时都会调用这种方法
     * ### 我将lastactiveTime设置为mutable
     */
    void updateLastActiveTime() const;

    /**
     * @brief 获取连接标识符
     * @return mysql连接的唯一标识符
     */
    std::string getConnectionId() const;

    // =============================
    // day3：新增重连统计方法
    // =============================

    /**
     * @brief 获取总的重连尝试次数
     */
    unsigned int getTotalReconnectAttempts() const;

    /**
     * @brief 获取成功重连的次数
     */
    unsigned int getSuccessfulReconnects() const;

    /**
     * @brief 重置重连统计
     */
    void resetReconnectStatus();

    /**
     * @brief 得到数据库连接句柄
     * @note 这是新添加的接口
     */
    MYSQL * getMysqlHandle() {
        return m_mysql;
    }

private:
    // =============================
    // 私有方法
    // =============================
    
    /**
     * @brief 初始化MySQL连接对象
     * 设置连接选项和参数
     */
    void init();

    /**
     * @brief 执行查询操作的内部方法，带有重连逻辑
     * @param SQL语句
     * @param isQuery 是否是查询操作
     * @return SQL操作结果的智能指针
     * 
     * 这是第3天的核心新功能
     * 1. 自动识别连接错误
     * 2. 智能重连机制
     * 3. 重连逻辑
     */
    QueryResultPtr executeWithReconnect(const std::string &sql, bool isQuery);

    /**
     * @brief 执行SQL语句的内部方法 ### 疑问：这是什么意思，什么SQL语句的内部方法
     * @param SQL语句
     * @param 是否是查询操作
     * @return 查询结果的智能指针
     */
    QueryResultPtr executeInternal(const std::string &sql, bool isQuery);

    /**
     * @brief 计算重连的延迟时间（指数退避算法）
     * @param attempt 重连次数（从1开始）
     * @return 延迟时间（毫秒）
     */
    unsigned int calculateReconnectDelay(unsigned int attempt) const;

private:
    // =============================
    // 私有数据成员
    // =============================
    MYSQL *m_mysql;                     // mysql连接句柄
    std::string m_host;                 // 主机名
    std::string m_user;                 // 用户名
    std::string m_password;             // 密码
    std::string m_database;             // 数据库名
    unsigned int m_port;                // 端口号
    std::string m_connectionId;         // 连接唯一标识符
    int64_t m_creationTime;             // 连接创建时间
    mutable int64_t m_lastActiveTime;   // 连接最后活动时间
    // mutable std::recursive_mutex m_mutex;         // 互斥锁，保证线程安全
    mutable std::mutex m_mutex;
    bool m_connected;                   // 是否已经建立连接
    
    // =============================
    // 新增的重连相关参数
    // =============================
    unsigned int m_reconnectInterval;  // 重连间隔（毫秒）
    unsigned int m_reconnectAttempts;   // 最大重连尝试次数

    // =============================
    // 重连统计
    // =============================
    unsigned int m_totalReconnectAttempts;  // 总的重连尝试次数
    unsigned int m_successfulReconnects;    // 成功的重连次数

};

// 智能指针类型别名
using ConnectionPtr = std::shared_ptr<Connection>;

#endif
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
class Connection
{
public:
    /**
     * @brief 构造函数，使用给定参数进行初始化，创建连接
     */
    Connection(const std::string &host, const std::string &user,
               const std::string &password, const std::string &database,
               unsigned int port = 3306);
     
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
     * @brief 关闭数据库连接
     * 通常不需要手动调用，析构函数会自动调用
     */
    void close();

    /**
     * @brief 检查连接是否有效
     * @return 返回连接是否有效且可用
     * 因为需要更新最新的活动时间，因此不能设置为const
     */
    bool isValid() const;

    // =============================
    // 查询执行方法
    // =============================

    /**
     * @brief 执行SELECT查询语句
     * @return 查询结果的智能指针
     * @throws std::runtime_error，如果查询失败
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
     * @brief 执行更新操作（INSERT/DELETE/UPDATE)
     * @param sql语句
     * @return affectedRows受影响的行数
     * @throws std::runtime_error 如果执行失败
     * 
     * 使用示例
     * auto affected = conn.executeUpdate("UPDATE users SET statu = 1 WHERE name = 'tom'")
     * ### 疑问：在使用mysqlclient时，这些sql语句最后不用使用;作为结尾吗？
     * std::cout << "Updated " << affected << " rows." << std::endl;
     */
    unsigned long long executeUpdate(const std::string &sql);

    // =============================
    // 事务管理方法 ### 重点
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
    std::string getLastError() const;

    /**
     * @brief 获取上次操作的错误代码
     * @return MySQL错误代码
     */
    unsigned int getLastErrorCode() const;

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
     * @brief 执行SQL语句的内部方法 ### 疑问：这是什么意思，什么SQL语句的内部方法
     * @param SQL语句
     * @param 是否是查询操作
     * @return 查询结果的智能指针
     */
    QueryResultPtr executeInternal(const std::string &sql, bool isQuery);

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
    mutable std::recursive_mutex m_mutex;         // 互斥锁，保证线程安全
    bool m_connected;                   // 是否已经建立连接
};

// 智能指针类型别名
using ConnectionPtr = std::shared_ptr<Connection>;

#endif
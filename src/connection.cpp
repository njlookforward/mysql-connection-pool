#include "connection.h"
#include "utils.h"
#include "db_exception.h"
#include <stdexcept>
#include <thread>
#include <random>

/**
 * @brief 这是连接类的基础实现
 */

// =============================
// 构造函数和析构函数
// =============================

Connection::Connection(const std::string &host, const std::string &user,
                       const std::string &password, const std::string &database,
                       unsigned int port,
                       unsigned int reconnectInterval, unsigned int reconnectAttempts)
    : m_mysql(nullptr), m_host(host), m_user(user), m_password(password), m_database(database), m_port(port), m_connectionId(Utils::generateRandomString(16)), m_creationTime(Utils::currentTimeMillis()), m_lastActiveTime(m_creationTime), m_connected(false), m_reconnectInterval(reconnectInterval), m_reconnectAttempts(reconnectAttempts), m_totalReconnectAttempts(0), m_successfulReconnects(0)
{
    // ###BUG 没有将整数转换成字符串，进行相加
    LOG_INFO("Creating enhanced connection [" + m_connectionId + "] to " +
             m_user + "@" + m_host + ":" + std::to_string(m_port) + "/" + m_database +
             ", reconnect config: interval=" + std::to_string(m_reconnectInterval) +
             "ms, attempts=" + std::to_string(m_reconnectAttempts));
    // 初始化连接对象
    init();
}

// 析构函数也需要更新，需要打印connection生命周期中的重连尝试信息
Connection::~Connection()
{
    close();
    m_connected = false;
    // ###BUG 同样，整数没有转换成字符串进行相加
    LOG_INFO("Destroying connection object [" + m_connectionId + "], reconnect stats: " +
             "totalReconnectAttempts=" + std::to_string(m_totalReconnectAttempts) +
             ", successful=" + std::to_string(m_successfulReconnects));
}

// =============================
// 初始化方法，这里面究竟要做些什么工作
// 设置建立连接前的准备工作，主要是各种超时时间还有字符集的设置
// =============================
void Connection::init()
{
    // 因为init在构造函数中被调用，因此不需要考虑多线程的问题，但是connect函数可能在多个线程中被调用，因此需要考虑多线程安全
    // 1. 创建连接句柄
    m_mysql = mysql_init(nullptr);
    if (!m_mysql)
    {
        std::string error = "Failed to initialize MYSQL connection object.";
        LOG_ERROR(error);
        throw std::runtime_error(error);
    }

    // 2. 设置超时时间
    unsigned int timeout = 5; // 超时时间为5秒
    if (mysql_options(m_mysql, MYSQL_OPT_CONNECT_TIMEOUT, &timeout) != 0)
    {
        // 超时时间设置失败，不影响后续的操作，只是没有连接超时的限制了，所以为LOG_WARNING
        LOG_WARNING("Failed to set connection timeout");
    }

    // 3. 设置读的超时时间
    unsigned int readTimeout = 30; // 读超时时间为30s
    if (mysql_options(m_mysql, MYSQL_OPT_READ_TIMEOUT, &readTimeout) != 0)
    {
        LOG_WARNING("Failed to set read timeout");
    }

    // 4. 设置写的超时时间
    unsigned int writeTimeout = 30;
    if (mysql_options(m_mysql, MYSQL_OPT_WRITE_TIMEOUT, &writeTimeout) != 0)
    {
        LOG_WARNING("Failed to set write timeout");
    }

    // 5. 设置字符集为utf8mb4，支持emoji等4字节字符
    // ### BUG 没有utf8mb4的字符集，只有utf8，即使改为utf8，也继续报错，没有修复好
    // if (mysql_options(m_mysql, MYSQL_SET_CHARSET_NAME, "uft8") != 0)
    // {
    //     // LOG_WARNING("Failed to set charset to utf8mb4");
    //     LOG_WARNING("Failed to set charset to utf8");
    // }

    // 6. 设置可以同时执行多条语句
    // 大多数连接池场景不需要在单个调用中执行多条SQL语句，而且多语句功能存在SQL注入风险
    // bool enable = true;
    // if(mysql_options4(m_mysql, MYSQL_OPTION_MULTI_STATEMENTS_ON, &enable) != 0)
    // {
    //     LOG_WARNING("Failed to set multistatement");
    // }

    // 日志记录
    LOG_INFO("MYSQL connection object initialized [" + m_connectionId + "]");
}

/**
 * @note
 * ### 连接不应该只建立一次吗？但是这里只有加锁操作，没有判断是否已经建立了连接，是否可能发生多次建立连接，而且会丢失曾经建立的连接
 * 根据MySQL C API文档，对于已经建立连接的MySQL句柄再次调用mysql_real_connect()会先关闭旧连接，再建立新连接，因此需要设置标志位，只建立一次连接就可以了
 */
bool Connection::connect()
{
    // 加锁
    std::unique_lock<std::mutex> lock(m_mutex);
    // std::unique_lock<std::recursive_mutex> lock(m_mutex);

    if (m_connected)
    {
        LOG_WARNING("connection to MySQL Server has established [" + m_connectionId + "]");
        lock.unlock();
        return true;
    }

    // 判断连接对象是否完成初始化
    if (!m_mysql)
    {
        // 如果没有完成初始化，可以继续进行初始化，但如果仍然不成功，那只能返回错误
        init();
        if (!m_mysql)
        {
            LOG_ERROR("MySQL connection object failed to initialized [" + m_connectionId + "]");
            return false;
        }
    }

    // 开始尝试进行连接
    LOG_INFO("Connecting to MySQL server [" + m_connectionId + "]");
    MYSQL *result = mysql_real_connect(
        m_mysql,
        m_host.c_str(),
        m_user.c_str(),
        m_password.c_str(),
        m_database.c_str(),
        m_port,
        nullptr, // unix_socket基本设计为nullptr
        0        // 客户端标志位选项设置为0
    );
    // 如果失败，需要日志输出错误信息
    if (result == nullptr)
    {
        // ### BUG getLastError和getLastErrorCode已经注释掉了，应该直接使用API
        std::string error = mysql_error(m_mysql);
        unsigned int errorCode = mysql_errno(m_mysql);
        // ###BUG 又是没有进行整数到字符串的转换
        LOG_ERROR("Failed to connect to MySQL Server: [" + m_connectionId + "]: " + error +
                  ", errorCode=" + std::to_string(errorCode));
        lock.unlock();
        return false;
    }
    m_connected = true;

    // 如果连接建立成功，更新连接的活动时间
    updateLastActiveTime();
    // 日志输出连接建立成功
    LOG_INFO("Successfully to connect to MySQL Server: [" + m_connectionId + "]");

    return true;
}

/**
 * @brief 尝试重连
 * @return 重连是否成功
 * 这个很有意思，就是在最大尝试次数范围内，不断地尝试重新建立连接，我需要详细地记录日志过程，而且需要能够判断那些是普通信息
 * 哪些应该归为警告信息，哪些是错误信息，哪些是debug信息
 *
 * 重连就要重置所有的连接信息，因此连接句柄也要进行重置
 */
bool Connection::reconnect()
{
    // ### BUG 又忘记线程安全的事情了
    std::unique_lock<std::mutex> lock(m_mutex);
    // std::unique_lock<std::recursive_mutex> lock(m_mutex);

    LOG_INFO("Connection object begin to attempt to reconnect [" + m_connectionId + "]");
    // ### 这里忘记了，先关闭当前的连接
    if (m_mysql)
    {
        mysql_close(m_mysql);
        m_mysql = nullptr;
        m_connected = false;
    }

    // 重新初始化连接句柄m_mysql
    init();
    if (!m_mysql)
    {
        LOG_ERROR("Reconnection failed to initialize connection [" + m_connectionId + "]");
        return false;
    }

    // 在m_reconnectAttempts范围内不断地尝试进行mysql_real_connect
    // 不要忘记更新总的重连次数和成功的重连次数
    // 如果成功，还要更新最新的活动时间
    // 先不要管记录日志，先把整体的重连逻辑梳理好，先实现一个最小可用版本
    // 使用指数退避算法进行多次尝试
    for (unsigned int attempt = 1; attempt <= m_reconnectAttempts; ++attempt)
    {
        MYSQL *result = mysql_real_connect(
            m_mysql,
            m_host.c_str(),
            m_user.c_str(),
            m_password.c_str(),
            m_database.c_str(),
            m_port,
            nullptr,
            0);

        // 增加总的重连尝试次数
        ++m_totalReconnectAttempts;

        // 成功
        if (result)
        {
            // ### 我忘记了更新连接的最新活动时间
            updateLastActiveTime();
            ++m_successfulReconnects;
            // 不要忘记是否建立连接这个值的变化
            m_connected = true;
            LOG_INFO("Success to reconnect [" + m_connectionId + "] at " + std::to_string(attempt) +
                     "/" + std::to_string(m_reconnectAttempts));
            return true;
        }
        else
        {
            // 打印错误日志，需要添加错误信息和错误码
            std::string error = mysql_error(m_mysql);
            unsigned int errCode = mysql_errno(m_mysql);
            LOG_ERROR("Failed to reconnect [" + m_connectionId + "] at " + std::to_string(attempt) +
                      "/" + std::to_string(m_reconnectAttempts) + ": " + error + " (errorCode: " + std::to_string(errCode) + ")");
            // 然后计算重连延迟时间，只有在不是最后一次重连机会的时候，进行延迟重连的操作才有意义
            if (attempt < m_reconnectAttempts)
            {
                // 随着重连尝试次数的增加，延长时间也在增加
                auto delay = calculateReconnectDelay(attempt);
                // 需要记录，### BUG 这里是字符串，需要将delay转换成字符串格式
                LOG_INFO("waiting " + std::to_string(delay) + "ms to continue next reconnection attempt [" + m_connectionId + "]");
                // 解锁
                lock.unlock();
                // 等待 ### BUG 这里有问题，需要指出睡眠时间的单位
                std::this_thread::sleep_for(std::chrono::milliseconds(delay));
                // 加锁继续下一次的重连
                lock.lock();
            }
        }
    }

    // 使用了所有的重连尝试次数，那么只能报错返回
    LOG_WARNING("Failed to reconnect [" + m_connectionId + "] with all attempt chances.");
    return false;
}

/**
 * @brief 因为有可能不仅仅在析构函数中调用close，因此需要加锁互斥访问
 */
void Connection::close()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    // std::unique_lock<std::recursive_mutex> lock(m_mutex);

    if (m_mysql)
    {
        mysql_close(m_mysql);
        m_mysql = nullptr;
        m_connected = false;
    }

    LOG_INFO("MySQL Connection closed [" + m_connectionId + "]");
}

/**
 * @brief 对连接池中的任何访问，都应该互斥加锁
 *
 * 使用mysql_ping来验证连接是否有效
 */
bool Connection::isValid(bool tryReconnect)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    // std::unique_lock<std::recursive_mutex> lock(m_mutex);

    // 因为是在建立连接的前提下，检测当前的连接是否有效，因此进行是否初始化判断和是有已经连接的判断
    if (!m_mysql)
    {
        // 对于一些不影响程序的逻辑错误，只需要添加警告日志即可
        // 不需要抛出异常
        LOG_WARNING("Please initialize mysql connection object [" + m_connectionId + "]");
        return false;
    }
    if (!m_connected)
    {
        LOG_WARNING("Please connect to MySQL Server [" + m_connectionId + "]");
        return false;
    }

    if (mysql_ping(m_mysql) == 0)
    {
        updateLastActiveTime();
        return true;
    }
    else
    {
        // 因此有重连因素的考虑，所以需要知道错误码和错误信息
        unsigned int errCode = mysql_errno(m_mysql);
        std::string error = mysql_error(m_mysql);
        // 任何mysql操作失败后，都应该打印getLastError()
        LOG_WARNING("Connection validation failed [" + m_connectionId + "]: " + error);
        // 需要判断是否需要进行重连或者不是连接错误
        if (!tryReconnect || !isConnectionError(errCode))
        {
            return false;
        }
    }

    LOG_INFO("Although connection validation failed, but attempt to reconnect [" + m_connectionId + "]");
    // ### BUG 这里又有一个大大的BUG，进入reconnect之前，需要解锁，否则一定死锁
    lock.unlock();
    return reconnect();
}

// =============================
// 查询执行方法
// @note 每次连接对象只有一个，但是可能被多个线程使用，因此必须加锁保证线程安全
// day3: 带重连的查询执行方法，使用executeWithReconnect作为query and update的通义接口
// @note 只要涉及到连接的操作，就必须通过加锁保证多线程安全
// =============================
QueryResultPtr Connection::executeQuery(const std::string &sql)
{
    // 调用内部实现的方法，应该是统一进行select and non-select操作
    // return executeInternal(sql, true);
    return executeWithReconnect(sql, true);
}

unsigned long long Connection::executeUpdate(const std::string &sql)
{
    // QueryResultPtr result = executeInternal(sql, false);
    QueryResultPtr result = executeWithReconnect(sql, false);
    return result ? result->getAffectedRows() : 0;
}

QueryResultPtr Connection::executeInternal(const std::string &sql, bool isQuery)
{
    // 之所以不能使用isValid()检验连接是否有效，是因为不能加两次锁，
    // 但是我可以先判断是否有效；然后再加锁进行后续的操作
    // 判断连接是否有效
    // ### BUG 我还是按照原来的写法，自己判断是否有连接而不是调用isValid函数
    // if (!isValid())
    // {
    //     // 日志记录规范：发生的事件 + [connectionId] + error
    //     LOG_ERROR("Connection not established [" + m_connectionId + "]");
    //     // 因为需要返回具体的结果，但是因为发生了错误，无法返回，只能抛出异常
    //     throw std::runtime_error("Connection not established [" + m_connectionId + "]");
    // }

    std::unique_lock<std::mutex> lock(m_mutex);
    // std::unique_lock<std::recursive_mutex> lock(m_mutex);
    if (!m_mysql || !m_connected)
    {
        throw db::SQLExecutionError("Connection not established", CR_SERVER_GONE_ERROR);
    }

    // 若有效
    // 记录日志：尝试进行什么操作
    LOG_DEBUG("Connection execute " + std::string(isQuery ? "query" : "update") +
              " [" + m_connectionId + "], sql: " + sql);

    updateLastActiveTime();

    // 开始对应的操作
    // 无论是query or update 是不是都采用一个mysql_query的接口 ### 疑问
    if (mysql_query(m_mysql, sql.c_str()) != 0)
    {
        unsigned int errorCode = mysql_errno(m_mysql);
        std::string error = mysql_error(m_mysql);
        LOG_ERROR("connection failed to execute " + std::string(isQuery ? "query" : "update") +
                  " [" + m_connectionId + "]: " + error + ", SQL: " + sql);
        throw db::SQLExecutionError("SQL execution failed: " + error, errorCode);
    }
    // 如果发生错误，进行错误处理
    // 执行成功
    // 如果为查询操作，需要返回查询结果

    if (isQuery)
    {
        // 处理查询，才需要处理结果集
        MYSQL_RES *result = mysql_store_result(m_mysql);
        // 判断是否有结果集，为什么这样判断呢？
        if (!result && mysql_field_count(m_mysql) > 0) // ### 这里命名result为空指针，为什么mysql_field_count还能够有结果呢？因为传入的是m_mysql，这里是不是要验证数据表不是空表，表是由多个域组成的
        {
            unsigned int errorCode = mysql_errno(m_mysql);
            std::string error = mysql_error(m_mysql);
            LOG_ERROR("Failed to store query result [" + m_connectionId + "]: " + error);
            throw db::SQLExecutionError("Failed to store query result [" + m_connectionId + "]: " + error, errorCode);
        }

        return std::make_shared<QueryResult>(result);
    }
    else
    {
        // 对于更新操作，返回受影响的行数
        unsigned long long affects = mysql_affected_rows(m_mysql);
        return std::make_shared<QueryResult>(nullptr, affects);
    }
}

QueryResultPtr Connection::executeWithReconnect(const std::string &sql, bool isQuery)
{
    // 加锁，保证多线程安全
    std::unique_lock<std::mutex> lock(m_mutex);
    // std::unique_lock<std::recursive_mutex> lock(m_mutex);
    // 日志记录
    LOG_DEBUG("trying to execute " + std::string(isQuery ? "query" : "update") + "[" + m_connectionId + "]");
    unsigned int errorCode = 0; // 准备好错误码
    std::string errorMsg;
    // 循环执行SQL命令
    for (unsigned int attempt = 0; attempt <= m_reconnectAttempts; ++attempt)
    {
        // 开始正式执行SQL语句之前，先重连
        if (attempt > 0)
        {
            // 日志记录，我的日志记录是很不完整的，需要继续学习
            LOG_WARNING("Retrying " + std::string(isQuery ? "query" : "update") +
                        "execution after reconnection, attempt " + std::to_string(attempt) +
                        " [" + m_connectionId + "]");
            // 开始重连
            // ###BUG 发生死锁
            lock.unlock();
            bool reconnected = reconnect();
            lock.lock();
            if (!reconnected)
            {
                // 如果重连失败，设置错误码和错误信息，然后继续下一次尝试进行重连
                errorCode = CR_SERVER_GONE_ERROR;
                errorMsg = "Failed to reconnect";

                LOG_WARNING("Failed to reconnect for " + std::string(isQuery ? "query" : "update") + " execution [" + m_connectionId + "]: " + errorMsg);
                continue;
            }
        }

        try
        {
            // 调用executeInternal真正执行SQL语句
            // ###BUG 这里发生死锁
            lock.unlock();
            auto result = executeInternal(sql, isQuery);
            lock.lock();
            // 更新最新的活动时间
            updateLastActiveTime();
            return result;
        }
        catch (const db::SQLExecutionError &e)
        {
            // 我已经自定义了SQL查询的异常类，为什么还要使用API呢，要使用我自己定义的接口
            errorCode = e.getErrorCode();
            // 仍然要使用我自己定义的接口
            errorMsg = e.what();

            // 判断是否是连接错误，如果不是的，继续抛出
            if (isConnectionError(errorCode))
            {
                LOG_WARNING("Trying reconnect to execute " + std::string(isQuery ? "query" : "update") +
                            " [" + m_connectionId + "]: " + errorMsg);
                continue;
            }
            else
            {
                throw; // 这里仅仅写throw，是将收到的上一个异常重新抛出吗？ ###疑问
            }
        }
    }

    std::string error = "Fail to execute " + std::string(isQuery ? "query" : "update") +
                        " with " + std::to_string(m_reconnectAttempts + 1) + "attempts [" +
                        m_connectionId + "]: " + errorMsg;
    LOG_ERROR(error);

    throw std::runtime_error(error);
}

// =============================
// 事务管理方法
// 无论是开始事务、提交事务、回滚事务，整体的逻辑是一样的，只是进行事务的不同阶段而已
// 增强版的事务管理方法，需要使用executeWithReconnect，在执行查询操作的过程中，如果需要重连的话就应该先进行重连，然后再执行事务的提交操作
// 这是与之前的事务管理不同的地方
// =============================

/* bool Connection::beginTransaction()
{
    // 加锁，保证多线程安全
    // std::unique_lock<std::mutex> lock(m_mutex);
    // std::unique_lock<std::recursive_mutex> lock(m_mutex);
    // 判断连接是否建立
    if (!m_mysql || !m_connected)
    {
        LOG_ERROR("Connection not established [" + m_connectionId + "]");
        // throw std::runtime_error("Connection not established [" + m_connectionId + "]");
        // 对于返回void 或者 bool，不需要抛出异常，只需要返回true/false即可
        return false;
    }
    // 日志记录开始事务的事件
    LOG_DEBUG("start transaction [" + m_connectionId + "]");
    // 执行命令 ### 注意好像无论执行什么命令，都是mysql_query这一个函数
    // if (mysql_query(m_mysql, "START TRANSACTION") != 0)
    {
        // 错误处理
        std::string error = "Failed to begin transaction [" + m_connectionId + "]: " + getLastError();
        LOG_ERROR(error);
        // throw std::runtime_error(error);
        return false;
    }
    // 更新最近连接活动时间，这应该是执行成功才会更新连接的最新活动时间吗？还是在活动开始之前更新活动时间
    updateLastActiveTime();
    // 返回
    return true;
} */

bool Connection::beginTransaction()
{
    // ###BUG 这里没有考虑很清楚，executeWithReconnect函数体中首先就是加锁，我当前使用的是递归锁，也就是可重入锁，所以没有什么问题
    // 但是需要加锁的临界区就是executeWithReconnect这一个函数，所以从性能角度来看，不需要加锁的
    std::unique_lock<std::mutex> lock(m_mutex);
    // std::unique_lock<std::recursive_mutex> lock(m_mutex);
    LOG_DEBUG("Begin transaction [" + m_connectionId + "]");
    // 在try-catch中执行executeWithReconnect函数
    try
    {
        // 执行成功
        // ### BUG 执行事务不应该使用重连
        // ### BUG 死锁
        lock.unlock();
        auto result = executeInternal("START TRANSACTION", false);
        lock.lock();
        // 对于返回的结果应该转为void，防止编译器警告
        (void)result;
        updateLastActiveTime();

        return true;
    }
    catch (const std::exception &e)
    {
        // 执行失败
        LOG_ERROR("Failed to begin transaction [" + m_connectionId +
                  "]: " + e.what());

        return false;
    }
}

// 待执行的SQL命令入队之后，就需要提交事务，说明需要让MySQL执行所有提交的命令，以及返回是否成功执行事务的标志
/* bool Connection::commit()
{
    // 加锁
    // std::unique_lock<std::mutex> lock(m_mutex);
    // std::unique_lock<std::recursive_mutex> lock(m_mutex);
    // 判断连接是否建立
    if (!m_mysql || !m_connected)
    {
        LOG_ERROR("Connection not established! [" + m_connectionId + "]");
        return false;
    }
    // 日志记录事件
    LOG_DEBUG("commit transaction [" + m_connectionId + "]");
    // 执行命令
    if (mysql_query(m_mysql, "COMMIT") != 0)
    {
        // 错误处理
        std::string error = "Failed to commit transaction [" + m_connectionId + "]: " + getLastError();
        LOG_ERROR(error);
        return false;
    }

    // 更新连接最新活动时间
    updateLastActiveTime();
    // 返回
    return true;
} */

bool Connection::commit()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    LOG_DEBUG("Commit transaction [" + m_connectionId + "]");

    // 执行带reconnect的操作
    try
    {
        // 成功
        lock.unlock();
        auto result = executeInternal("COMMIT", false);
        lock.lock();
        (void)result;

        updateLastActiveTime();
        return true;
    }
    catch (const std::exception &e)
    {
        // 失败
        // 日志记录
        LOG_ERROR("Failed to commit transaction [" + m_connectionId + "]");
        return false;
    }
}

// 如果提交事务失败后，需要进行事务的回滚，是不是在MySQL中事务具有原子性，要么全部执行成功，如果有一个失败，就需要回滚；就是要么执行成功，要么回到最初的模样
/* bool Connection::rollback()
{
    // 加锁
    // std::unique_lock<std::mutex> lock(m_mutex);
    // std::unique_lock<std::recursive_mutex> lock(m_mutex);
    // 判断连接是否建立
    if (!m_mysql || !m_connected)
    {
        LOG_ERROR("Connection not eatablished [" + m_connectionId + "]");
    }
    // 记录事件
    LOG_DEBUG("roll back transaction [" + m_connectionId + "]");
    // 开始执行事务回滚
    if (mysql_query(m_mysql, "ROLLBACK") != 0)
    {
        // 错误处理
        LOG_ERROR("Failed to rollback [" + m_connectionId + "]: " + getLastError());
        return false;
    }

    // 更新连接的最新活动时间
    updateLastActiveTime();
    // 返回
    return true;
} */

bool Connection::rollback()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    LOG_DEBUG("Roll back transaction [" + m_connectionId + "]");

    try
    {
        lock.unlock();
        auto result = executeInternal("ROLLBACK TRANSACTION", false);
        lock.lock();
        (void)result;

        updateLastActiveTime();
        return true;
    }
    catch (const std::exception &e)
    {
        LOG_ERROR("Failed to rollback transaction [" + m_connectionId + "]");

        return false;
    }
}

// =============================
// 错误处理方法
// ### BUG 在getLastError，getLastErrorCode，escapeString函数中，我自己使用了isValid代替直接判断
// 连接是否有效，这导致了死锁的发生，所以我需要修改逻辑
// 我先尝试使用递归锁解决问题，然后再修改逻辑
// ### BUG 对于错误处理，我必须改变逻辑，单纯的将互斥锁修改为递归锁是不够的，因为isValid中调用了mysql_ping，这会清除mysql之前的错误状态，因此
// 我打印出来的错误信息都是空的，因为mysql_ping没有错误，所以就不会有任何字符串显示
// =============================

// 在增强版带reconnect的版本中，不需要getLastError和getLastErrorCode两个函数
/* std::string Connection::getLastError() const
{
    // 进行任何与MySQL Server的通信，都需要先判断是否建立连接，然后再执行操作
    // if(!isValid())
    // ### 疑问：为什么仅仅查看m_mysql就能够判断连接是否建立，不要再查看m_connected吗？
    if (!m_mysql || !m_connected)
    {
        return "MySQL connection not established!";
    }

    // 有连接，直接返回上次mysql操作后记录的错误信息
    return mysql_error(m_mysql);
} */

/* unsigned int Connection::getLastErrorCode() const
{
    // 没有建立连接，直接返回0
    // if(!isValid())
    if (!m_mysql || !m_connected)
        return 0;
    // 有连接，调用API
    return mysql_errno(m_mysql);
} */

// ##BUG const成员函数不要忘记定义的时候也要写const
bool Connection::isConnectionError(unsigned int errorCode) const
{
    // MySQL连接相关错误代码
    switch (errorCode)
    {
    // 服务器已经关闭连接
    case 2006: // CR_SERVER_GONE_ERROR
        LOG_DEBUG("Detected SERVER_GONE_ERROR [" + m_connectionId + "]");
        return true;

    // 服务器连接断开
    case 2013: // CR_SERVER_LOST
        LOG_DEBUG("Detected SERVER_LOST [" + m_connectionId + "]");
        return true;

    // 连接失败
    case 2003: // CR_CONN_HOST_ERROR
        LOG_DEBUG("Detected CONN_HOST_ERROR [" + m_connectionId + "]");
        return true;

    // 无法连接到MySQL服务器
    case 2002: // CR_CONNECTION_ERROR
        LOG_DEBUG("Detected CONNECTION_ERROR [" + m_connectionId + "]");
        return true;

    // 丢失与MySQL服务器的连接（扩展版）
    case 2055: // CR_SERVER_LOST_EXTENDED
        LOG_DEBUG("Detected SERVER_LOST_EXTENDED [" + m_connectionId + "]");
        return true;

    // 读取通信数据包时出错
    case 2027: // CR_MALFORMED_PACKET
        LOG_DEBUG("Detected MALFORMED_PACKET [" + m_connectionId + "]");
        return true;

    default:
        LOG_DEBUG("Error Code " + std::to_string(errorCode) + " is not an connection error [" + m_connectionId + "]");
        return false;
    }
}

// =============================
// 工具方法：包括转义SQL语句；初始化创建时间，更新活动时间
// =============================
std::string Connection::escapeString(const std::string &sql)
{
    // ### 我需要确定的一点是：对于SQL中的字符串我需要进行转义，而且这些字符串需要使用单引号包围起来，整个SQL语句是字符串形式，这两个字符串是不一样的意思的，我需要分清楚

    // 我认为这个函数的功能仅仅是转义SQL语句，而不需要判断连接是否建立，但是写上也无所谓
    // ### 疑问：由于mysql_real_escape_string仍然需要传入连接句柄m_mysql，所以使用API进行转义的前提是需要建立MySQL连接 ### 疑问
    // 判断连接是否建立
    // if(!isValid())
    if (!m_mysql || !m_connected)
    {
        LOG_ERROR("Connection not established, can not escape string [" + m_connectionId + "]");
        // 返回的应该是转义后的SQL语句，因此这里无法返回，只能抛出异常
        throw std::runtime_error("connection not established, cannot escape string!");
    }
    // 提前分配足够大的缓冲区，这里使用的是vector而不是string，需要思考为什么这样做
    // 因为我需要得到容器底层的数组指针，然后进行赋值，但是string.c_str返回的是const char*无法进行赋值操作，但是vector<char>.data()可以得到char*，这样就可以直接赋值
    std::vector<char> escaped(sql.size() + 1); // 按照C风格字符串数组进行分配
    // 调用mysql官方的转义函数
    // ### 必须要进行错误处理，需要进行错误处理吗？如果需要的话，应该如何做错误处理？
    uint64_t escaped_length = mysql_real_escape_string(m_mysql,
                                                       escaped.data(),
                                                       sql.c_str(),
                                                       sql.length()); // 原始的SQL字符串的长度

    // 返回转义后的字符串，需要自己进行构造
    return std::string(escaped.data(), escaped_length);
}

// ### getCreationTime and getLastActiveTime就很容易说明很多的问题
// 同样都是共享资源，哪些需要加锁访问，哪些不需要加锁访问，这是一个很讲究的问题
// 因为creationTime是固定值，不会随着时间而变化，但是lastActiveTime是一个变化值，随着操作的进行而变化
// 因此前者不需要加锁读；但是后者需要加锁读，来得到此时的最新activeTime
int64_t Connection::getCreationTime() const
{
    return m_creationTime;
}

// ### BUG 我需要加锁锁定此时的最新活动时间
int64_t Connection::getLastActiveTime() const
{
    std::unique_lock<std::mutex> lock(m_mutex);
    // std::unique_lock<std::recursive_mutex> lock(m_mutex);
    return m_lastActiveTime;
}

// ### 这是有大坑的地方，死锁的第二种情况，就是自己锁自己；已经上锁了，但是继续加锁，但是自己没有释放，导致死锁，这是解不开的
void Connection::updateLastActiveTime() const
{
    // 往往是在连接后进行某些操作，才会更新最新的活动时间
    // 因此默认调用该函数的时候，是在加锁状态下
    m_lastActiveTime = Utils::currentTimeMillis();
}

// 这个connection对象创建成功后，connectionId就不会再发生改变了，因此不需要加锁
std::string Connection::getConnectionId() const
{
    return m_connectionId;
}

// =============================
// day3：新增重连统计方法
// =============================

unsigned int Connection::getTotalReconnectAttempts() const
{
    // 加锁，返回重连总的次数
    std::unique_lock<std::mutex> lock(m_mutex);
    return m_totalReconnectAttempts;
}

unsigned int Connection::getSuccessfulReconnects() const
{
    std::unique_lock<std::mutex> lock(m_mutex);
    return m_successfulReconnects;
}

void Connection::resetReconnectStatus()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    m_totalReconnectAttempts = 0;
    m_successfulReconnects = 0;
    LOG_DEBUG("Reconnection statistics reset [" + m_connectionId + "]");
}
unsigned int Connection::calculateReconnectDelay(unsigned int attempt) const
{
    // 根据指数退避算法计算标准延迟时间
    unsigned int baseDelay = m_reconnectInterval;   // 延迟时间的根
    // 标准延迟时间不能无限大，因此需要有上限
    // ###BUG 写了static const，不要忘记写unsigned int
    static const unsigned int maxDelay = 30000;  // 30秒是最长延迟时间
    unsigned int standardDelay = baseDelay * (1 << (attempt - 1));  // 0就是2^0; 1就是2^1
    standardDelay = std::min(maxDelay, standardDelay);  // 说明最大就是30秒

    // 定义局部静态变量，而且是thread_local类型
    // 这是更安全的随机抖动计算，避免惊群效应
    static thread_local std::mt19937 rng { std::random_device{}() };
    static thread_local std::uniform_real_distribution<double> dist { 0.8, 1.2 };   // 变化范围为80%-120%

    // 计算抖动的概率[0.8, 1,2] jittered
    double jitteredDelay = standardDelay * dist(rng);
    unsigned int delay = static_cast<unsigned int>(std::max(1.0, jitteredDelay));    // 最小延长时间要有1ms
    // 得到该尝试次数的延迟时间，并日志记录
    LOG_INFO("Calculate reconnect delay " + std::to_string(delay) + "ms for Attempt " + std::to_string(attempt)
            + " [" + m_connectionId + "]");

    return delay;
}
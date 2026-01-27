#include "connection.h"
#include "utils.h"
#include <stdexcept>

/**
 * @brief 这是连接类的基础实现
 */

// =============================
// 构造函数和析构函数
// =============================

Connection::Connection(const std::string &host, const std::string &user,
                       const std::string &password, const std::string &database,
                       unsigned int port)
    : m_mysql(nullptr), m_host(host), m_user(user), m_password(password), m_database(database), m_port(port), m_connectionId(Utils::generateRandomString(16)), m_creationTime(Utils::currentTimeMillis()), m_lastActiveTime(m_creationTime), m_connected(false)
{
    LOG_INFO("Creating connection [" + m_connectionId + "] to " +
             m_user + "@" + m_host + ":" + std::to_string(m_port) + "/" + m_database);
    // 初始化连接对象
    init();
}

Connection::~Connection()
{
    close();
    m_connected = false;
    LOG_INFO("destroy connection object [" + m_connectionId + "]");
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
    if (mysql_options(m_mysql, MYSQL_SET_CHARSET_NAME, "uft8mb4") != 0)
    {
        LOG_WARNING("Failed to set charset to utf8mb4");
    }

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

    if (m_connected)
    {
        LOG_WARNING("connection to MySQL Server has established [" + m_connectionId + "]");
        lock.unlock();
        return true;
    }

    // 判断连接对象是否完成初始化
    if (!m_mysql)
    {
        LOG_ERROR("MySQL connection object not initialized [" + m_connectionId + "]");
        return false;
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
        std::string error = getLastError();
        LOG_ERROR("Failed to connect to MySQL Server: [" + m_connectionId + "]: " + error);
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
 * @brief 因为有可能不仅仅在析构函数中调用close，因此需要加锁互斥访问
 */
void Connection::close()
{
    std::unique_lock<std::mutex> lock(m_mutex);

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
bool Connection::isValid() const
{
    std::unique_lock<std::mutex> lock(m_mutex);

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
        // 任何mysql操作失败后，都应该打印getLastError()
        LOG_ERROR("Connection validation failed [" + m_connectionId + "]: " + getLastError());
        return false;
    }
}

// =============================
// 查询执行方法
// @note 每次连接对象只有一个，但是可能被多个线程使用，因此必须加锁保证线程安全
// =============================
QueryResultPtr Connection::executeQuery(const std::string &sql)
{
    // 调用内部实现的方法，应该是统一进行select and non-select操作
    return executeInternal(sql, true);
}

unsigned long long Connection::executeUpdate(const std::string &sql)
{
    QueryResultPtr result = executeInternal(sql, false);
    return result ? result->getAffectedRows() : 0;
}


QueryResultPtr Connection::executeInternal(const std::string &sql, bool isQuery)
{
    // 之所以不能使用isValid()检验连接是否有效，是因为不能加两次锁，
    // 但是我可以先判断是否有效；然后再加锁进行后续的操作
    // 判断连接是否有效
    if (!isValid())
    {
        // 日志记录规范：发生的事件 + [connectionId] + error
        LOG_ERROR("Connection not established [" + m_connectionId + "]");
        // 因为需要返回具体的结果，但是因为发生了错误，无法返回，只能抛出异常
        throw std::runtime_error("Connection not established [" + m_connectionId + "]");
    }

    std::unique_lock<std::mutex> lock(m_mutex);
    // 若有效
    // 记录日志：尝试进行什么操作
    LOG_DEBUG("Connection execute " + std::string(isQuery ? "query" : "update") +
              " [" + m_connectionId + "], sql: " + sql);

    updateLastActiveTime();

    // 开始对应的操作
    // 无论是query or update 是不是都采用一个mysql_query的接口 ### 疑问
    if (mysql_query(m_mysql, sql.c_str()) != 0)
    {
        std::string error = getLastError();
        LOG_ERROR("connection failed to execute " + std::string(isQuery ? "query" : "update") +
                 " [" + m_connectionId + "]: " + error + ", SQL: " + sql);
        throw std::runtime_error("SQL execution failed: " + error);
    }
    // 如果发生错误，进行错误处理
    // 执行成功
    // 如果为查询操作，需要返回查询结果
    
    if(isQuery)
    {
        // 处理查询，才需要处理结果集
        MYSQL_RES *result = mysql_store_result(m_mysql);
        // 判断是否有结果集，为什么这样判断呢？
        if(!result && mysql_field_count(m_mysql) > 0)    // ### 这里命名result为空指针，为什么mysql_field_count还能够有结果呢？因为传入的是m_mysql，这里是不是要验证数据表不是空表，表是由多个域组成的
        {
            std::string error = getLastError();
            LOG_ERROR("Failed to store query result [" + m_connectionId + "]: " + error);
            throw std::runtime_error("Failed to store query result [" + m_connectionId + "]: " + error);
        }

        return std::make_shared<QueryResult>(result);
    } else {
        // 对于更新操作，返回受影响的行数
        unsigned long long affects = mysql_affected_rows(m_mysql);
        return std::make_shared<QueryResult>(nullptr, affects);
    }
}

// =============================
// 事务管理方法
// 无论是开始事务、提交事务、回滚事务，整体的逻辑是一样的，只是进行事务的不同阶段而已
// =============================

bool Connection::beginTransaction()
{
    // 加锁，保证多线程安全
    std::unique_lock<std::mutex> lock(m_mutex);
    // 判断连接是否建立
    if(!m_mysql || !m_connected)
    {
        LOG_ERROR("Connection not established [" + m_connectionId + "]");
        // throw std::runtime_error("Connection not established [" + m_connectionId + "]");
        // 对于返回void 或者 bool，不需要抛出异常，只需要返回true/false即可
        return false;
    }
    // 日志记录开始事务的事件
    LOG_DEBUG("start transaction [" + m_connectionId + "]");
    // 执行命令 ### 注意好像无论执行什么命令，都是mysql_query这一个函数
    if(mysql_query(m_mysql, "START TRANSACTION") != 0)
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
}

// 待执行的SQL命令入队之后，就需要提交事务，说明需要让MySQL执行所有提交的命令，以及返回是否成功执行事务的标志
bool Connection::commit()
{
    // 加锁
    std::unique_lock<std::mutex> lock(m_mutex);
    // 判断连接是否建立
    if(!m_mysql || !m_connected)
    {
        LOG_ERROR("Connection not established! [" + m_connectionId + "]");
        return false;
    }
    // 日志记录事件
    LOG_DEBUG("commit transaction [" + m_connectionId + "]");
    // 执行命令
    if(mysql_query(m_mysql, "COMMIT") != 0)
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
}

// 如果提交事务失败后，需要进行事务的回滚，是不是在MySQL中事务具有原子性，要么全部执行成功，如果有一个失败，就需要回滚；就是要么执行成功，要么回到最初的模样
bool Connection::rollback()
{
    // 加锁
    std::unique_lock<std::mutex> lock(m_mutex);
    // 判断连接是否建立
    if(!m_mysql || !m_connected)
    {
        LOG_ERROR("Connection not eatablished [" + m_connectionId + "]");
    }
    // 记录事件
    LOG_DEBUG("roll back transaction [" + m_connectionId + "]");
    // 开始执行事务回滚
    if(mysql_query(m_mysql, "ROLLBACK") != 0)
    {
        // 错误处理
        LOG_ERROR("Failed to rollback [" + m_connectionId + "]: " + getLastError());
        return false;
    }
    
    // 更新连接的最新活动时间
    updateLastActiveTime();
    // 返回
    return true;
}

// =============================
// 错误处理方法
// =============================

std::string Connection::getLastError() const
{
    // 进行任何与MySQL Server的通信，都需要先判断是否建立连接，然后再执行操作
    if(!isValid())
    {
        return "MySQL connection not established!";
    }

    // 有连接，直接返回上次mysql操作后记录的错误信息
    return mysql_error(m_mysql);
}

unsigned int Connection::getLastErrorCode() const
{
    // 没有建立连接，直接返回0
    if(!isValid())
        return 0;
    // 有连接，调用API
    return mysql_errno(m_mysql);
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
    if(!isValid())
    {
        LOG_ERROR("Connection not established, can not escape string [" + m_connectionId + "]");
        // 返回的应该是转义后的SQL语句，因此这里无法返回，只能抛出异常
        throw std::runtime_error("connection not established, cannot escape string!");
    }
    // 提前分配足够大的缓冲区，这里使用的是vector而不是string，需要思考为什么这样做
    // 因为我需要得到容器底层的数组指针，然后进行赋值，但是string.c_str返回的是const char*无法进行赋值操作，但是vector<char>.data()可以得到char*，这样就可以直接赋值
    std::vector<char> escaped(sql.size() + 1);  // 按照C风格字符串数组进行分配
    // 调用mysql官方的转义函数
    // ### 必须要进行错误处理，需要进行错误处理吗？如果需要的话，应该如何做错误处理？
    uint64_t escaped_length = mysql_real_escape_string(m_mysql,
                                                       escaped.data(),
                                                       sql.c_str(),
                                                       sql.length());   // 原始的SQL字符串的长度
    
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

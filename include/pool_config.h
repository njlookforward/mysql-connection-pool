/**
 * @brief 实现连接池的配置信息
 */
#ifndef POOL_CONFIG_H
#define POOL_CONFIG_H

#include <string>
#include <vector>
#include <algorithm>
#include <stdexcept>
#include <cassert>
#include "db_config.h"

/**
 * @brief 连接池配置信息
 * 
 * 这个结构体定义了连接池所需的全部配置参数
 * 包括数据库连接信息、连接池大小、超时设置等
 */
struct PoolConfig
{
    // =============================
    // 数据库连接的基本信息
    // =============================
    std::string host;       // 默认主机（单数据库模式）
    std::string user;       // 默认用户
    std::string password;   // 默认密码
    std::string database;   // 默认数据库
    unsigned int port;      // 默认端口号

    // =============================
    // 多数据库实例配置（用于负载均衡）
    // =============================
    DBConfigList dbInstances;   // 多个数据库实例配置

    // =============================
    // 连接池大小
    // =============================
    unsigned int minConnections;    // 最少的连接数量（池中始终保持的连接数）
    unsigned int maxConnections;    // 最大的连接数量（池中最多允许的连接数）
    unsigned int initConnections;   // 初始连接数（启动时创建的连接数）

    // =============================
    // 超时设置（毫秒）
    // =============================
    unsigned int connectionTimeout; // 等待获取连接的超时时间
    unsigned int maxIdleTime;       // 连接最大的空闲时间（超过则断开连接）
    unsigned int healthCheckPeriod; // 健康检测的周期

    // =============================
    // 重连设置
    // =============================
    unsigned int reconnectInterval; // 重连的时间间隔（毫秒）
    unsigned int reconnectAttemps;  // 最大重连尝试次数

    // =============================
    // 其他设置
    // =============================
    bool logQueries;            // 日志是否记录所有的SQL查询
    bool enablePerformanceStat; // 是否启用性能统计

    /**
     * @brief 默认构造函数
     * 设置合理的默认值，适用于绝大多数场景
     */
    PoolConfig()
        : port(3306)                    // MYSQL标准端口
        , minConnections(5)             // 最少保持5个连接
        , maxConnections(20)            // 最多允许20个连接
        , initConnections(5)            // 启动时创建5个连接
        , connectionTimeout(5000)       // 5秒获取连接超时
        , maxIdleTime(600000)           // 连接最多10分钟空闲时间
        , healthCheckPeriod(30000)      // 每30秒一次健康检测
        , reconnectInterval(1000)       // 1秒的重连时间间隔
        , reconnectAttemps(3)           // 最多重试3次
        , logQueries(false)             // 默认不记录SQL查询
        , enablePerformanceStat(true)   // 默认启动性能统计
    {}

    /**
     * @brief 便捷构造函数（单数据库模式）
     * 我只需要传输待连接的数据库信息
     */
    PoolConfig(const std::string &host, const std::string &user,
               const std::string &password, const std::string &database,
               unsigned int port = 3306)
               : PoolConfig()       // 先委托默认构造函数进行初始化，然后再在函数体中进行赋值
    {
        this->host = host;
        this->user = user;
        this->password = password;
        this->database = database;
        this->port = port;
    }

    /**
     * @brief 验证配置参数是否有效
     * 从上到下分部分检测配置参数是否有效
     */
    bool isValid() const 
    {
        // 1. 检测数据库参数信息，分多数据库模式和单数据库模式进行讨论
        if(!dbInstances.empty())
        {
            for(const auto &instance : dbInstances)
            {
                if(!instance.isValid())
                    return false;
            }
        } else {
            if(host.empty() || user.empty() || database.empty() || port <= 0)
                return false;
        }

        // 2. 检查连接池大小
        // @note 注意initConnections不需要过多讨论，只需要满足不能超过maxConnections即可
        if(minConnections == 0 || maxConnections == 0 || 
           minConnections > maxConnections || initConnections > maxConnections)
            return false;

        // 3. 检查超时设置
        if(connectionTimeout == 0 || maxIdleTime == 0 || healthCheckPeriod == 0)
            return false;   
        
        // 4. 检查重连设置，不需要检查重连信息
        // if(reconnectInterval == 0 || reconnectAttemps == 0)
            // return false;

        return true;
    }

    /**
     * @brief 添加数据库实例（多数据库模式）
     */
    void addDatabase(const DBConfig &config)
    {
        if(config.isValid())
            dbInstances.push_back(config);
    }

    /**
     * @brief 获取数据库实例数量
     */
    size_t getDatabaseCount() const
    {
        return dbInstances.empty() ? 1 : dbInstances.size();
    }

    /**
     * @brief 得到连接池配置的摘要信息 
     * @return 配置摘要字符串
     * */    
    std::string getSummary() const
    {
        std::string summary = "PoolConfig:{";
        // 连接池大小
        summary += "connections:[" + std::to_string(minConnections) + ", "
                + std::to_string(maxConnections) + "]";
        // 超时设置
        summary += ", timeout:" + std::to_string(connectionTimeout) + "ms";
        // 已经建立连接的数据库实例数量
        summary += ", databases:" + std::to_string(getDatabaseCount());
        summary += "}";

        return summary;
    }

    /**
     * @brief 设置连接池大小 min max init
     */
    void setConnectionLimits(unsigned int min, unsigned int max, unsigned init)
    {
        // 我必须判断max与min是否满足要求，才能进行设置
        // 其实从数据类型的角度，输入参数肯定是>=0的，但是仍然需要检查不为0
        assert(max > 0 && "Please input maxConnections which is bigger than 0.");
        assert(min > 0 && "Please input minConnections which is bigger than 0.");
        maxConnections = max;
        minConnections = min;
        // 这也是一种有效性判断
        initConnections = (init == 0) ? min : std::min(init, max);
    }

    /**
     * @brief 设置超时参数，以毫秒为单位
     * @param 连接超时
     * @param 空闲超时
     * @param 健康检查周期
     */
    void setTimeouts(unsigned int connTimeout, unsigned int idleTimeout, unsigned int checkPeriod)
    {
        assert(connTimeout > 0 && "connTimeout is must bigger than 0");
        assert(idleTimeout > 0 && "idletimeout is must bigger than 0");
        assert(checkPeriod > 0 && "checkperiod is must bigger than 0");
        connectionTimeout = connTimeout;
        maxIdleTime = idleTimeout;
        healthCheckPeriod = checkPeriod;
    }
};

#endif  // POOL_CONFIG_H
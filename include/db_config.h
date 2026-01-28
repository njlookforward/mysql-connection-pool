/**
 * @brief 实现单个数据库配置
 */
#ifndef DB_CONFIG_H
#define DB_CONFIG_H

#include <string>
#include <vector>

/**
 * @brief 单个数据库实例的配置信息
 *
 * 这个结构体包含了连接单个MySQL数据库所需的全部配置信息
 * 采用结构体而不是类的原因：
 * 1. 数据简单，不需要复杂的方法
 * 2. 方便初始化和赋值
 * 3. 可以直接用于函数参数传递
 */
struct DBConfig
{
    std::string host;     // 数据库的主机IP地址，如Localhost，192.168.1.100
    std::string user;     // 用户名
    std::string password; // 密码
    std::string database; // 使用哪一个数据库
    unsigned int port;    // mysql的端口号3306
    unsigned int weight;  // 权重，用于负载均衡，权重越大，这个数据库被选中的概率就越大

    /**
     * @brief 默认构造函数
     * 设置MySQL的默认标准值
     */
    DBConfig() : port(3306), weight(1) {}

    /**
     * @brief 便捷构造函数
     * @param 除了port and weight之外，其他的值必须给出
     * 
     * @bug ### 我没有在初始化列表中进行显示初始化，进行使用参数构造，所有的数据成员仍然都是默认值
     */
    DBConfig(const std::string &host, const std::string &user,
             const std::string &password, const std::string &database,
             unsigned int port = 3306, unsigned int weight = 1)
             : host(host)
             , user(user)
             , password(password)
             , database(database)
             , port(port)
             , weight(weight) {}

    /**
     * @brief 验证数据库配置是否有效
     * @return 数据库配置是否完整有效
     * 需要有host、user、database，并且port必须大于0；因为密码可能设置为空，所以password排除在外
     */
    bool isValid() const
    {
        return !host.empty() && !user.empty() && !database.empty() && port > 0;
    }

    /**
     * @brief 得到mysql连接字符串，用于日志记录，所以不能包含密码
     */
    std::string getConnectionStr() const
    {
        return user + "@" + host + ":" + std::to_string(port) + "/" + database;
    }

    /**
     * @brief 比较运算符 用于容器内排序和查找
     * @param other
     * @retval bool
     * 因为只要是同一个host、同一个port、同一个user，那么必然就是同一个用户的mysql，password肯定相同
     * 最后就是差database判断是否相同，来说明是不是同一个数据库连接
     * 至于权重，是没有必要进行判断的
     * 而且判断的顺序应该从大范围到小范围
     */
    bool operator==(const DBConfig &rhs) const
    {
        return host == rhs.host &&
               port == rhs.port &&
               user == rhs.user &&
               database == rhs.database;
    }

    bool operator!=(const DBConfig &rhs)
    {
        return !(*this == rhs);
    }
};

/**
 * @brief 数据库配置的列表的数据类型
 * 使用类型别名来提高可读性
 */
using DBConfigList = std::vector<DBConfig>;

#endif // DB_CONFIG_H
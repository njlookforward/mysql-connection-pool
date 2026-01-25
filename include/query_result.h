#ifndef QUERY_RESULT_H
#define QUERY_RESULT_H

#include <string>
#include <vector>
#include <mysql/mysql.h>

/**
 * @brief MySQL查询结果封装类
 * 
 * 这个类封装了MySQL的查询结果，提供了安全、便捷的数据访问接口
 * 
 * 设计原则：
 * 1）RAII：构造时申请资源，析构时释放资源
 * 2) 类型安全：提供强类型的数据访问方法
 * 3) 异常安全：所有可能抛出异常的地方都有适当的错误处理
 * 4) 易用性：提供直观地API，支持索引和字段名两种访问方式
 */
class QueryResult
{
public:
    /**
     * @brief 构造函数
     * @param result MySQL结果集指针，若为nullptr表示无结果集的操作
     * @param affectedRows 受影响的行数（用于INSERT/UPDATE/DELETE的操作）
     * 
     * 注意构造函数会接管MYSQL_RES的所有权，在析构时会自动释放
     * 
     * ### 疑问：不应该只有含有一个参数的构造函数，如果不希望有隐式转换才需要声明为explicit吗？但是这里是两个参数啊？？
     */
    explicit QueryResult(MYSQL_RES *result, unsigned long long affectedRows = 0);

    /**
     * @brief 析构函数，自动释放MySQL结果集
     * 这是RAII原则的体现，资源获取即初始化，对象销毁即资源释放
     */
    ~QueryResult();

    /**
     * 避免重复释放资源
     * @brief 禁用拷贝构造和拷贝赋值运算符，因为两个对象维护同一个结果集指针，第二个对象销毁时会出现double—free的问题
     */
    QueryResult(const QueryResult &) = delete;
    QueryResult &operator=(const QueryResult &) = delete;

    // 允许移动构造和移动赋值运算（C++11的新特性，进行资源转移，而不是资源的复制）
    QueryResult(QueryResult &&) noexcept;       // ###疑问：为什么要填加noexcept关键字呢？
    QueryResult &operator=(QueryResult &&) noexcept;

    // =============================
    // 结果集导航方法
    // =============================

    /**
     * @brief 移动至下一行
     * @return 是否成功移动到下一行（false表示移动到末尾）
     * 
     * 使用示例：
     * while(result.next())
     * { 
     *      std::string name = result.getString("name");
     *      // 处理每一行的结果
     * }
     */
    bool next();

    /**
     * @brief 重置到第一行（如果支持）
     * @return 是否成功重置到第一行
     * 
     * 注意：有些MySQL配置不允许进行重置
     */
    bool reset();

    // =============================
    // 元数据获取方法
    // =============================
    
    /**
     * @brief 得到结果集中字段数量
     */
    unsigned int getFieldCount() const;

    /**
     * @brief 获取行数
     * @note 只有调用mysql_store_result()才能得到准确的结果集行数
     */
    unsigned long long getRowCount() const;

    /**
     * @brief 得到mysql操作后受影响的行数
     */
    unsigned long long getAffectedRows() const;

    /**
     * @brief 获取所有的字段名
     * @return 返回字段名向量
     */
    std::vector<std::string> getFieldNames() const;



private:
    MYSQL_RES *m_result;                    // mysql查询结果集指针
    MYSQL_ROW m_currentRow;                 // 当前行数据
    unsigned long *m_lengths;               // 当前行各字段的长度 --- 这个没有理解是什么意思
    unsigned int m_fieldCount;              // 字段数量
    unsigned long long m_rows;              // 行数，行数与受影响的行数，都需要使用unsigned long long，因为可能很长
    unsigned long long m_affectedRows;      // 受影响的行数
    std::vector<std::string> m_fieldNames;  // 字段名列表
};



#endif  // QUERY_RESULT_H
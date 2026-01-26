#ifndef QUERY_RESULT_H
#define QUERY_RESULT_H

#include <string>
#include <vector>
#include <memory>
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
     * ### 我的解释：因为第二个参数给出了默认参数，因此相当于单参数构造函数
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

    // =============================
    // 数据访问方法（按字段索引）
    // =============================

    /**
     * @brief 获取指定索引的字段值（字符串）
     * @param index，字段索引（从0开始）
     * @retval 字段值（NULL值返回空字符串）
     */
    std::string getString(unsigned int index) const;

    /**
     * @brief 获取指定索引的字段值（整数）
     * @param 字段索引（从0开始）
     * @return 字段值（整数，如果为NULL返回0）
     * @throws std::invalid_argument 如果无法转换为整数
     */
    int getInt(unsigned int index) const;

    /**
     * @brief 获取指定索引的字段值（长整数）, long long 应该为8 Byte，###待确认
     * @param index，字段索引（从0开始）
     * @return 字段值，NULL值返回0
     * @throws std::invalid_argument，无法转换为长整数
     */
    long long getLong(unsigned int index) const;

    /**
     * @brief 获取指定索引的字段值（浮点数）
     * @param index，字段索引，从0开始
     * @return double，NULL值返回0.0
     * @throws std::invalid_argument 如果无法转换成浮点数
     */
    double getDouble(unsigned int index) const;

    /**
     * @brief 检查指定索引是否为NULL
     * @param index，字段索引
     */
    bool isNull(unsigned int index) const;

    // =============================
    // 数据访问方法（按字段名）
    // =============================

    /**
     * @brief 获取指定字段名对应的字段值（字符串）
     * @param fieldName 字段名
     * @return 字段值
     * @throws std::out_of_range 如果字段名不存在
     */
    std::string getString(const std::string &fieldName) const;

    /**
     * @brief 按照字段名获取字段值（整数）
     * @param fieldName
     * @return 字段值
     * @throws std::out_of_range 字段名不存在
     */
    int getInt(const std::string &fieldName) const;

    /**
     * @brief 按照字段名得到字段值（长整数）
     * @param fieldName
     * @return 字段值
     * @throws std::out_of_range 字段名不存在
     */
    long long getLong(unsigned int index) const;

    /**
     * @brief 按照字段名得到字段值（double）
     * @param fieldName
     * @return 字段值
     * @throws std::out_of_range 字段名不存在
     */
    double getDouble(const std::string &fieldName) const;

    /**
     * @brief 检查指定字段名的字段值是否为NULL
     * @param fieldName
     * @return true/false
     */
    bool isNull(const std::string &fieldName) const;

    // =============================
    // 便利方法
    // =============================

    /**
     * @brief 结果集是否为空（我理解为存在结果集，进行了正确的操作，不过返回的结果为空）
     * @return true/false
     */
    bool isEmpty() const;

    /**
     * @brief 是否有结果集（我理解是操作存在问题，没有结果集，返回的是错误的结果）
     * @return true/false(区别于空结果集，我需要仔细区别这两个问题)
     */
    bool hasResultSet() const;

private:
    /**
     * @brief 初始化元数据信息
     * 从MySQL结果集中提取出元数据
     */
    void initializeMetaData();

    /**
     * @brief 根据字段名得到字段索引
     * @param fieldName
     * @return index
     * @throws std::out_of_range 如果字段名不存在
     */
    unsigned int getFieleIndex(const std::string &fieldName) const;

    /**
     * @brief 检查索引是否有效，是否超出范围
     * @retval void ### 为什么返回值不是true/false，是因为只要索引超出有效范围，直接抛出异常吗
     * @throws std::out_of_range 索引超出有效范围 
     */
    void checkIndex(unsigned int index) const;
    
    /**
     * @brief 检查当前行是否有效
     * @return void
     * @throws std::runtime_error 如果当前行无效，直接抛出运行时异常
     * 检查的就是当前的 MYSQL_ROW m_currentRow;
     */
    void checkRow() const;

    /**
     * @brief 将返回的字符串转换为正确的数据类型
     * @note ### MYSQL与Redis一样，返回的都是字符串值吗
     */
#if 0
    int safeConvert(const char *value, int defaultValue) const;
    long long safeConvert(const char *value, long long defaultValue) const;
    double safeConvert(const char *value, double defaultValue) const;
#endif
    /**
     * @brief 以上的三个函数重载可以使用一个函数模板全部覆盖
     */
#if 1
    template <typename T>
    T safeConvert(const char *value, T defaultVale) const;
#endif


private:
    MYSQL_RES *m_result;                    // mysql查询结果集指针
    MYSQL_ROW m_currentRow;                 // 当前行数据
    unsigned long *m_lengths;               // 当前行各字段的长度 --- 这个没有理解是什么意思，我猜测unsigned long lengths[]，这是当前行各字段长度的数组
    unsigned int m_fieldCount;              // 字段数量
    unsigned long long m_rows;              // 行数，行数与受影响的行数，都需要使用unsigned long long，因为可能很长
    unsigned long long m_affectedRows;      // 受影响的行数
    std::vector<std::string> m_fieldNames;  // 字段名列表
};

// 类型别名，智能指针类型定义，推荐这样
using QueryResultPtr = std::shared_ptr<QueryResult>;

#endif  // QUERY_RESULT_H
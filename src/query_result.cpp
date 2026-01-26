#include "query_result.h"
#include <algorithm>

/**
 * @brief 查询结果的实现文件
 */

// =============================
// 构造和析构函数
// =============================

/**
 * @brief 构造函数 MYSQL_RES 要么是select查询操作的结果，得到的是结果集MYSQL_RES
 * 要么是non-select结果集，UPDATA/DELETE/INSERT，传入的是受到影响的行的数量
 */
QueryResult::QueryResult(MYSQL_RES *result, unsigned long long affectedRows)
    : m_result(result), m_currentRow(nullptr) // 是char * []退化为二级指针，字符串的数组
      ,
      m_lengths(nullptr), m_fieldCount(0), m_rowCount(0), m_affectedRows(affectedRows)
{
    if (m_result)
    {
        // 说明这是查询结果SELECT，就可以初始化元数据信息
        initializeMetaData();
        // 记录日志
        LOG_DEBUG("QueryResult created with " + std::to_string(m_rowCount) +
                  " rows, " + std::to_string(m_fieldCount) + " fields.");
    }
    else
    {
        // 记录日志
        LOG_DEBUG("QueryResult created for non-select operation with " +
                  std::to_string(m_affectedRows) + " affectedRows.");
    }
}

// RAII要求，对象销毁时释放所有的资源
// @note ### 对于m_currentRow and m_lengths这两个指针对应的资源，需要进行回收吗？还是mysql_free_result过程中已经将资源进行回收了
QueryResult::~QueryResult()
{
    if (m_result)
    {
        // 使用MYSQL官方函数，销毁结果集
        mysql_free_result(m_result);
        // @note ### 对于指针必须安全回收，所以释放后应该设置为Nullptr
        m_result = nullptr;
        // 记录日志
        LOG_DEBUG("QueryResult destoryed, free resultSet.");
    }
    else
    {
        LOG_DEBUG("QueryResult destoryed.");
    }
}

// 移动构造函数，资源所有权的转移，所以不能互相干扰
// 我认为noexcept与const一样，在定义的时候也需要指定出来
QueryResult::QueryResult(QueryResult &&other) noexcept
    : m_result(other.m_result), m_currentRow(other.m_currentRow), m_lengths(other.m_lengths), m_fieldCount(other.m_fieldCount), m_rowCount(other.m_rowCount), m_affectedRows(other.m_affectedRows), m_fieldNames(std::move(other.m_fieldNames))
{
    // 清空源对象，避免重复释放
    other.m_result = nullptr;
    // 除了m_result，其他的也要全部回收，更安全
    other.m_currentRow = nullptr;
    other.m_lengths = nullptr;
    other.m_fieldCount = 0;
    other.m_rowCount = 0;
    other.m_affectedRows = 0;
}

// 移动赋值运算符，需要先释放自己的资源，然后再进行资源转移
// 永远不要忘记，在类外定义成员函数的时候，要指定类名作用域
QueryResult &QueryResult::operator=(QueryResult &&other) noexcept
{
    // 自赋值检查
    if (this != &other)
    {
        if (m_result)
        {
            // 收回自己的资源
            mysql_free_result(m_result);
            m_result = nullptr;
        }
        // 资源转移
        m_result = other.m_result;
        m_currentRow = other.m_currentRow;
        m_lengths = other.m_lengths;
        m_fieldCount = other.m_fieldCount;
        m_rowCount = other.m_rowCount;
        m_affectedRows = other.m_affectedRows;
        m_fieldNames = std::move(other.m_fieldNames);

        // 清空源对象
        other.m_result = nullptr;
        other.m_currentRow = nullptr;
        other.m_lengths = nullptr;
        other.m_fieldCount = 0;
        other.m_rowCount = 0;
        other.m_affectedRows = 0;
    }

    return *this;
}

// =============================
// 初始化方法
// =============================
void QueryResult::initializeMetaData()
{
    // 调用MySQL官方提供的方法
    // m_result是否有效
    if (!m_result)
        return;

    // 获取基本信息
    // fieldCount
    m_fieldCount = mysql_num_fields(m_result);
    // rowCount
    m_rowCount = mysql_num_rowCount(m_result);

    // 字段名称列表
    MYSQL_FIELD *fields = mysql_fetch_field(m_result); // 这得到的也是MYSQL_FIELD的数组
    // 提前开辟空间，提高效率
    m_fieldNames.clear();
    m_fieldNames.reserve(m_fieldCount);

    for (size_t i = 0; i < m_fieldCount; ++i)
    {
        m_fieldNames.push_back(fields[i].name);
    }

    // 记录日志
    LOG_DEBUG("QueryResult initialized: " + std::to_string(m_fieldCount) +
              " fields, " + std::to_string(m_rowCount) + " rows");
}

// =============================
// 结果集导航方法，m_currentRow与m_lengths是一体的，都是用来描述当前行的信息
// =============================
bool QueryResult::next()
{
    if (!m_result)
        return false;

    m_currentRow = mysql_fetch_row(m_result);
    if (m_currentRow)
    {
        // 这里得到的就是每个字段长度的集合，每次操作应该都一样的
        m_lengths = mysql_fetch_lengths(m_result);
        return true;
    }
    return false;
}

bool QueryResult::reset()
{
    if (!m_result)
        return false;

    // 重置到结果集的开始位置，同时不要忘记更新m_currentRow and m_lengths
    mysql_data_seek(m_result, 0);
    m_currentRow = nullptr;
    m_lengths = nullptr;
    return true;
}

// =============================
// 元数据获取方法
// =============================
unsigned int QueryResult::getFieldCount() const
{
    return m_fieldCount;
}

unsigned long long QueryResult::getRowCount() const
{
    return m_rowCount;
}

unsigned long long QueryResult::getAffectedRows() const
{
    return m_affectedRows;
}

std::vector<std::string> QueryResult::getFieldNames() const
{
    return m_fieldNames;
}

// 判断结果集是否为空
bool QueryResult::isEmpty() const
{
    return m_result && m_rowCount == 0;
}

// 判断是否有结果集
bool QueryResult::hasResultSet() const
{
    return m_result != nullptr;
}

// =============================
// 数据访问方法（按索引）
// =============================
std::string QueryResult::getString(unsigned int index) const
{
    // m_result有效，且m_currentRow有效
    if (m_result && m_currentRow)
    {
        try
        {
            // 检查索引是否有效
            checkIndex(index);
            // 检查currentRow是否有效
            checkRow();
            
            if(m_currentRow[index] == nullptr)
                return "";
            else 
                return std::string(m_currentRow[index], m_lengths[index]);  // 说明m_lengths也是随着每一行的不同而动态变化
        }
        catch (const std::exception &e)
        {
            std::cerr << e.what() << std::endl;
        }
    }
    else
    {
        throw std::runtime_error(
            m_result == nullptr ? "this is non-select operation, cannot getString"
                                : "Please invoke next() before getString()");
    }
}

// 需要搞清楚checkRow的具体逻辑，才能知道需不需要判断m_currentRow的值
int QueryResult::getInt(unsigned int index) const
{
    try
    {
        // 验证索引值
        checkIndex(index);
        // 验证当前行
        checkRow();

        // 得到转换值
        // 我需要根据safeConvert的逻辑知道需不需要单独判断nullptr的情况
        if(m_currentRow[index] == nullptr)
            return 0;
        return safeConvert(m_currentRow[index], 0);
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
    }
}

long long QueryResult::getLong(unsigned int index) const
{
    try
    {
        checkIndex(index);
        checkRow();

        if(m_currentRow[index] == nullptr)
            return 0LL; // null值返回0
        return safeConvert(m_currentRow[index], 0LL);
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
    }
}

double QueryResult::getDouble(unsigned int index) const
{
    try
    {
        checkIndex(index);
        checkRow();

        if(m_currentRow[index] == nullptr)
            return 0.0;
        return safeConvert(m_currentRow[index], 0.0);
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
    }
}

bool QueryResult::isNull(unsigned int index) const
{
    try
    {
        checkIndex(index);
        checkRow();

        return m_currentRow[index] == nullptr;
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
    }
}

// =============================
// 数据访问方法（按照字段名）
// =============================
std::string QueryResult::getString(const std::string &fieldName) const
{
    // 判断字段名是否合法
    if(std::find(m_fieldNames.cbegin(), m_fieldNames.cend(), fieldName) == m_fieldNames.cend())
    {
        throw std::invalid_argument(fieldName + " is an invalid fieldName, please check and input again!");
    }
    // 字段名映射到字段索引
    unsigned int index = getFieldIndex(fieldName);
    // 直接调用返回
    return getString(index);
}

int QueryResult::getInt(const std::string &fieldName) const
{
    if(std::find(m_fieldNames.cbegin(), m_fieldNames.cend(), fieldName) == m_fieldNames.cend())
        throw std::invalid_argument(fieldName + " is an invalid fieldName, please check and input again!");
    
    return getInt(getFieldIndex(fieldName));
}

long long QueryResult::getLong(const std::string &fieldName) const
{
    if(std::find(m_fieldNames.cbegin(), m_fieldNames.cend(), fieldName) == m_fieldNames.cend())
        throw std::invalid_argument(fieldName + " is an invalid fieldName, please check and input again!");
    
    return getLong(getFieldIndex(fieldName));
}

double QueryResult::getDouble(const std::string &fieldName) const
{
    if(std::find(m_fieldNames.cbegin(), m_fieldNames.cend(), fieldName) == m_fieldNames.cend())
        throw std::invalid_argument(fieldName + " is an invalid fieldName, please check and input again!");
    
    return getDouble(getFieldIndex(fieldName));   
}

bool QueryResult::isNull(const std::string &fieldName) const
{
    if(std::find(m_fieldNames.cbegin(), m_fieldNames.cend(), fieldName) == m_fieldNames.cend())
        throw std::invalid_argument(fieldName + " is an invalid fieldName, please check and input again!");
    
    return isNull(getFieldIndex(fieldName));   
}

// =============================
// 私有辅助方法
// =============================
unsigned int QueryResult::getFieldIndex(const std::string &fieldName) const
{
    // 最简单的从头到尾遍历，找到其位置
    for(unsigned int i = 0; i < m_fieldNames.size(); ++i)
    {
        if(m_fieldNames[i] == fieldName)
            return i;
    }

    throw std::out_of_range("Field name not found: " + fieldName);
}

void QueryResult::checkIndex(unsigned int index) const
{
    // 这里面已经包含了判断m_result是否有效，只有有效，才会初始化得到m_fieldCount
    if(index >= m_fieldCount)
        throw std::out_of_range("Field index out of range: " + std::to_string(index) +
                                ", max = " + std::to_string(m_fieldCount - 1));
}

void QueryResult::checkRow() const
{
    // 判断是否为空
    if(!m_currentRow)
        throw std::runtime_error("No currentRow available, call next() first.");
}

int QueryResult::safeConvert(const char *value, int defaultValue) const
{
    if(!value)
        return defaultValue;
    try
    {
        return std::stoi(value);
    }
    catch(const std::exception& e)
    {
        LOG_WARNING("Failed to convert '" + std::string(value) + "' to int: " + e.what());
        // 只能返回默认值
        return defaultValue;
    } 
}

long long QueryResult::safeConvert(const char *value, long long defaultValue) const
{
    if(!value)
        return defaultValue;
    try
    {
        return std::stoll(value);
    }
    catch(const std::exception& e)
    {
        LOG_WARNING("Failed to convert '" + std::string(value) + "' to long long: " + e.what());
        // 只能返回默认值
        return defaultValue;
    } 
}

double QueryResult::safeConvert(const char *value, double defaultValue) const
{
    if(!value)
        return defaultValue;
    try
    {
        return std::stod(value);
    }
    catch(const std::exception& e)
    {
        LOG_WARNING("Failed to convert '" + std::string(value) + "' to double: " + e.what());
        // 只能返回默认值
        return defaultValue;
    } 
}

// 对于不同类型的函数模板，可以使用模板特化进行定义
#if 1
// =============================
// 模板特化：安全类型转换
// =============================

template <>
int QueryResult::safeConvert<int>(const char *value, int defaultValue) const
{
    if(!value)
        return defaultValue;
    try
    {
        return std::stoi(value);
    }
    catch(const std::exception& e)
    {
        LOG_WARNING("Failed to convert '" + std::string(value) + "' to int: " + e.what());
        // 只能返回默认值
        return defaultValue;
    } 
}

template <>
long long QueryResult::safeConvert<long long>(const char *value, long long defaultValue) const
{
    if(!value)
        return defaultValue;
    try
    {
        return std::stoll(value);
    }
    catch(const std::exception& e)
    {
        LOG_WARNING("Failed to convert '" + std::string(value) + "' to long long: " + e.what());
        // 只能返回默认值
        return defaultValue;
    } 
}

template <>
double QueryResult::safeConvert<double>(const char *value, double defaultValue) const
{
    if(!value)
        return defaultValue;
    try
    {
        return std::stod(value);
    }
    catch(const std::exception& e)
    {
        LOG_WARNING("Failed to convert '" + std::string(value) + "' to double: " + e.what());
        // 只能返回默认值
        return defaultValue;
    } 
}

#endif
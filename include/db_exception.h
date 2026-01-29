#ifndef DB_EXCEPTION_H
#define DB_EXCEPTION_H

#include <string>
#include <stdexcept>

// 这里需要使用命名空间，说明这些类的定义非常常见，需要进行命名隔离
namespace db
{
    // 数据库操作基础异常类
    class DataBaseError : public std::runtime_error 
    {
    public:
        // ###BUG 为了避免隐式类型转换，对于单参数的构造函数，应该声明为explicit
        explicit DataBaseError(const std::string &msg)
        : std::runtime_error(msg) 
        {
            // DataBaseError类是runtime_error的派生类，所以runtime_error是基类子对象，应该在构造函数初始化列表中进行初始化
        }

    };

    // 执行SQL操作（query+update）产生的异常
    // SQL执行异常类（携带错误码）
    class SQLExecutionError : public DataBaseError
    {
    public:
        SQLExecutionError(const std::string &msg, unsigned int errorCode)
        : DataBaseError(msg)
        , m_errorCode(errorCode) {}

        unsigned int getErrorCode() const
        {
            return m_errorCode;
        }

    private:
        // 仅仅多了一个错误码的数据成员
        unsigned int m_errorCode;
    };

}   // namespace db

#endif  // DB_EXCEPTION_H
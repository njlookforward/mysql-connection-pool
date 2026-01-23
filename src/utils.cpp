// 这里创建的是简单的源文件实现
#include "utils.h"
#include <map>

// 命名空间放在包含头文件的外面
namespace Utils {
    // 因为utils.h中的所有函数都是inline实现
    // 在源文件中主要用于未来可能的非inline函数的扩展
    // 在这里添加的都是不适用于inline的复杂工具函数
    // 比如一些复杂的配置文件解析函数、复杂的字符串处理函数等

    // std::map<std::string, std::string> parseConfigureFile(const std::string &configFile)
    // {
    //     // TODO 复杂的配置文件解析逻辑
    //     return std::map<std::string, std::string>{};
    // }

}   // namespace Utils
/**
 * @brief 对比测试：不使用 app 模式，看会发生什么覆盖行为
 */

#include <iostream>
#include <fstream>
#include <thread>
#include <string>
#include <chrono>

// 不使用 app 模式！
void writer1(const std::string& filename) {
    // 注意：这里不使用 ios::app
    std::ofstream file(filename, std::ios::out);

    for (int i = 0; i < 10; i++) {
        file << "AAAAAA_" << i << "\n";
        file.flush();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void writer2(const std::string& filename) {
    // 注意：这里不使用 ios::app
    std::ofstream file(filename, std::ios::out);

    for (int i = 0; i < 10; i++) {
        file << "BBBBBB_" << i << "\n";
        file.flush();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

int main() {
    const std::string filename = "without_app_test.log";

    // 先创建一个有内容的文件
    {
        std::ofstream f(filename);
        for (int i = 0; i < 5; i++) {
            f << "ORIGINAL_" << i << "\n";
        }
    }

    std::cout << "不使用 app 模式的测试...\n";

    std::thread t1(writer1, filename);
    std::thread t2(writer2, filename);

    t1.join();
    t2.join();

    std::cout << "\n结果文件内容：\n";
    std::cout << "========================================\n";

    std::ifstream result(filename);
    std::string line;
    while (std::getline(result, line)) {
        std::cout << line << "\n";
    }

    return 0;
}

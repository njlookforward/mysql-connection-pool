/**
 * @brief 演示：两个独立的ofstream写同一个文件会发生什么
 * 编译运行：g++ test_ofstream_conflict.cpp -o test -pthread && ./test
 */

#include <iostream>
#include <fstream>
#include <thread>
#include <string>
#include <chrono>

// 线程1：使用ofstream1写文件
void writer1(const std::string& filename) {
    std::ofstream file(filename, std::ios::app);  // 独立的ofstream #1

    for (int i = 0; i < 5; i++) {
        file << "[线程1] 第" << i << "条消息：这是一段很长的日志内容，用于演示文件指针冲突问题\n";

        // 模拟一些处理时间
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    file.close();
}

// 线程2：使用ofstream2写同一个文件
void writer2(const std::string& filename) {
    std::ofstream file(filename, std::ios::app);  // 独立的ofstream #2

    for (int i = 0; i < 5; i++) {
        file << "[线程2] 第" << i << "条消息：另一段很长的日志内容，看看会不会和线程1的内容混在一起\n";

        // 模拟一些处理时间
        std::this_thread::sleep_for(std::chrono::milliseconds(15));
    }

    file.close();
}

int main() {
    const std::string filename = "conflict_test.log";

    // 先清空文件
    std::ofstream(filename, std::ios::trunc).close();

    std::cout << "启动两个线程，使用两个独立的ofstream写同一个文件...\n";

    // 创建两个线程，各自有独立的ofstream
    std::thread t1(writer1, filename);
    std::thread t2(writer2, filename);

    // 等待两个线程完成
    t1.join();
    t2.join();

    std::cout << "\n写入完成！请查看 conflict_test.log 文件\n";
    std::cout << "========================================\n";
    std::cout << "文件内容：\n";
    std::cout << "========================================\n";

    // 读取并显示文件内容
    std::ifstream result(filename);
    std::string line;
    while (std::getline(result, line)) {
        std::cout << line << "\n";
    }

    return 0;
}

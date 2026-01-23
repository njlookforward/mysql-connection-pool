/**
 * @brief 更激进的演示：强制触发文件指针冲突
 * 使用 flush() 和 seekp() 来暴露问题
 */

#include <iostream>
#include <fstream>
#include <thread>
#include <string>
#include <chrono>
#include <iomanip>

// 线程1：写文件，但每次写完都seek到文件末尾
void writer1(const std::string& filename) {
    std::ofstream file(filename, std::ios::app);

    for (int i = 0; i < 20; i++) {
        // 先获取当前文件位置
        auto pos = file.tellp();

        // 写入一些内容
        file << "A" << i << "=" << std::setw(5) << (i * 100);
        file.flush();  // 强制刷新到磁盘

        // 模拟处理
        std::this_thread::sleep_for(std::chrono::microseconds(100));

        // 继续写
        file << " [Writer1]\n";
        file.flush();
    }

    file.close();
}

// 线程2：同样操作
void writer2(const std::string& filename) {
    std::ofstream file(filename, std::ios::app);

    for (int i = 0; i < 20; i++) {
        auto pos = file.tellp();

        file << "B" << i << "=" << std::setw(5) << (i * 200);
        file.flush();

        std::this_thread::sleep_for(std::chrono::microseconds(100));

        file << " [Writer2]\n";
        file.flush();
    }

    file.close();
}

int main() {
    const std::string filename = "conflict_test_v2.log";

    // 清空文件
    std::ofstream(filename, std::ios::trunc).close();

    std::cout << "测试：两个独立的ofstream写同一文件（带flush）...\n\n";

    std::thread t1(writer1, filename);
    std::thread t2(writer2, filename);

    t1.join();
    t2.join();

    std::cout << "结果文件内容：\n";
    std::cout << "========================================\n";

    std::ifstream result(filename);
    std::string line;
    int line_num = 1;
    while (std::getline(result, line)) {
        std::cout << std::setw(2) << line_num++ << ": " << line << "\n";
    }

    std::cout << "========================================\n";

    // 检查是否有问题
    result.clear();
    result.seekg(0);
    bool has_problem = false;
    while (std::getline(result, line)) {
        // 检查是否有交错的情况：A和B的行混在一起
        bool has_A = line.find("A") != std::string::npos;
        bool has_B = line.find("B") != std::string::npos;
        bool has_W1 = line.find("Writer1") != std::string::npos;
        bool has_W2 = line.find("Writer2") != std::string::npos;

        // 如果一行同时有A和B，或者A和Writer2，或者B和Writer1，说明出问题了
        if ((has_A && has_B) || (has_A && has_W2) || (has_B && has_W1)) {
            std::cout << "⚠️  检测到混乱行: " << line << "\n";
            has_problem = true;
        }
    }

    if (!has_problem) {
        std::cout << "✅ 这次运行看起来没有明显的交错（但问题依然存在！）\n";
    }

    return 0;
}

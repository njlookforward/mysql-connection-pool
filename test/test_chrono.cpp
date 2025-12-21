#include <iostream>
#include <thread>
#include "../include/utils.h"

using namespace std;

int main()
{
    int64_t timestamp = currentTimeMillis();
    cout << "当前时间戳：" << timestamp << endl;

    // 可以用来计算时间差
    int64_t start_ms = currentTimeMillis();
    // 阻塞一个时间段chrono::duration就是指一个时间段
    std::this_thread::sleep_for(std::chrono::seconds(1));
    int64_t end_ms = currentTimeMillis();
    // 得到这个时间段的毫秒数
    cout << "时间段No1持续了 " << (end_ms - start_ms) << " ms." << endl;

    // 使用steady_clock得到的微秒数
    int64_t start_us = currentTimeMicros();
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    int64_t end_us = currentTimeMicros();
    cout << "时间段No2持续了 " << (end_us - start_us) << " us." << endl
         << "时间段No2持续了 " << chrono::duration_cast<chrono::milliseconds>(
            chrono::microseconds(end_us - start_us)
         ).count() << " ms." << endl;
    
    return 0;
}
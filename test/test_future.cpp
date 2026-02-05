#include <chrono>
#include <cmath>
#include <functional>
#include <future>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <thread>

using namespace std;

/**
 * @brief 这是对标准库<future>的学习
 *
 * 需要异步执行
 *  -- 简单任务，直接获取结果 使用std::async + std::future
 *  -- 需要在线程间传递结果   使用std::promise + std::future
 *  -- 需要将函数包装成任务   使用std::packaged_task
 *  -- 多个线程等待同一结果   使用std::shared_future
 *
 * <future>库的核心组件
 *  -- std::future          只读的异步结果，只能获取一次
 *  -- std::shared_future   可复制的异步结果（可多次获取）
 *  -- std::async           启动异步任务的便捷函数
 *  -- std::promise         生产者端：设置异步结果
 *  -- std::packaged_task
 * 包装可调用对象，使其异步化（意思是packaged_task执行起来是另起一个线程，然后执行；但是function同样是函数包装器，但是仅仅起到包装可调用对象的作用，仍然是在同一线程中被调用）###疑问
 * 回答：不是的，只有thread and
 * async才能拉起异步线程；function和packaged_task都是函数对象包装器，也都是函数对象，而且将packaged_task对象作为函数参数传入
 * 还要通过std::move移动传入才行，因为packaged_task禁止复制。选择packaged_task是因为主线程需要得到异步线程返回的结果，可以通过get_future得到future对象
 *
 *  -- std::promise/future  异常机制
 *
 * <future>中不同组件的作用（###疑问：是不是std::promise/std::packaged_task与std::future都是一一对应的，所以只能移动不能复制;只有shared_future是可以被共享的，因此可以复制）
 * promise/packaged_task/future都是独占共享状态所有权，promise只能set_value一次，如果可以复制，谁来set_value，而且只能设置一次；
 * future只能get一次，如果可以复制，其中一个get了，另一个就无法get了，因此future只能移动，不能复制
 *  -- std::future<T>           获取异步结果       只能移动，不能复制
 *  -- std::shared_future<T>    多次获取结果       可以复制
 *  -- std::promise<T>          设置异步结果       只能移动，不能复制
 *  -- std::packaged_task<T>    包装可调用对象     只能移动，不能复制
 *  -- std::async()             启动异步任务       这是函数模板
 *
 * 使用场景
 *  -- 简单异步执行     async + future      并行计算、IO密集任务
 *  -- 线程间传递结果   promise + future    生产者-消费者模式
 *  -- 异步函数包装     packaged_task       任务队列、线程池
 *  -- 多个等待者       shared_future       广播给多个线程
 */

/**
 * @example std::async  最简单的异步执行
 *
 */
int slow_calculation(int n) {
  std::cout << "计算开始..." << std::endl;
  this_thread::sleep_for(chrono::seconds(3)); // 睡眠2秒再进行计算

  return pow(n, 2); // base^exponent
}

// 南江，你与大人物之间差的是耐心与努力而已，加油，你的天太高了，要把握好自己的天分
void test_async() {
  // 启动异步任务，立即返回future
  future<int> result = async(launch::async, slow_calculation, 10);

  // 主线程可以继续做其他事情
  cout << "主线程继续执行后续任务，此时calculation线程已经开始并行计算了..."
       << endl;
  this_thread::sleep_for(chrono::milliseconds(500)); // 0.5秒
  cout << "主线程睡眠结束，等待2."
          "5秒的时间才能获得计算的异步结果，这个时间主线程阻塞等待异步计算线程"
          "结果..."
       << endl;

  int value = result.get(); // 等待并获取结果
  cout << "结果：" << value << endl;
}

/**
 * @example launch::async VS launch::deferred
 *
 * ###疑问：launch::async立即拉起一个异步线程；而launch::deferred其实仍然是在同一线程执行可调用对象
 * 回答：launch::async立即拉起异步线程，主线程get()/wait()时阻塞等待异步线程；
 * launch::deferred仍然在同一线程中执行，直到get()/wait()的时候才真正调用可调用对象
 */
int work() {
  // 工作线程，显示线程id，并打印立即执行
  cout << "[工作线程 " << this_thread::get_id() << " ]"
       << "立即执行了..." << endl;
  // 等待2秒钟，再返回结果，这样主线程
  this_thread::sleep_for(chrono::seconds(2));
  return 42;
}

// 南江，你一定可以把自己的专注找回来的，把自己融入到软件的设计中，体验其中的美妙，把自己找回来，融入到自身中
// 南江，我相信你，不要断，继续坚持下去
void test_async_deferred() {
  // 首先打印主线程的线程id
  cout << "[主线程 " << this_thread::get_id()
       << " ]验证async拉起异步线程，deferred在当前线程同步执行" << endl;
  // async: 立即在新线程中启动
  auto f1 = async(launch::async, work);
  cout << "async - 调用后可能已经立即开始" << endl;

  auto value1 = f1.get();
  cout << "得到异步线程执行结果 " << value1 << endl;

  // ###BUG 这里必须阻塞一会再执行，不然无法分清属于谁的work
  this_thread::sleep_for(chrono::seconds(2)); // 睡2秒

  // deferred: 延迟到get()/wait()函数才执行，而且是在同一线程中（###疑问
  // 这里需要确定，deferred模式启动的函数，是不是同步函数，直到get或者wait的时候才调用）-->
  // 回答：是的，是在同一线程中
  auto f2 = async(launch::deferred, work);
  cout << "deferred - 还没执行，调用get()之后开始执行" << endl;

  auto value = f2.get();
  cout << "deferred - 现在才执行" << endl;
}

/**
 * @example 3. std::promise + std::future - 线程间传递结果
 * promise<T>
 * 也是模板类，模板参数是预期返回结果的数据类型，与future的数据类型一致
 */
void producer(promise<int> prom) {
  // 做一些复杂的工作
  this_thread::sleep_for(chrono::milliseconds(2000));

  // 设置结果（只能设置一次）
  prom.set_value(100);
  // prom.set_value(1000);    // 错误！不能多次设置
}

void consumer(future<int> fut) {
  // 等待并获取结果
  auto value = fut.get();
  cout << "消费者收到 " << value << endl;
}

void test_promise_future() {
  // 创建promise/future对
  promise<int> prom;
  auto fut = prom.get_future();

  // 因为promise和future只能移动，不能复制，因此只能移动赋值，而不能拷贝赋值
  // 因此，将promise移动到生产者线程
  thread prod(&producer, std::move(prom));
  // 将future移动到消费者线程
  thread cons(&consumer, std::move(fut));

  prod.join();
  cons.join();
}

/**
 * @example 4. packaged_task 包装任务
 * ###疑问：不同于function<>包装可调用对象，packaged_task对象在执行可调用对象的时候，先拉起异步线程，然后执行异步函数是吗
 * 回答：无论是是function还是packaged_task都可以看做是函数对象，都是可调用对象，都无法自己拉起异步线程，只有作为参数构造thread对象才能创建新的异步线程；
 * 之所以使用packaged_task封装可调用对象，是因为能够让主线程得到get_future，然后接收到异步线程返回的结果
 * ###注意### 只有thread and async才能接受任何可调用对象，然后创建线程
 *
 * ###疑问：thread对象的构造，即使使用正常的函数，也是会创建一个新的线程，也就是异步线程去执行可调用对象，为什么得构造成packaged_task对象呢？
 * 回答：这里需要认清楚异步操作：实际上是主线程与异步线程两个为主体。主线程创建异步线程，然后立即返回，异步线程执行自己的操作，主线程需要拿到异步线程返回的结果
 * 因此future是主线程与异步线程之间的纽带，通过future得到异步线程返回的结果，而packaged_task是建立这个纽带的工具，可以get_future；一句话：packaged_task的独特价值就是获得返回值
 *
 * // 示例函数为什么需要detach，其实没有那么复杂
 * detach与join之间的区别
 * 1) detach之后线程独立运行，与主线程无关;
 * future.get()会阻塞直到任务完成，推荐在fire-and-forget场景下使用detach
 * 2）join必须等待线程完成，future.get()在join之后执行会立即返回，因为join已经完成阻塞等待异步线程执行结束，推荐在需要明确等待异步线程完成场景下使用join等待
 *  使用detach与join的时机：
 *  - 有future.get()  -->  两者都可以
 *  - fire-and-forget   --> detach
 *  - 必须确保完成  --> join
 */
int calculate(int x, int y) {
  cout << "计算中..." << endl;
  return x + y;
}

void test_packaged_task() {
  // 包装函数，使其可异步，packaged_task本身就是可调用对象
  packaged_task<int(int, int)> task(
      calculate); // 不可移动，而且仅仅传入可调用对象，不包含需要传入的参数

  // 在任务启动前获取future
  future<int> result = task.get_future();

  // ###疑问：thread对象的构造，即使使用正常的函数，也是会创建一个新的线程，也就是异步线程去执行可调用对象，为什么得构造成packaged_task对象呢？
  thread t(std::move(task), 10, 20); // 因为packaged_task只能移动，不能拷贝
  t.detach(); // ###疑问：为什么要detach呢？是因为调用packaged_task任务的thread都需要detach吗？

  // 获取结果
  cout << "结果：" << result.get() << endl;
}

/**
 * @example 5. shared_future 多个等待者
 *
 */
void wait_for_result(shared_future<int> fut, const string &name) {
  cout << name << " 等待中..." << endl;
  auto value = fut.get();
  cout << name << " 得到：" << value << "\n";
}

void test_shared_future() {
  promise<int> prom;
  future<int> fut = prom.get_future();

  // 转换为shared_future（可复制)
  shared_future<int> shared_fut = fut.share();

  // 建立三个异步线程
  thread t1(wait_for_result, shared_fut, "线程1");
  thread t2(wait_for_result, shared_fut, "线程2");
  thread t3(wait_for_result, shared_fut, "线程3");

  this_thread::sleep_for(chrono::seconds(2));
  prom.set_value(100); // 所有等待的线程都会被唤醒

  t1.join();
  t2.join();
  t3.join();
}

/**
 * @example 6.异常处理
 * 如果异步函数没有正确返回值，而是抛出异常，也可以捕获到
 */
void failing_task() { throw std::runtime_error("任务失败了"); }

void test_async_exception() {
  auto result = async(launch::async, failing_task);
  try {
    result.get(); // 调用get，异常会被重新抛出
  } catch (const std::runtime_error &e) {
    std::cerr << "异常：" << e.what() << '\n';
  }
}

/**
 * @example 7. wait() vs get() + wait_for()
 */
void test_wait_for_get() {
  auto fut = async(launch::async, [] {
    this_thread::sleep_for(chrono::seconds(2));
    return 25;
  });

  // wait，执行wait就会阻塞等待结果
  //   fut.wait();
  //   cout << "验证2秒之后，才会打印这句话" << endl
  //        << "value = " << fut.get() << endl;

  // 验证wait_for这里带有超时返回
  future_status status = fut.wait_for(chrono::milliseconds(100));
  if (status == future_status::ready) {
    cout << "就绪" << endl;
  } else {
    cout << "超时" << endl;
  }
  // get()：继续等待并获得结果（只能调用一次）
  cout << "仍然会等待一会才会打印结果，value = " << flush << fut.get() << endl;
  //   fut.get();    // 错误！抛出异常啦
}

/**
 * @example 8.线程池任务，适用于使用future
 * 只不过比较复杂，慢慢看，慢慢琢磨
 * ###重要
 * 这里面有一个非常好的技巧，使用shared_ptr包装后，只能移动的对象，通过shared_ptr也可以复制了
 */
template <typename F, typename... Args>
auto submit(F &&f, Args &&...args) -> future<decltype(f(args...))> {
  using RetType = decltype(f(args...));
  auto taskPtr = make_shared<packaged_task<RetType()>>(
      bind(std::forward<F>(f), std::forward<Args>(args)...));

  // ###疑问
  // 无论packaged_task被那个线程调用，与之相应的future都能够得到返回值，是吗？
  future<RetType> result = taskPtr->get_future();

  // 然后将可调用对象放入回调函数任务队列中，通过使用lambda表达式包装之后
  // 任务队列中的回调函数都是void()类型，因此通过以上的操作，抹去了函数的特有类型，变成了通用类型
  // 其实除了使用shared_ptr，使用移动构造也是可以的
  // task_queue.push([taskPtr] { (*taskPtr)(); });

  return result;
}

// 使用移动而不是shared_ptr
// ###BUG 我这里还有很多毛病
template <typename F, typename... Args>
auto submit_move(F &&f, Args &&...args) -> future<decltype(f(args...))> {
  using RetType = decltype(f(args...));
  // 得到可调用对象
  packaged_task<RetType()> task =
      bind(std::forward<F>(f), std::forward<Args>(args)...);
  // 得到future
  future<RetType> result = task.get_future();
  // 添加到任务队列
  // ###BUG1 C++14通用lambda捕获，而且可以正确地进行名字覆盖
  // ###BUG2 需要添加mutable.lambda表达式的operator()默认是const调用，但是
  // packaged_task的operator()是非const的，所以需要添加mutable，这样lambda表达式的operator就是非const的
  // @note
  // lambda表达式中，值捕获的变量不能被修改；但是引用捕获的变量可以被修改，因为const成员函数中，this指针是(如果是类的名称是Lambda)const
  // Lambda *const this； 因此它的数据成员全部都要用const修饰一遍;int -> const
  // int; int& -> int& const,它的意思是不能修改引用的绑定，但是
  // 可以修改引用绑定的对象；如果也想修改值捕获的对象，需要在Lambda表达式中加上mutable关键字
  /**
   * 1. 空类大小1字节；
   * 2. 自定义类中引用成员占8字节
   * 3. 对齐规则，按照自定义类中大小最大的变量的倍数
   * 4. 普通引用类型的大小，是被引用对象的大小，编译器是知道的
   */
  // task_queue.push([task = std::move(task)] mutable { task(); });
  // 返回future
  return result;
}

int main() {
  // test_async();
  test_async_deferred();
  // test_promise_future();
  // test_packaged_task();
  // test_shared_future();
  // test_async_exception();
  // test_wait_for_get();

  return 0;
}
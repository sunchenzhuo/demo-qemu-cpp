/**
 * @文件路径         : /shu/projects/demo-qemu-cpp/app/thread_safe_queue.hpp
 * @作者           : 树
 * @创建时间         : 2026-06-02 10:36:11
 * @最后编辑         : 树
 * @最后编辑时间       : 2026-06-02 13:35:29
 * @Version      : V1.0.0
 * @功能描述         :多个线程之间安全地传递数据。
                     生产者线程调用 push() 放数据，消费者线程调用 waitPop() 或 tryPopLatest() 取数据。
 * @Copyright    : Copyright (c) 2026 by 树, All Rights Reserved.
 */
#pragma once

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <queue>

/**
 * @brief 线程安全队列模板类。
 *
 * 该类用于在多线程环境下安全地传递数据。
 * 内部通过 mutex 保护队列数据，通过 condition_variable 实现阻塞等待。
 *
 * 典型使用场景：
 * 1. 通信线程接收数据后，将状态消息 push 到队列；
 * 2. 业务线程或监控线程从队列中取出数据进行处理；
 * 3. 避免多个线程直接共享复杂数据导致数据竞争。
 *
 * @tparam T 队列中存储的数据类型。
 */
template <typename T>
class ThreadSafeQueue
{
private:
    /**
     * @brief 保护队列的互斥锁。
     *
     * mutable 表示即使在 const 成员函数中，也允许对 mutex_ 加锁。
     * 例如 size() 是 const 函数，但读取 queue_.size() 时仍然需要加锁。
     */
    mutable std::mutex mutex_;

    /**
     * @brief 条件变量，用于实现线程等待和通知。
     *
     * 当队列为空时，消费者线程可以等待 cond_。
     * 当生产者线程 push 新数据后，通过 notify_once() 唤醒等待线程。
     */
    std::condition_variable cond_;

    /**
     * @brief 实际存储数据的队列。
     *
     * 所有对 queue_ 的访问都必须在 mutex_ 保护下进行。
     */
    std::queue<T> queue_;

public:
    /**
     * @brief 向队列尾部添加一个元素。
     *
     * 该函数用于生产者线程向队列中写入数据。
     * 写入时会先加锁，确保多个线程同时 push 时不会破坏队列结构。
     * 数据写入完成后，会调用 notify_once() 唤醒一个正在等待数据的消费者线程。
     *
     * @param value 待写入队列的数据。
     */
    void push(const T &value)
    {
        {
            // 加锁保护队列写入操作。
            std::lock_guard<std::mutex> lock(mutex_);

            // 将数据拷贝到队列尾部。
            queue_.push(value);
        }
        // 数据写入完成后释放锁，再通知等待线程。
        // 这样被唤醒的线程可以尽快拿到锁并读取数据。
        cond_.notify_one();
    }

    /**
     * @brief 等待并弹出队列头部元素。
     *
     * 如果队列中有数据，则立即取出队头元素。
     * 如果队列为空，则最多等待 timeout 指定的时间。
     * 在等待期间，如果其他线程调用 push() 插入数据，本函数会被唤醒并取出数据。
     *
     * @param value 用于接收弹出的队头元素。
     * @param timeout 最大等待时间。
     * @return true 成功取出一个元素。
     * @return false 等待超时，队列仍然为空。
     */
    bool waitPop(T &value, std::chrono::milliseconds timeout)
    {
        // 使用 unique_lock，因为 condition_variable::wait_for 需要能够临时释放和重新加锁。
        std::unique_lock<std::mutex> lock(mutex_);

        // 等待队列非空。
        // 如果队列已经有数据，立即返回 true。
        // 如果队列为空，则等待 timeout 时间。
        // lambda 条件用于防止虚假唤醒。
        const bool ready = cond_.wait_for(lock, timeout, [this]()
                                          { return !queue_.empty(); });

        // 等待超时，并且队列仍然为空。
        if (!ready)
        {
            return false; // 超时，未能弹出元素
        }

        // 取出队头元素。
        value = queue_.front();
        // 从队列中移除已经取出的元素。
        queue_.pop();

        return true;
    }

    /**
     * @brief 弹出队列中的最新元素。
     *
     * 该函数不会等待。
     * 如果队列为空，立即返回 false。
     * 如果队列不为空，会丢弃旧数据，只保留并返回最后一个元素。
     *
     * 适合状态类数据场景：
     * 例如底盘状态、电池电压、传感器数据等，业务方通常只关心最新状态，
     * 不一定需要逐条处理历史状态。
     *
     * @param value 用于接收队列中的最新元素。
     * @return true 成功获取最新元素。
     * @return false 队列为空，未获取到数据。
     */
    bool tryPopLatest(T &value)
    {
        // 加锁保护队列读取和弹出操作。
        std::lock_guard<std::mutex> lock(mutex_);

        // 队列为空，无法取出数据。
        if (queue_.empty())
        {
            return false; // 队列为空，无法弹出元素
        }

        // 持续弹出队列元素。
        // 每次都把当前队头赋值给 value。
        // 循环结束后，value 中保存的就是最后一个元素，也就是最新数据。
        while (!queue_.empty())
        {
            value = queue_.front();
            queue_.pop();
        }
        return true; // 成功弹出最新元素
    }

    /**
     * @brief 获取当前队列元素数量。
     *
     * 该函数只读取队列大小，不修改队列内容。
     * 但由于 queue_ 可能被其他线程同时 push 或 pop，
     * 所以读取 size 时仍然需要加锁保护。
     *
     * @return 当前队列中的元素数量。
     */
    std::size_t size() const
    {
        // 即使是 const 函数，也要加锁保护共享队列。
        // mutex_ 使用 mutable 修饰，因此可以在 const 函数中加锁。
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }
};
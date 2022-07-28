#include <new>
#include <cassert>

#include <cstring>
#include <string.h>
#include <iostream>

#include <libdsa.h>
#include <thread>
#include <queue>
#include <chrono>
using namespace std::chrono;

std::queue <char*> move_failure_queue;
const long int OneG = (1024 * 1024 * 1024);
// 假设只有两个DSA硬件
int dsa_num = 8;
// 一次copy的大小
int len = 32 * 1024;

// 假设要copy 1G的数据
char dst[OneG];
char src[OneG];

// 要开启的线程数为thread_num
int threads_num = 16;

void mem_move(const char* src, char* dst, size_t len, libdsa::WorkQueue* wq ) {
    void* comp_buffer = aligned_alloc(32, 32);
    auto* comp = new (comp_buffer)libdsa::MoveCompletion;
    bool success = wq->submitMove(comp, src, dst, len);
    bool done = comp->wait();
    if (!success) {
        // 将copy失败的地址存放在队列中
        move_failure_queue.push(dst);
    }
}

void thread_mem_move(const char* src, char* dst, libdsa::WorkQueue* wq[8]) {
    // 总共有16个线程，每个线程1次copy len大小的数据，1此循环提交dsa_num次，总共要提交的次数为sub_num
    int sub_num = OneG / (16 * len * dsa_num);

    for (int i = 0; i < sub_num; ++i) {  
        mem_move(src + 8 * i * len, dst + 2 * i * len, len, wq[0]);
        mem_move(src + (8 * i + 1) * len, dst + (8 * i + 1) * len, len, wq[1]);
        mem_move(src + (8 * i + 2) * len, dst + (8 * i + 2) * len, len, wq[2]);
        mem_move(src + (8 * i + 3) * len, dst + (8 * i + 3) * len, len, wq[3]);
        mem_move(src + (8 * i + 4) * len, dst + (8 * i + 4) * len, len, wq[4]);
        mem_move(src + (8 * i + 5) * len, dst + (8 * i + 5) * len, len, wq[5]);
        mem_move(src + (8 * i + 6) * len, dst + (8 * i + 6) * len, len, wq[6]);
        mem_move(src + (8 * i + 7) * len, dst + (8 * i + 7) * len, len, wq[7]);

    }
}


int main() {
    //定义时间变量
    high_resolution_clock::time_point dsa_start, dsa_end, cpu_start, cpu_end;
    std::chrono::milliseconds dsa_ms, cpu_ms;
    
    memset(dst, 0, sizeof(dst));
    std::fill(src, src + OneG, '1');
    src[OneG - 1] = '\0';// 防止乱码

    // 计算cpu复制1G数据用时
    cpu_start = high_resolution_clock::now();
    for (unsigned int i = 0; i < (OneG / len); ++i) {
        memcpy(dst + i * len, src + i * len, len);
    }
    cpu_end = high_resolution_clock::now();
    cpu_ms = duration_cast<std::chrono::milliseconds>(cpu_end - cpu_start);
    float cpu_band_width = 1000.0 / cpu_ms.count();
    std::cout << "cpu copy successful" << std::endl;
    std::cout << "cpu cost time:" << cpu_ms.count() << "ms" << std::endl;
    std::cout << "cpu band width:" << cpu_band_width << "G/s" << std::endl;

    // 目标地址重新置0
    memset(dst, 0, sizeof(dst));
    std::thread threads[threads_num];
    
    // 此处计时start
    dsa_start = high_resolution_clock::now();

    libdsa::WorkQueue* wq0 = libdsa::WorkQueue::getWorkQueueByDevName("wq0.0");
    libdsa::WorkQueue* wq1 = libdsa::WorkQueue::getWorkQueueByDevName("wq0.1");
    libdsa::WorkQueue* wq2 = libdsa::WorkQueue::getWorkQueueByDevName("wq0.2");
    libdsa::WorkQueue* wq3 = libdsa::WorkQueue::getWorkQueueByDevName("wq0.3");
    libdsa::WorkQueue* wq4 = libdsa::WorkQueue::getWorkQueueByDevName("wq0.4");
    libdsa::WorkQueue* wq5 = libdsa::WorkQueue::getWorkQueueByDevName("wq0.5");
    libdsa::WorkQueue* wq6 = libdsa::WorkQueue::getWorkQueueByDevName("wq0.6");
    libdsa::WorkQueue* wq7 = libdsa::WorkQueue::getWorkQueueByDevName("wq0.7");
    libdsa::WorkQueue* wq[8] = { wq0,wq1,wq2,wq3,wq4,wq5,wq6,wq7 };


    // 16个线程平分1G的空间，同时提交任务
    for (int i = 0; i < threads_num; ++i) {
        threads[i] = std::thread(thread_mem_move, src + i * (OneG / threads_num), dst + i * (OneG / threads_num), wq);
    }
    
    for (int j = 0; j < threads_num; ++j) {
        threads[j].join();
    }
    
    // join函数是在所有子线程都执行完后，主线程才往下走，因此此时计时结束
    // 此处计时end
    dsa_end = high_resolution_clock::now();
    dsa_ms = duration_cast<std::chrono::milliseconds>(dsa_end - dsa_start);
    float dsa_band_width = 1000.0 / dsa_ms.count();
    if (move_failure_queue.empty()) {
        std::cout<<"dsa cpoy successful"<<std::endl;
        std::cout<<"dsa cost time:"<<dsa_ms.count()<<"ms"<<std::endl;
        std::cout<<"dsa band width:" << dsa_band_width << "G/s" << std::endl;
    }
    else {
        while (!move_failure_queue.empty()) {
            std::cout << move_failure_queue.front() << " move failure\n" << std::endl;
            move_failure_queue.pop();
        }
    }
    return 0;
}
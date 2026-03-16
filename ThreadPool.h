#pragma once
#include "EventLoop.h"
#include "WorkerThread.h"
#include <stdbool.h>
#include <vector>
using namespace std;
//线程池结构体
class ThreadPool
{
private:
    bool m_isStart;
    int m_threadNum;
    int m_index;
    EventLoop* m_mainloop;
    vector<WorkerThread*> m_workerThreads;
public:
    ThreadPool(EventLoop* mainloop,int count);
    ~ThreadPool();
    //启动线程池
    void run();
    //取出线程池中的，某个子线程的反应堆实例
    EventLoop* takeWorkerEventLoop();
};


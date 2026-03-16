#pragma once
#include <thread>
#include <mutex>
#include <condition_variable>
#include "EventLoop.h"
using namespace std;
//子线程结构体
class WorkerThread
{
private:
    thread* m_thread;
    thread::id m_threadId;
    string m_name;
    mutex m_mutex;
    condition_variable m_cond;
    EventLoop* m_evloop;
    void running();

public:
    WorkerThread(int index);
    ~WorkerThread();
    void run();
    inline EventLoop* getEvloop(){
        return m_evloop;
    }
};

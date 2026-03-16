#include "WorkerThread.h"

WorkerThread::WorkerThread(int index)
{
    m_thread=nullptr;
    m_evloop=nullptr;
    m_threadId=thread::id();
    m_name="SubThread-"+to_string(index);
}

WorkerThread::~WorkerThread()
{
    if (m_thread!=nullptr)
    {
        delete m_thread;
    }
    
}

void WorkerThread::running()
{
    m_mutex.lock();
    m_evloop= new EventLoop(m_name);
    m_mutex.unlock();
    m_cond.notify_one();
    m_evloop->Run();   
}

void WorkerThread::run()
{
    m_thread=new thread(&WorkerThread::running,this);
    unique_lock<mutex> locker(m_mutex);
    while (m_evloop==nullptr)
    {
        m_cond.wait(locker);
    }
    
}
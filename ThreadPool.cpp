#include "ThreadPool.h"
#include <assert.h>
ThreadPool::ThreadPool(EventLoop* mainloop,int count)
{
    m_isStart=false;
    m_index=0;
    m_mainloop=mainloop;
    m_threadNum=count;
    m_workerThreads.clear();
}

ThreadPool::~ThreadPool()
{
    for (auto& it:m_workerThreads)
    {
        delete it;
    }
    
}

void ThreadPool::run()
{
    assert(!m_isStart);
    if (m_mainloop->getThreadID()!=this_thread::get_id())
    {
        exit(0);
    }
    m_isStart=true;
    if (m_threadNum>0)
    {
        for (int i = 0; i < m_threadNum; i++)
        {
            WorkerThread* subThread=new WorkerThread(i);
            subThread->run();
            m_workerThreads.push_back(subThread);
        }      
    }
}

EventLoop* ThreadPool::takeWorkerEventLoop()
{
    assert(m_isStart);
    if (m_mainloop->getThreadID()!=this_thread::get_id())
    {
        exit(0);
    }
    //从线程池中找一个子线程，
    EventLoop* evloop=m_mainloop;
    if (m_threadNum>0)
    {
        evloop=m_workerThreads[m_index]->getEvloop();
        m_index=++m_index%m_threadNum;
    }
    return evloop;

}
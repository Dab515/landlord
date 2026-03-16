#pragma once
#include "EventLoop.h"
#include "ThreadPool.h"
#include "TcpConnection.h"
class TcpServer
{
private:
    int m_threadNum;
    EventLoop* m_mainLoop;
    ThreadPool* m_threadPool;
    int m_lfd;
    unsigned short m_port;

public:
    TcpServer(unsigned short port,int threadNum);
    ~TcpServer();
    void setListen();
    void run();
    static int acceptConnection(void* arg);

};


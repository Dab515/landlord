#pragma once
#include "Channel.h"
#include "EventLoop.h"
#include "Dispatcher.h"
#include <string>
#include <sys/epoll.h>
using namespace std;
class EpollDispatcher:public Dispatcher
{
private:
    int m_epfd;
    struct epoll_event* m_events;
    const int m_maxNode= 520;
    int epollCtl(int op);
public:
    EpollDispatcher(EventLoop* evloop);
    ~EpollDispatcher();
    //add
    int add() override;
    //remove
    int remove() override;
    //modify
    int modify() override;
    //事件监测
    int dispatch(int timeout=2) override;
  
};


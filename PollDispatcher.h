#pragma once
#include "Channel.h"
#include "EventLoop.h"
#include "Dispatcher.h"
#include <string>
#include <sys/poll.h>
using namespace std;
class PollDispatcher:public Dispatcher
{
private:
    int m_maxfd;
    struct pollfd* m_fds;
    const int m_maxNode=1024;
public:
    PollDispatcher(EventLoop* evloop);
    ~PollDispatcher();
    //add
    int add() override;
    //remove
    int remove() override;
    //modify
    int modify() override;
    //事件监测
    int dispatch(int timeout=2) override;
  
};


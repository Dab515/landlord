#pragma once
#include "Channel.h"
#include "EventLoop.h"
#include "Dispatcher.h"
#include <string>
#include <sys/select.h>
using namespace std;

class SelectDispatcher:public Dispatcher
{
private:
    fd_set m_readset;
    fd_set m_writeset;
    const int m_maxSize=1024;
    void setFdSet();
    void clearFdSet();

public:
    SelectDispatcher(EventLoop* evloop);
    ~SelectDispatcher();
    //add
    int add() override;
    //remove
    int remove() override;
    //modify
    int modify() override;
    //事件监测
    int dispatch(int timeout=2) override;

};


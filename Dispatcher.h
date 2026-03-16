#pragma once
#include "Channel.h"
#include "EventLoop.h"
#include <string>

using namespace std;
// 前置声明，避免头文件循环包含
class EventLoop;

class Dispatcher
{
public:
    Dispatcher(EventLoop* evloop);
    virtual ~Dispatcher();
    //add
    virtual int add();
    //remove
    virtual int remove();
    //modify
    virtual int modify();
    //事件监测
    virtual int dispatch(int timeout=2);
    //更新Channel
    inline void setChannel(Channel* channel)
    {
        m_channel=channel;
    }

protected:   
    string m_name=string();
    Channel* m_channel;
    EventLoop* m_evloop;
};


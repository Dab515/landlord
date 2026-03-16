#include "Channel.h"

Channel::Channel(int fd,FDEvent events,handleFunc readFunc,handleFunc writeFunc,handleFunc destoryFunc,void* arg)
{
    m_arg=arg;
    m_events=(int)events;
    m_fd=fd;
    readCallBack=readFunc;
    writeCallBack=writeFunc;
    destoryCallBack=destoryFunc;
}

void Channel::writeEventEnable(bool flag)
{
    if (flag)
    {
        //m_events |= (int)FDEvent::WriteEvent;
        m_events |= static_cast<int>(FDEvent::WriteEvent);
    }
    else
    {
        m_events=m_events & ~(int)(FDEvent::WriteEvent);
    }
    
}

bool Channel::isWriteEventEnable()
{
    return m_events & (int)FDEvent::WriteEvent;
}
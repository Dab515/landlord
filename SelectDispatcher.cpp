#include "SelectDispatcher.h"
#include "Dispatcher.h"
//#include <sys/select.h>
SelectDispatcher::SelectDispatcher(EventLoop* evloop):Dispatcher(evloop)
{
    FD_ZERO(&m_readset);
    FD_ZERO(&m_writeset);
    m_name="Select";
}

SelectDispatcher::~SelectDispatcher()
{
}
void SelectDispatcher::setFdSet()
{
    if (m_channel->getEvent()&(int)FDEvent::ReadEvent)
    {
        FD_SET(m_channel->getSocket(),&m_readset);
    }
    if (m_channel->getEvent()&(int)FDEvent::WriteEvent)
    {
        FD_SET(m_channel->getSocket(),&m_writeset);
    }
    
}
void SelectDispatcher::clearFdSet()
{
    if (m_channel->getEvent()&(int)FDEvent::ReadEvent)
    {
        FD_CLR(m_channel->getSocket(),&m_readset);
    }
    if (m_channel->getEvent()&(int)FDEvent::WriteEvent)
    {
        FD_CLR(m_channel->getSocket(),&m_writeset);
    }
}

int SelectDispatcher::add() 
{
    if (m_channel->getSocket()>=m_maxSize)
    {
        return -1;
    }
    
    setFdSet();
    return 0;

}

int SelectDispatcher::remove()
{
    clearFdSet();
    m_channel->destoryCallBack(const_cast<void*>(m_channel->getArg()));
    return 0;
}

int SelectDispatcher::modify()
{
    setFdSet();
    clearFdSet();
    
    return 0;

}

int SelectDispatcher::dispatch(int timeout)
{
    struct timeval val;
    val.tv_sec=timeout;
    val.tv_usec=0;
    fd_set rdtmp=m_readset;
    fd_set wrtmp=m_writeset;
    int count=select(m_maxSize,&rdtmp,&wrtmp,NULL,&val);
    if (count==-1)
    {
        perror("select");
        exit(0);
    }
    for (int i = 0; i < m_maxSize; i++)
    {
        if (FD_ISSET(i,&rdtmp))
        {
            m_evloop->eventActivate(i,(int)FDEvent::ReadEvent);
        }
        if (FD_ISSET(i,&wrtmp))
        {
            m_evloop->eventActivate(i,(int)FDEvent::WriteEvent);
        }          
    }
    return 0;

}
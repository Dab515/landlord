#include "Dispatcher.h"
#include <sys/epoll.h>
#include <stdlib.h>
#include <unistd.h>
#include "EventLoop.h"
#include "EpollDispatcher.h"

//static加上后，函数作用域就是当前源文件
EpollDispatcher::EpollDispatcher(EventLoop* evloop):Dispatcher(evloop)
{
    m_epfd=epoll_create(10);
    if (m_epfd==-1)
    {
        perror("epoll_create");
        exit(0);
    }
    m_events=new epoll_event[m_maxNode]; 
    m_name="Epoll";
}

EpollDispatcher::~EpollDispatcher()
{
    close(m_epfd);
    delete[] m_events;
}

int EpollDispatcher::epollCtl(int op)
{
    struct epoll_event ev;
    ev.data.fd=m_channel->getSocket();
    int events=0;
    if (m_channel->getEvent()&(int)FDEvent::ReadEvent)
    {
        events|=EPOLLIN;
    }
    if (m_channel->getEvent()&(int)FDEvent::WriteEvent)
    {
        events|=EPOLLOUT;
    }
    ev.events=events;
    int ret=epoll_ctl(m_epfd,op,m_channel->getSocket(),&ev);
    return ret;
}


int EpollDispatcher::add()
{
    int ret=epollCtl(EPOLL_CTL_ADD);
    if (ret==-1)
    {
        perror("epollCtl add");
        exit(0);
    }
    return ret;    
}

int EpollDispatcher::remove()
{
    int ret=epollCtl(EPOLL_CTL_DEL);
    if (ret==-1)
    {
        perror("epollCtl delete");
        exit(0);
    }
    m_channel->destoryCallBack(const_cast<void*>(m_channel->getArg()));
    return ret;    

}

int EpollDispatcher::modify()
{
    int ret=epollCtl(EPOLL_CTL_MOD);
    if (ret==-1)
    {
        perror("epollCtl modify");
        exit(0);
    }
    return ret;

}

int EpollDispatcher::dispatch(int timeout)
{
    int count=epoll_wait(m_epfd,m_events,m_maxNode,timeout*1000);
    for (int i = 0; i < count; i++)
    {
        int events=m_events[i].events;
        int fd=m_events[i].data.fd;
        if (events & EPOLLERR || events & EPOLLHUP)
        {
            //对方断开连接，删除fd
            //epollRemove(channel,evloop);
            continue;
        }
        if (events & EPOLLIN)
        {
                      
            m_evloop->eventActivate(fd, (int)FDEvent::ReadEvent);
        }
        if (events & EPOLLOUT)
        {
            m_evloop->eventActivate(fd, (int)FDEvent::WriteEvent);
        }
        
    }
    return 0;
   
}



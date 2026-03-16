#include "EventLoop.h"
#include "SelectDispatcher.h"
#include "EpollDispatcher.h"
#include "PollDispatcher.h"
#include <sys/socket.h>
#include <unistd.h>
#include <assert.h>
#include <cstring>
EventLoop::EventLoop(/* args */):EventLoop(string())
{

}
void EventLoop::taskWakeUp()
{
    const char* msg="这是测试写数据模块的。。。";
    write(m_socketPair[0],msg,strlen(msg));
}
int EventLoop::readLocalMsg(void* arg)
{
    EventLoop* evloop=static_cast<EventLoop*>(arg) ;
    char buf[256];
    read(evloop->m_socketPair[1],buf,sizeof buf);
    return 0;
}
int EventLoop::readMessage()
{
    char buf[256];
    read(m_socketPair[1],buf,sizeof buf);
    return 0;

}
EventLoop::EventLoop(const string threadName)
{
    m_isQuit=true;
    m_threadID=this_thread::get_id();
    //strcpy(evloop->treadName,treadName==NULL?"MainTread":treadName);
    m_threadName=threadName==string()?"MainTread":threadName;
    //让evloop的dispatcher使用epoll的各类函数
    //evloop->dispatcher=&SelectDispatche;
    m_dispatcher=new EpollDispatcher(this);
    m_channelMap.clear();
    if(-1==socketpair(AF_UNIX,SOCK_STREAM,0,m_socketPair))
    {
        perror("socketpair");
        exit(0);
    }
    //指定规则 [0]发送数据 [1]接受数据
    //struct Channel* channel=channelInit(evloop->socketPair[1],ReadEvent,readLocalMsg,
    //NULL,NULL,evloop);
#if 0
    Channel* channel = new Channel(m_socketPair[1],FDEvent::ReadEvent,
        readLocalMsg, nullptr,nullptr,this);
#else
    //绑定bind
    auto obj=bind(&EventLoop::readMessage,this);
    Channel* channel = new Channel(m_socketPair[1],FDEvent::ReadEvent,
        obj, nullptr,nullptr,this);
#endif
    
    //channel添加到任务队列
    AddTask(channel,ElemType::ADD);

    

}
EventLoop::~EventLoop()
{
}

int EventLoop::Run()
{
    m_isQuit=false;
    if (m_threadID!=this_thread::get_id())
    {
        return -1;
    }
    
    //循环进行事件处理
    while (!m_isQuit)
    {
        m_dispatcher->dispatch();
        
        ProcessTaskQ();
        
    }
    return 0;

}
int EventLoop::eventActivate(int fd,int event)
{
    if (fd<0)
    {
        return -1;
    }
    Channel* channel=m_channelMap[fd];
    assert(channel->getSocket()==fd);
    if (event & (int)FDEvent::ReadEvent && channel->readCallBack)
    {
        channel->readCallBack(const_cast<void*>(channel->getArg()));
    }
    if (event & (int)FDEvent::WriteEvent && channel->writeCallBack)
    {
        channel->writeCallBack(const_cast<void*>(channel->getArg()));
    }
    return 0;

}

int EventLoop::AddTask(struct Channel* channel,ElemType type)
{
    //加锁
    //pthread_mutex_lock(&evloop->mutex);
    m_mutex.lock();
    //创建新节点
    ChannelElement* node=new ChannelElement;
    node->channel=channel;
    node->type=type;
    m_taskQ.push(node);
    
    //解锁
    //pthread_mutex_unlock(&evloop->mutex);
    m_mutex.unlock();
    //处理节点
    if(m_threadID==this_thread::get_id())
    {
        //当前子线程（基于子线程的角度分析）
        ProcessTaskQ();
    }
    else
    {
        //主线程==告诉子线程处理任务队列中的任务
        //1、子线程在工作 2、子线程被阻塞：select、epoll、poll
        taskWakeUp();
    }
    return 0;
}
int EventLoop::ProcessTaskQ()
{
    //pthread_mutex_lock(&evloop->mutex);
    
    //取出头节点
    while (!m_taskQ.empty())
    {
        m_mutex.lock();
        ChannelElement* node=m_taskQ.front();
        m_taskQ.pop();//删除节点
        m_mutex.unlock();

        Channel* channel=node->channel;
        if (node->type==ElemType::ADD)
        {
            Add(channel);
        }
        else if (node->type==ElemType::DELECT)
        {
            Remove(channel);
            
        }
        else if (node->type==ElemType::MODIFY)
        {
            Modify(channel);
        }
        delete node;  
    }
    //pthread_mutex_unlock(&evloop->mutex);
    return 0;

}

int EventLoop::Add(Channel* channel)
{
    int fd=channel->getSocket();

    //找到fd对应的数组元素位置
    if (m_channelMap.find(fd)==m_channelMap.end())
    {
        m_channelMap.insert(make_pair(fd,channel));
        //m_channelMap[fd]=channel;
        m_dispatcher->setChannel(channel);
        int ret=m_dispatcher->add();
        return ret;
    }
    return -1;

}

int EventLoop::Remove(Channel* channel)
{
    int fd=channel->getSocket();
    if (m_channelMap.find(fd)==m_channelMap.end())   
    {
        //没有
        return -1;
        
    }
    m_dispatcher->setChannel(channel);
    return m_dispatcher->remove();
}

int EventLoop::Modify(Channel* channel)
{
    int fd=channel->getSocket();
    if (m_channelMap.find(fd)==m_channelMap.end())   
    {
        //没有
        return -1;
        
    }
    m_dispatcher->setChannel(channel);
    return m_dispatcher->modify();
}

int EventLoop::freeChannel(Channel* channel)
{
    //删除channel和fd的关系
    
    auto it=m_channelMap.find(channel->getSocket());
    if (it!=m_channelMap.end())
    {
        m_channelMap.erase(it);
        close(channel->getSocket());
        delete channel;
    }
    return 0;
}

#pragma once
#include "Dispatcher.h"
#include <thread>
#include "Channel.h"
#include <queue>
#include <map>
#include <mutex>
#include <string>
using namespace std;

enum class ElemType:char{
    ADD,
    DELECT,
    MODIFY
};
//任务队列节点
struct ChannelElement
{
    ElemType type;//如何处理该节点中的channel
    Channel* channel;
};

class Dispatcher;

class EventLoop
{
private:
    bool m_isQuit;
    //该指针指向子类的实例 epoll、poll、select
    Dispatcher* m_dispatcher;
    //任务队列
    queue<ChannelElement*> m_taskQ;
    //channelMap
    map<int, Channel*> m_channelMap;
    //线程ID，name, mutex
    thread::id m_threadID;
    string m_threadName;
    mutex m_mutex;
    int m_socketPair[2];//存储本地通信的fd 
    void taskWakeUp();
public:
    EventLoop(/* args */);
    EventLoop(const string threadName);
    ~EventLoop();
    //启动反应堆
    int Run();
    //处理被激活的文件fd
    int eventActivate(int fd,int event);
    //添加任务到任务队列
    int AddTask(struct Channel* channel,ElemType type);
    //处理任务队列中的任务
    int ProcessTaskQ();
    //添加头节点
    int Add(Channel* channel);
    //删除头节点
    int Remove(Channel* channel);
    //修改节点
    int Modify(Channel* channel);
    //释放channel
    int freeChannel(Channel* channel);
    static int readLocalMsg(void* arg);
    int readMessage();
    inline thread::id getThreadID(){
        return m_threadID;
    }
    inline string getThreadName(){
        return m_threadName;
    }
};


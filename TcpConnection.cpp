#include "TcpConnection.h"
#include <iostream>

TcpConnection::TcpConnection(int fd,EventLoop* evloop)
{
    m_evloop=evloop;
    m_readBuffer=new Buffer(10240);
    m_writeBuffer=new Buffer(10240);
    m_request=new HttpRequest;
    m_response=new HttpResponse;
    //sprintf(conn->name,"Connection-%d",fd);
    m_name="Connection-"+to_string(fd);
    m_channel=new Channel(fd,FDEvent::ReadEvent,processRead,processWrite,destory,this);
    //m_channel=new channelInit(fd,ReadEvent,processRead,processWrite,tcpConnectionDestory,conn);
    //eventLoopAddTask(evloop,conn->channel,ADD);
    m_evloop->AddTask(m_channel,ElemType::ADD);
}

TcpConnection::~TcpConnection()
{
    if (m_readBuffer && m_readBuffer->readAbleSize()==0
        && m_writeBuffer && m_writeBuffer->readAbleSize()==0)
    {
        delete m_readBuffer;
        delete m_writeBuffer;      
        delete m_request;
        delete m_response;
        m_evloop->freeChannel(m_channel);
    }
}

int TcpConnection::processRead(void* arg)
{
    TcpConnection* conn=static_cast<TcpConnection*>(arg);
    int socket=conn->m_channel->getSocket();
    int count=conn->m_readBuffer->socketRead(socket);
    std::cout<<"接收到的http请求数据:\n"<<conn->m_readBuffer->data()<<std::endl;
    //Debug("接收到http的请求数据: %s",conn->m_readBuffer->data());
    if (count>0)
    {
        //接收到了Http请求，解析http请求
#ifdef MSG_SEND_AUTO
        //writeEventEnable(conn->channel,true);
        conn->m_channel->writeEventEnable(true);
        conn->m_evloop->AddTask(conn->m_channel,ElemType::MODIFY);
        
#endif
        //bool flag=paresHttpRequest(conn->request,conn->readBuffer,conn->response,conn->writeBuffer,socket);
        bool flag=conn->m_request->paresHttpRequest(conn->m_readBuffer,conn->m_response,conn->m_writeBuffer,socket);
        if (!flag)
        {
            string errMsg="Http/1.1 400 Bad Request\r\n\r\n";
            conn->m_writeBuffer->appendString(errMsg.data());
        }
        
    }
    else
    {
#ifdef MSG_SEND_AUTO
        //断开连接
        conn->m_evloop->AddTask(conn->m_channel,ElemType::DELECT);
        
#endif
 
    }
    
#ifndef MSG_SEND_AUTO
    conn->m_evloop->AddTask(conn->m_channel,ElemType::DELECT);

#endif
    return 0;
}

int TcpConnection::processWrite(void* arg)
{
    TcpConnection* conn=static_cast<TcpConnection*>(arg);
    //发送数据
    int count=conn->m_writeBuffer->sendData(conn->m_channel->getSocket());
    if (count>0)
    {
        //判断数据是不是发送完了
        if (conn->m_readBuffer->readAbleSize()==0)
        {
            //1、不再检测事件--修改channel中保存的事件
            conn->m_channel->writeEventEnable(false);
            //2、修改dispatcher检测的事件--添加任务节点
            conn->m_evloop->AddTask(conn->m_channel,ElemType::MODIFY);
            //3、删除这个节点
            conn->m_evloop->AddTask(conn->m_channel,ElemType::DELECT);
        }
        
    }
    return 0; 
}

int TcpConnection::destory(void* arg)
{
    TcpConnection* conn=static_cast<TcpConnection*>(arg);
    if (conn!=nullptr)
    {
        delete conn;
    }
    
    return 0;
}
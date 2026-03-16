#pragma once
#include "Buffer.h"
#include "EventLoop.h"
#include "Channel.h"
#include "HttpRequest.h"
#include "HttpResponse.h"
//#define MSG_SEND_AUTO
class TcpConnection
{
private:
    string m_name;
    Buffer* m_readBuffer;
    Buffer* m_writeBuffer;
    Channel* m_channel;
    EventLoop* m_evloop;
    //http
    HttpRequest* m_request;
    HttpResponse* m_response;
public:
    TcpConnection(int fd,EventLoop* evloop);
    ~TcpConnection();
    static int processRead(void* arg);
    static int processWrite(void* arg);
    static int destory(void* arg);

};

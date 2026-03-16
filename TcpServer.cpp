#include "TcpServer.h"
#include <sys/socket.h>
#include <arpa/inet.h>
TcpServer::TcpServer(unsigned short port,int threadNum)
{
    
    m_threadNum=threadNum;
    m_mainLoop=new EventLoop();
    m_port=port;
    m_threadPool=new ThreadPool(m_mainLoop,threadNum);
    setListen();
}



void TcpServer::setListen()
{
    m_lfd=socket(AF_INET,SOCK_STREAM,0);
    if (m_lfd == -1)
    {
        perror("socket");
        return;
    }
    int opt = 1;
    int ret = setsockopt(m_lfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof opt);
        if (ret == -1)
    {
        perror("setsockopt");
        return;
    }
    struct sockaddr_in addr;
    addr.sin_family=AF_INET;
    addr.sin_port=htons(m_port);
    addr.sin_addr.s_addr=INADDR_ANY;
    ret=bind(m_lfd,(struct sockaddr*)&addr,sizeof(addr));
    if (ret==-1)
    {
        perror("bind");
        return;
    }
    ret=listen(m_lfd,128);
    if (ret == -1)
    {
        perror("listen");
        return ;
    }


}


void TcpServer::run()
{
    m_threadPool->run();
    Channel* channel=new Channel(m_lfd,FDEvent::ReadEvent,acceptConnection,nullptr,nullptr,this);
    m_mainLoop->AddTask(channel,ElemType::ADD);
    m_mainLoop->Run();
}

int TcpServer::acceptConnection(void* arg)
{
    TcpServer* server=static_cast<TcpServer*>(arg);
    int cfd=accept(server->m_lfd,NULL,NULL);
    EventLoop* evloop=server->m_threadPool->takeWorkerEventLoop();
    new TcpConnection(cfd,evloop);
    return 0;
}

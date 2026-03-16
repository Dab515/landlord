#include "HttpResponse.h"

HttpResponse::HttpResponse()
{
    m_statusCode=StatusCode::Unknown;
    m_fileName=string();
    m_headers.clear();
    sendDataFunc=nullptr;
}
HttpResponse::~HttpResponse(){
    
}
void HttpResponse::addHeader(const string key,const string value)
{
    if (key.empty()||value.empty())
    {
        return;
    }
    m_headers.insert(make_pair(key,value));
}

void HttpResponse::prepareMsg(Buffer* sendBuffer,int socket)
{
    char tmp[1024]={0};
    //添加请求行
    int code=static_cast<int>(m_statusCode);
    
    // 安全地获取状态描述
    const char* statusMsg = "Unknown";
    auto it = m_info.find(code);
    if (it != m_info.end()) {
        statusMsg = it->second.c_str();
    }
    sprintf(tmp,"HTTP/1.1 %d %s\r\n", code, statusMsg);

    //bufferAppendString(sendBuffer,tmp);
    sendBuffer->appendString(tmp);
    //添加请求头
    for (auto it=m_headers.begin();it!=m_headers.end();++it)
    {
        sprintf(tmp,"%s: %s\r\n",it->first.data(),it->second.data());
        //bufferAppendString(sendBuffer,tmp);
        sendBuffer->appendString(tmp);
    }
    //空行
    //bufferAppendString(sendBuffer,"\r\n");
    sendBuffer->appendString("\r\n");
#ifndef MSG_SEND_AUTO
    //bufferSendDate(sendBuffer,socket);
    sendBuffer->sendData(socket);
#endif
    //回复的数据
    sendDataFunc(m_fileName,sendBuffer,socket);

}

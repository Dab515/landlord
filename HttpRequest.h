#pragma once
#include "Buffer.h"
#include <stdbool.h>
#include "HttpResponse.h"
#include <map>
#include <string>
#include <functional>//有一个不一样
using namespace std;
enum class ProcessState:char
{
    ParseReqLine,
    ParseReqHeaders,
    ParseReqBody,
    ParseReqDone
};

class HttpRequest
{
private:   
    string m_method;
    string m_url;
    string m_version;   
    map<string,string> m_reqHeaders;
    ProcessState m_curState;
    char* splitRequestLine(const char* start,const char* end,const char* sub,function<void(string)> callback);
    int hexToDec(char c);

public:
    HttpRequest();
    ~HttpRequest();
    //重置
    void reSet();
    //获取处理状态
    inline ProcessState getState(){return m_curState;}
    //添加请求头
    void addHeader(const string key,const string value);
    //根据key得到请求头value
    string getHeader(const string key);
    //解析请求行
    bool paresRequestLine(Buffer* readBuffer);
    //解析请求头
    bool paresRequestHeader(Buffer* readBuffer);
    //解析http请求协议
    bool paresHttpRequest(Buffer* readBuffer,HttpResponse* response,Buffer* sendBuffer,int socket);
    //处理http请求协议
    bool processHttpRequest(HttpResponse* response);
    string decodeMsg(string from);
    static void sendFile(string filename,Buffer* sendBuffer,int cfd);
    static void sendDir(string dirName,Buffer* sendBuffer,int cfd);
    static void handleFileUpload(HttpRequest* req, Buffer* readBuffer, HttpResponse* response);
    static string getBoundary(const string& contentType);
    const string getFileType(const string name);
    inline void setMethod(string method)
    {
        m_method=method;
    }
    inline void seturl(string url)
    {
        m_url=url;
    }
    inline void setversion(string version)
    {
        m_version=version;
    }
    inline void setState(ProcessState state){m_curState=state;}
};


#include "HttpRequest.h"
#include <stdio.h>
#include <strings.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include "TcpConnection.h"
#include <assert.h>
#include <ctype.h>
#include <functional>
#include <fstream>
#include <sstream>
#include <errno.h>

// 简单的 URL 参数提取：从 url 的 query 中取 key 的值
static string getQueryParam(const string& url, const string& key)
{
    size_t qpos = url.find('?');
    if (qpos == string::npos) return "";
    string query = url.substr(qpos + 1);
    string pat = key + "=";
    size_t pos = query.find(pat);
    if (pos == string::npos) return "";
    pos += pat.size();
    size_t end = query.find('&', pos);
    return query.substr(pos, end == string::npos ? string::npos : end - pos);
}

using namespace std;

char* HttpRequest::splitRequestLine(const char* start,const char* end,const char* sub,function<void(string)> callback)
{
    char* space=const_cast<char*>(end);
    if (sub!=nullptr)
    {
        space=static_cast<char*>(memmem(start,end-start,sub,strlen(sub)));
        assert(space!=nullptr);
    }
    int length=space-start;
    callback(string(start,length));
    return space+1;
}

HttpRequest::HttpRequest()
{
    reSet();
}

HttpRequest::~HttpRequest()
{
    
}

void HttpRequest::reSet()
{
    m_curState=ProcessState::ParseReqLine;
    m_method=m_url=m_version=string();
    m_reqHeaders.clear();
}

void HttpRequest::addHeader(const string key,const string value)
{
    if (key.empty()||value.empty())
    {
        return;
    }
    m_reqHeaders.insert(make_pair(key,value));
}

string HttpRequest::getHeader(const string key)
{
    auto it=m_reqHeaders.find(key);
    if (it==m_reqHeaders.end())
    {
        return string();
    }
    return it->second;
}

bool HttpRequest::paresRequestLine(Buffer* readBuffer)
{
    //保存字符串的结束地址
    char* end=readBuffer->findCRLF();
    //保存字符串的起始地址
    char* start=readBuffer->data();
    //请求行的总长度
    int LineSize=end-start;

    if (LineSize>0)
    {
        auto methodFunc=bind(&HttpRequest::setMethod,this,placeholders::_1);
        start=splitRequestLine(start,end," ",methodFunc);
        auto urlFunc=bind(&HttpRequest::seturl,this,placeholders::_1);
        start=splitRequestLine(start,end," ",urlFunc);
        auto versionFunc=bind(&HttpRequest::setversion,this,placeholders::_1);
        splitRequestLine(start,end,NULL,versionFunc);
    
        // 为解析请求头做准备
        readBuffer->readPosIncrease(LineSize+2);
     
        // 修改状态
        //setstate(ProcessState::ParseReqHeaders);
        m_curState=ProcessState::ParseReqHeaders;
        return true;
    }
    return false;

}

bool HttpRequest::paresRequestHeader(Buffer* readBuffer)
{
    char* end=readBuffer->findCRLF();
    if (end!=nullptr)
    {
        char* start=readBuffer->data();
        int lineSize=end-start;
        //基于：搜索字符串
        char* middle=static_cast<char*>(memmem(start,lineSize,": ",2));
        if (middle!=nullptr)
        {
            int keyLen=middle-start;
            int valueLen=end-middle-2;
            if (keyLen>0&&valueLen>0)
            {
                string key(start,keyLen);
                string value(middle+2,valueLen);
                addHeader(key,value);
            }

            readBuffer->readPosIncrease(lineSize+2);

        }
        else
        {
            //请求头解析完了，跳过空行
            readBuffer->readPosIncrease(2);
            //对于GET请求
            //setstate(ProcessState::ParseReqDone);
            m_curState=ProcessState::ParseReqDone;
        }
        
        return true;
    }
    return false;
}

bool HttpRequest::paresHttpRequest(Buffer* readBuffer,HttpResponse* response,Buffer* sendBuffer,int socket)
{
    bool flag=true;
    while (m_curState != ProcessState::ParseReqDone)
    {
        switch (m_curState)
        {
            case ProcessState::ParseReqLine:
                flag = paresRequestLine(readBuffer);
                break;
            case ProcessState::ParseReqHeaders:
                flag = paresRequestHeader(readBuffer);
                // 如果是POST请求，解析完请求头后，需要进入解析请求体状态
                if (strcasecmp(m_method.c_str(), "post") == 0) {
                    m_curState = ProcessState::ParseReqBody;
                }
                break;
            case ProcessState::ParseReqBody:
            {
                // POST请求体处理：先确保 body 已经完整到达（至少按 Content-Length）
                // 你现在的实现是“假设请求体一次性读全”，这会导致：boundary 找不到/上传失败/偶发崩溃。
                string cl = getHeader("Content-Length");
                if (!cl.empty()) {
                    long need = strtol(cl.c_str(), nullptr, 10);
                    if (need > 0 && readBuffer->readAbleSize() < need) {
                        // body 还没收完整，等待下一次可读事件再解析
                        return true;
                    }
                }

                HttpRequest::handleFileUpload(this, readBuffer, response);
                m_curState = ProcessState::ParseReqDone;
                break;
            }
            default:
                break;
        }
        if (!flag)
        {
            return flag;
        }
        if (m_curState==ProcessState::ParseReqDone)
        {
            if (strcasecmp(m_method.c_str(), "get") == 0) {
                processHttpRequest(response);
            }
            response->prepareMsg(sendBuffer,socket);
        }

    }
    m_curState=ProcessState::ParseReqLine;
    return flag;
}

bool HttpRequest::processHttpRequest(HttpResponse* response)
{
    // 此函数现在主要处理GET请求
    if (strcasecmp(m_method.data(), "get") != 0)
    {
        // 非GET请求在此处不处理
        return false;
    }

    m_url = decodeMsg(m_url);

    // -------------------- 下载路由 --------------------
    // GET /download?file=xxx
    if (m_url.rfind("/download", 0) == 0)
    {
        string fileParam = getQueryParam(m_url, "file");
        if (fileParam.empty())
        {
            response->setStatusCode(StatusCode::BadRequest);
            response->addHeader("Content-Type", "text/plain; charset=utf-8");
            response->sendDataFunc = [](string, Buffer* sendBuffer, int cfd) {
                const char* msg = "bad request";
                sendBuffer->appendString(msg);
#ifndef MSG_SEND_AUTO
                sendBuffer->sendData(cfd);
#endif
            };
            return true;
        }

        // 简单防目录穿越（避免 ../）
        if (fileParam.find("..") != string::npos)
        {
            response->setStatusCode(StatusCode::BadRequest);
            response->addHeader("Content-Type", "text/plain; charset=utf-8");
            response->sendDataFunc = [](string, Buffer* sendBuffer, int cfd) {
                const char* msg = "invalid file";
                sendBuffer->appendString(msg);
#ifndef MSG_SEND_AUTO
                sendBuffer->sendData(cfd);
#endif
            };
            return true;
        }

        struct stat st;
        if (stat(fileParam.c_str(), &st) == -1 || !S_ISREG(st.st_mode))
        {
            response->setFileName("404.html");
            response->setStatusCode(StatusCode::NotFound);
            response->addHeader("Content-Type", getFileType(".html"));
            response->sendDataFunc = sendFile;
            return true;
        }

        response->setFileName(fileParam);
        response->setStatusCode(StatusCode::OK);
        // 强制下载
        response->addHeader("Content-Type", "application/octet-stream");
        response->addHeader("Content-Length", to_string(st.st_size));
        response->addHeader("Content-Disposition", string("attachment; filename=\"") + fileParam + "\"");
        response->sendDataFunc = sendFile;
        return true;
    }
    // -------------------------------------------------

    // 目录或文件
    const char* file = NULL;
    if (strcmp(m_url.data(), "/") == 0)
    {
        file = "./"; // 请求根目录时, 指向当前目录
    }
    else
    {
        // 去掉前导 '/'
        file = m_url.data() + 1;
    }

    struct stat st;
    int ret = stat(file, &st);
    if (ret == -1)
    {
        response->setFileName("404.html");
        response->setStatusCode(StatusCode::NotFound);
        response->addHeader("Content-Type", getFileType(".html"));
        response->sendDataFunc = sendFile;
        return true;
    }

    response->setStatusCode(StatusCode::OK);
    if (S_ISDIR(st.st_mode))
    {
        response->setFileName(file);
        response->addHeader("Content-Type", getFileType(".html"));
        response->sendDataFunc = sendDir;
    }
    else
    {
        response->setFileName(file);
        response->addHeader("Content-Type", getFileType(file));
        response->addHeader("Content-Length", to_string(st.st_size));
        response->sendDataFunc = sendFile;
    }
    return true;
}

// 新增：处理文件上传的函数
void HttpRequest::handleFileUpload(HttpRequest* req, Buffer* readBuffer, HttpResponse* response) {
    auto setFailed = [&] {
        response->setStatusCode(StatusCode::BadRequest);
        response->addHeader("Content-Type", "text/html; charset=utf-8");
        response->sendDataFunc = [](string, Buffer* sendBuffer, int socket) {
            const char* errorResponse = "<html><body><h1>上传失败!</h1><p>格式错误 / body 未收完整 / 文件名非法 / 无法写入。</p><a href=\"/\">返回首页</a></body></html>";
            sendBuffer->appendString(errorResponse);
#ifndef MSG_SEND_AUTO
            sendBuffer->sendData(socket);
#endif
        };
    };

    // 1) 取 boundary（Content-Type: multipart/form-data; boundary=xxxx）
    string contentType = req->getHeader("Content-Type");
    string b = HttpRequest::getBoundary(contentType);
    if (b.empty()) {
        setFailed();
        return;
    }
    string boundary = "--" + b;

    // 2) 在 readBuffer 中查找 boundary
    char* start = readBuffer->data();
    int dataLen = readBuffer->readAbleSize();
    char* end = start + dataLen;
    char* boundaryPos = static_cast<char*>(memmem(start, dataLen, boundary.c_str(), boundary.length()));
    if (!boundaryPos) {
        setFailed();
        return;
    }

    // 3) 查找文件名
    const char* filenameTag = "filename=\"";
    char* filenameStart = static_cast<char*>(memmem(boundaryPos, end - boundaryPos, filenameTag, strlen(filenameTag)));
    if (!filenameStart) {
        setFailed();
        return;
    }
    filenameStart += strlen(filenameTag);
    char* filenameEnd = static_cast<char*>(memmem(filenameStart, end - filenameStart, "\"", 1));
    if (!filenameEnd) {
        setFailed();
        return;
    }

    string filename(filenameStart, filenameEnd - filenameStart);
    if (filename.empty()) {
        setFailed();
        return;
    }

    // 最基础的安全过滤：不允许路径分隔符，防止上传覆盖任意路径
    if (filename.find('/') != string::npos || filename.find("..") != string::npos) {
        setFailed();
        return;
    }

    // 4) 找内容起始（\r\n\r\n）
    const char* contentStartTag = "\r\n\r\n";
    char* contentStart = static_cast<char*>(memmem(filenameEnd, end - filenameEnd, contentStartTag, strlen(contentStartTag)));
    if (!contentStart) {
        setFailed();
        return;
    }
    contentStart += strlen(contentStartTag);

    // 5) 找内容结束（\r\n--boundary）
    string endBoundaryStr = "\r\n" + boundary;
    char* contentEnd = static_cast<char*>(memmem(contentStart, end - contentStart, endBoundaryStr.c_str(), endBoundaryStr.length()));
    if (!contentEnd) {
        contentEnd = static_cast<char*>(memmem(contentStart, end - contentStart, boundary.c_str(), boundary.length()));
        if (!contentEnd) {
            setFailed();
            return;
        }
    }

    // 6) 写文件
    {
        ofstream outFile(filename, ios::binary);
        if (!outFile.is_open()) {
            setFailed();
            return;
        }
        outFile.write(contentStart, contentEnd - contentStart);
        if (!outFile.good()) {
            setFailed();
            return;
        }
    }

    // 7) 消耗掉本次 body（避免同一连接里重复解析）
    string cl = req->getHeader("Content-Length");
    if (!cl.empty()) {
        long need = strtol(cl.c_str(), nullptr, 10);
        if (need > 0 && need <= readBuffer->readAbleSize()) {
            readBuffer->readPosIncrease((int)need);
        }
    } else {
        readBuffer->readPosIncrease(readBuffer->readAbleSize());
    }

    response->setStatusCode(StatusCode::OK);
    response->addHeader("Content-Type", "text/html; charset=utf-8");
    response->sendDataFunc = [](string, Buffer* sendBuffer, int socket) {
        const char* successResponse = "<html><body><h1>上传成功!</h1><a href=\"/\">返回首页</a></body></html>";
        sendBuffer->appendString(successResponse);
#ifndef MSG_SEND_AUTO
        sendBuffer->sendData(socket);
#endif
    };
}

// 新增：从 Content-Type 中提取 boundary 的辅助函数
string HttpRequest::getBoundary(const string& contentType) {
    size_t pos = contentType.find("boundary=");
    if (pos == string::npos) {
        return "";
    }
    string b = contentType.substr(pos + 9);
    // 去掉可能的引号以及后续参数/空白
    if (!b.empty() && b.front() == '"') {
        size_t endq = b.find('"', 1);
        if (endq != string::npos) b = b.substr(1, endq - 1);
    }
    size_t sc = b.find(';');
    if (sc != string::npos) b = b.substr(0, sc);
    while (!b.empty() && isspace((unsigned char)b.back())) b.pop_back();
    while (!b.empty() && isspace((unsigned char)b.front())) b.erase(b.begin());
    return b;
}

// 将字符转换为整形数
int HttpRequest::hexToDec(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;

    return 0;
}
string HttpRequest::decodeMsg(string msg)
{
    string str=string();
    const char* from=msg.data();
    for (; *from != '\0'; ++from)
    {
        // isxdigit -> 判断字符是不是16进制格式, 取值在 0-f
        // Linux%E5%86%85%E6%A0%B8.jpg
        if (from[0] == '%' && isxdigit(from[1]) && isxdigit(from[2]))
        {
            // 将16进制的数 -> 十进制 将这个数值赋值给了字符 int -> char
            // B2 == 178
            // 将3个字符, 变成了一个字符, 这个字符就是原始数据
            str.append(1,hexToDec(from[1]) * 16 + hexToDec(from[2]));

            // 跳过 from[1] 和 from[2] 因此在当前循环中已经处理过了
            from += 2;
        }
        else
        {
            // 字符拷贝, 赋值
            str.append(1,*from);
        }

    }
    str.append(1, '\0');
    return str;

}

const string HttpRequest::getFileType(const string name)
{
    // a.jpg a.mp4 a.html
    // 自右向左查找‘.’字符, 如不存在返回NULL
    const char* dot = strrchr(name.data(), '.');
    if (dot == NULL)
        return "text/plain; charset=utf-8";	// 纯文本
    if (strcmp(dot, ".html") == 0 || strcmp(dot, ".htm") == 0)
        return "text/html; charset=utf-8";
    if (strcmp(dot, ".jpg") == 0 || strcmp(dot, ".jpeg") == 0)
        return "image/jpeg";
    if (strcmp(dot, ".gif") == 0)
        return "image/gif";
    if (strcmp(dot, ".png") == 0)
        return "image/png";
    if (strcmp(dot, ".css") == 0)
        return "text/css";
    if (strcmp(dot, ".au") == 0)
        return "audio/basic";
    if (strcmp(dot, ".wav") == 0)
        return "audio/wav";
    if (strcmp(dot, ".avi") == 0)
        return "video/x-msvideo";
    if (strcmp(dot, ".mov") == 0 || strcmp(dot, ".qt") == 0)
        return "video/quicktime";
    if (strcmp(dot, ".mpeg") == 0 || strcmp(dot, ".mpe") == 0)
        return "video/mpeg";
    if (strcmp(dot, ".vrml") == 0 || strcmp(dot, ".wrl") == 0)
        return "model/vrml";
    if (strcmp(dot, ".midi") == 0 || strcmp(dot, ".mid") == 0)
        return "audio/midi";
    if (strcmp(dot, ".mp3") == 0)
        return "audio/mpeg";
    if (strcmp(dot, ".ogg") == 0)
        return "application/ogg";
    if (strcmp(dot, ".pac") == 0)
        return "application/x-ns-proxy-autoconfig";

    return "text/plain; charset=utf-8";
}

void HttpRequest::sendDir(string dirName, Buffer* sendBuffer, int cfd)
{
    // 1. 读取 index.html 模板文件
    string templateContent;
    ifstream file("index.html");
    if (file.is_open()) {
        stringstream ss;
        ss << file.rdbuf();
        templateContent = ss.str();
        file.close();
    } else {
        // 如果模板文件不存在，可以发送一个简单的错误信息或者回退到旧的列表模式
        perror("open index.html template failed");
        // 这里简单地返回，不发送任何内容
        return;
    }

    // 2. 生成文件列表的HTML
    string fileListHtml;
    struct dirent** nameList;
    int num = scandir(dirName.c_str(), &nameList, NULL, alphasort);
    for (int i = 0; i < num; i++)
    {
        char* name = nameList[i]->d_name;
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
            free(nameList[i]);
            continue;
        }

        struct stat st;
        char subPath[1024] = {0};
        sprintf(subPath, "%s/%s", dirName.c_str(), name);
        stat(subPath, &st);

        char row[1024] = {0};
        if (S_ISDIR(st.st_mode))
        {
            // 目录：下载列实际为“进入”
            sprintf(row,
            "<tr><td class=\"col1\"><a href=\"%s/\">%s/</a></td><td class=\"col3\"><a href=\"%s/\">打开</a></td></tr>",
            name, name, name);
        }
        else
        {
            // 文件：下载走 /download 路由
            sprintf(row,
            "<tr><td class=\"col1\"><a href=\"%s\">%s</a></td><td class=\"col3\"><a href=\"/download?file=%s\">下载</a></td></tr>",
            name, name, name);
        }
        fileListHtml += row;
        free(nameList[i]);
    }
    free(nameList);

    // 3. 替换模板中的占位符
    const string placeholder = "<!--filelist_label-->";
    size_t pos = templateContent.find(placeholder);
    if (pos != string::npos) {
        templateContent.replace(pos, placeholder.length(), fileListHtml);
    }

    // 4. 发送最终的HTML内容
    sendBuffer->appendString(templateContent.c_str());
#ifndef MSG_SEND_AUTO
    sendBuffer->sendData(cfd);
#endif
}

void HttpRequest::sendFile(string filename,Buffer* sendBuffer,int cfd)
{
    //1、打开文件
    int fd=open(filename.data(),O_RDONLY);

    if (fd < 0)
    {
        perror("open");
        // 文件打不开（不存在/权限等），给一个最小的 404 body，避免服务进程被 assert 杀掉
        const char* body = "<html><body><h1>404 Not Found</h1></body></html>";
        sendBuffer->appendString(body);
#ifndef MSG_SEND_AUTO
        sendBuffer->sendData(cfd);
#endif
        return;
    }

#if 1
    while (1)
    {
        char buf[1024];
        int len=read(fd,buf,sizeof buf);
        if (len>0)
        {
            //send(cfd,buf,len,0);
            //bufferAppendDate(sendBuffer,buf,len);
            sendBuffer->appendString(buf,len);
#ifndef MSG_SEND_AUTO
            //bufferSendDate(sendBuffer,cfd);
            sendBuffer->sendData(cfd);
#endif            
            //usleep(10);
        }
        else if (len==0)
        {
            break;
        }
        else
        {
            close(fd);
            perror("read");
        }
        
    }
#else
    off_t offset = 0;
    int size = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);
    while (offset < size) {
        int ret = sendfile(cfd, fd, &offset, size - offset);
        if (ret == -1 && errno != EAGAIN) {
            perror("sendfile");
            close(fd);
            return -1;
        }
    }
#endif
    close(fd);
}

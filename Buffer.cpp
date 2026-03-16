//#define _GNU_SOURCE
#include "Buffer.h"
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <unistd.h>
Buffer::Buffer(int size):m_capacity(size)
{
    m_data=(char*)malloc(size);
    bzero(m_data,size);
}

Buffer::~Buffer()
{
    if (m_data!=nullptr)
    {
        free(m_data);
    }
    
    
}


void Buffer::extendRoom(int size)
{
    //1、内存够用--不需要扩容
    if (writeAbleSize()>=size)
    {
        return ;
    }
    
    //2、内存需要合并才行--不需要扩容
    //剩余的可写的内存+已读的内存>size
    else if (m_readPos+writeAbleSize()>=size)
    {
        int readable=readAbleSize();
        //
        memcpy(m_data,m_data+m_readPos,readable);
        m_readPos=0;
        m_writePos=readable;
    }
    
    //3、内存不够用--要扩容
    else
    {
        void* temp=realloc(m_data,m_capacity+size);
        if (temp==NULL)
        {
            return ;
        }
        memset((char*)temp+m_capacity,0,size);
        m_data=static_cast<char*>(temp);
        m_capacity+=size;
    }
}

int Buffer::appendString(const char* data,int size)
{
    if (data==nullptr||size<=0)   
    {
        return -1;
    }
    //扩容
    extendRoom(size);
    memcpy(m_data+m_writePos,data,size);
    m_writePos+=size;
    return 0;
}

int Buffer::appendString(const char* data)
{
    int len=strlen(data);
    return appendString(data,len);
}

int Buffer::socketRead(int fd)
{
    //使用readv函数接受数据
    struct iovec vec[2];
    // 初始化数组元素
    int writeable=writeAbleSize();
    vec[0].iov_base=m_data+m_writePos;
    vec[0].iov_len=writeable;
    char* tmpbuf=(char*)malloc(40960);
    vec[1].iov_base=tmpbuf;
    vec[1].iov_len=40960;
    int res=readv(fd,vec,2);
    if (res==-1)
    {
        return -1;
    }
    else if (res<=writeable)
    {
        m_writePos+=res;
    }
    else
    {
        m_writePos=m_capacity;
        appendString(tmpbuf,res-writeable);
    }
    free(tmpbuf);
    return res;
}

char* Buffer::findCRLF()
{
    
    char* ptr=(char*)memmem(m_data+m_readPos,readAbleSize(),"\r\n",2);
    return ptr;

}

int Buffer::sendData(int socket)
{
    int size=readAbleSize();
    if (size>0)
    {
        int count=send(socket,m_data+m_readPos,size,MSG_NOSIGNAL);
        if (count>0)
        {
            m_readPos+=count;
            usleep(1);
        }
        return count;
    }
    return 0;
}

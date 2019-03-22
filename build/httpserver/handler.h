#ifndef __HANDLER_H__
#define __HANDLER_H__

#include <unistd.h>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>
#include <unordered_map>
#include <iostream>

#include "resourse.hpp"
#include "code.h"
#include "connect.h"


class Handler
{
public:
    Handler(int sock)
        :cont_(sock)
        ,cgi_(false)
    {}
    //处理cgi请求
    bool HandlerCgi();
    //处理cgi程序返回的结果
    void ProcessCgiFollow(int fd);
    
    //通过code构建回复报文并回复
    void ProcessReplay();
   

    //对于非cgi直接处理并且发送
    //对于cgi，启动cgi并发送参数，然后添加到s_pepoll中，等待cgi程序处理完结果返回到程序
    //重新加入任务队列处理
    bool ReadAndParse();


    //用于事件回调
    static void ReadAndDecode(Handler * handler, int fd)
    {
        handler = new Handler(fd);
        if(handler->ReadAndParse())
            delete handler;        
    }
    static void EncodeAndsend(Handler * handler, int fd)
    {
        handler->ProcessCgiFollow(fd);
        delete handler;
    }


private: 
    bool cgi_;
    Resourse res_;
    Request req_;
    Replay rep_;
    Connect cont_;
    pid_t pid_;
};


#endif


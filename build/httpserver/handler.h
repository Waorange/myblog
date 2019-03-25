#ifndef __HANDLER_H__
#define __HANDLER_H__

#include <unistd.h>
#include <string>
#include <sys/timerfd.h>
#include <time.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unordered_map>
#include <iostream>

#include "../common/log.h"
#include "resourse.hpp"
#include "code.h"
#include "connect.h"

#define OVERTIME 10
class Singleton; 
class Event;


class Handler
{
public:
    Handler(Event * event, int sock)
        :cont_(sock)
        ,event_(event)
        ,cgi_(false)
    {}
    ~Handler()
    {
        std::cout << "***************" << std::endl;
    }
    //处理cgi请求
    bool HandlerCgi();
    //处理cgi程序返回的结果
    void ProcessCgiFollow(int pipe);
    
    //通过code构建回复报文并回复
    void ProcessReplay();
   

    //对于非cgi直接处理并且发送
    //对于cgi，启动cgi并发送参数，然后添加到s_pepoll中，等待cgi程序处理完结果返回到程序
    //重新加入任务队列处理
    bool ReadAndParse();


    //用于事件回调
    static void ReadAndDecode(std::shared_ptr<Handler> &handler, \
            int fd, Event * event)
    {

        handler = std::shared_ptr<Handler>(new Handler(event, fd));
        handler->ReadAndParse();

    }
    static void EncodeAndsend(std::shared_ptr<Handler> &handler, \
            int fd, Event * event)
    {
        handler->ProcessCgiFollow(fd);
    }
    //设置定时器
    static void StartTimer(int timefd)
    {
        std::cout <<"重启定时器: "<< timefd <<std::endl;
        struct itimerspec now_value;
        struct timespec now;
        //获取当前时间
        if(clock_gettime(CLOCK_REALTIME, &now) == -1)
        {
            LOG(WARNING, "clock_gettime error");
            return;
        }
        now_value.it_value.tv_sec = now.tv_sec + OVERTIME;
        now_value.it_value.tv_nsec = now.tv_nsec;
        now_value.it_interval.tv_sec = OVERTIME;
        now_value.it_interval.tv_nsec = 0;
        timerfd_settime(timefd, TFD_TIMER_ABSTIME, &now_value, nullptr);
    }


private: 
    bool cgi_;
    Event * event_;
    Resourse res_;
    Request req_;
    Replay rep_;
    Connect cont_;
    pid_t pid_;
};


#endif


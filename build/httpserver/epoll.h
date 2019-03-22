#ifndef __EPOLL_H__
#define __EPOLL_H__
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unordered_map>

#include "handler.h"

#define EVENT_NUM 10000

//事件

class Event
{
typedef void (*EventHandler)(Handler * handler, int fd);
public:
    enum FdType
    {
        LISTEN_FD,
        TIME_FD,
        CLIENT_FD,
        PIPE_FD
    };

    Event()
    :fd_(0)
        ,handler_(NULL)
        ,evhandler_(NULL)
        ,fd_type_(LISTEN_FD)
    {
    }
    Event(int fd, EventHandler evhandler, FdType fd_type,
            Handler * handler = NULL)
        :fd_(fd)
        ,handler_(handler)
        ,evhandler_(evhandler)
        ,fd_type_(fd_type)
    {}
    void run()
    {
        evhandler_(handler_, fd_);
    }
    int GetFd()
    {
        return fd_;
    }

    int fd_;
    Handler * handler_;
    EventHandler evhandler_;
    FdType fd_type_;
};

class Epoll
{
public:

    Epoll()
    {
        epollfd_ = epoll_create(EVENT_NUM);
    }
    ~Epoll()
    {
        close(epollfd_);
    }
    bool EventAdd(int fd, int events, Event * pevent);
    bool EventDel(int fd);
    bool EventMod(int fd, int events, Event * pevent);
    int EpollWait(struct epoll_event *events, int size);
    void SetNoBlock(int sfd);
private:
    int epollfd_;
};
//使用饿汉单例模式启动epoll
class Singleton
{
    public:
    static Epoll * GetEpoll()
    {
        return epoll;
    }
private:
    Singleton() = delete;
    Singleton(Singleton const &) = delete;
    Singleton & operator=(Singleton const&) = delete;
private:
    static Epoll * epoll;
};



#endif

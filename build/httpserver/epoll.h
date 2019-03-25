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
#include <memory>

#include "handler.h"

#define EVENT_NUM 10000

//事件

class Event
{
public:
    enum FdType
    {
        LISTEN_FD,
        TIME_FD,
        CLIENT_FD,
        PIPE_FD
    };

    Event(int fd = 0, FdType fd_type = LISTEN_FD)
        :fd_(fd)
        ,fd_type_(fd_type)
    {}
    virtual void run()
    {
        ;
    }
    int fd_;
    FdType fd_type_;
};

typedef void (*EventHandler)(int fd, Event * pevent);
class EventTime:public Event
{
public:
    EventTime(int fd, Event * event)
        : Event(fd, TIME_FD)
        , event_(event)
    {}
    Event * event_;
};

class EventClinetOrPipe:public Event
{
public:
    EventClinetOrPipe(int fd, EventHandler evhandler, FdType fd_type, \
            Handler* handler, int timefd = 0)
        :Event(fd, fd_type)
        ,handler_(handler)
        ,timefd_(timefd)
        ,evhandler_(evhandler)
    {}
    ~EventClinetOrPipe()
    {
        if(handler_ != nullptr)
            delete handler_;
    }   

    void run()
    {
        Event * pevent = const_cast<EventClinetOrPipe *>(this);
        evhandler_(fd_, pevent);
    }
    int GetFd()
    {
        return fd_;
    }

    int timefd_;
    Handler * handler_;
    EventHandler evhandler_;
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

//  class EventClinetOrPipe:public Event
//  {
//  public:

//  Event()
//      :fd_(0)
//      ,timefd_(0)
//      ,handler_(nullptr)
//      ,evhandler_(nullptr)
//      ,fd_type_(LISTEN_FD)
//  {}

//  Event(int fd, EventHandler evhandler, FdType fd_type,
//          int timefd = 0)
//      :fd_(fd)
//      ,timefd_(timefd)
//      ,handler_(nullptr)
//      ,evhandler_(evhandler)
//      ,fd_type_(fd_type)
//  {}
//  Event(int fd, EventHandler evhandler, FdType fd_type,
//          Handler* handler)
//      :fd_(fd)
//      ,timefd_(0)
//      ,handler_(handler)
//      ,evhandler_(evhandler)
//      ,fd_type_(fd_type)
//  {
//      std::cout << "PIPE处理" <<std::endl;
//  }
//  ~Event()
//  {
//      if(handler_ != nullptr)
//          delete handler_;
//  }   

//  void run()
//  {
//      evhandler_(fd_, const_cast<Event *>(this));
//  }
//  int GetFd()
//  {
//      return fd_;
//  }

//  int fd_;
//  int timefd_;
//  Handler * handler_;
//  EventHandler evhandler_;
//  };




#endif

#include "epoll.h"
#include "../common/log.h"


//事件
bool Epoll::EventAdd(int fd, int events, Event * pevent)
{
    struct epoll_event event;
    event.events = events;
    event.data.ptr = static_cast<void *>(pevent);

    if(epoll_ctl(epollfd_, EPOLL_CTL_ADD, fd, &event) < 0){
        return false;
    }
    return true;
}
bool Epoll::EventDel(int fd)
{
    if(epoll_ctl(epollfd_, EPOLL_CTL_DEL, fd, NULL) < 0){
        return false;
    }
    return true;
}
bool Epoll::EventMod(int fd, int events, Event * pevent)
{
    struct epoll_event event;
    event.events = events;
    event.data.ptr = static_cast<void *>(pevent);
    if(epoll_ctl(epollfd_, EPOLL_CTL_MOD, fd, &event) < 0)
    {
        return false;
    }    
    return true;
}
void Epoll::SetNoBlock(int sfd)
{
    int flags, s;
    flags = fcntl(sfd, F_GETFL, 0);
    if(flags == -1){
        LOG(ERROR, "setnoblock error");
    }

    flags |= O_NONBLOCK;
    if(fcntl(sfd, F_SETFL, flags)){
        LOG(ERROR, "setnoblock error");
    }
}
int Epoll::EpollWait(struct epoll_event *events, int size)
{
    int event_num = epoll_wait(epollfd_, events, size, -1);

    if(event_num < 0)
    {
        LOG(WARNING, "epoll wait error");    
    }
    return event_num;
}


Epoll * Singleton::epoll = new Epoll;


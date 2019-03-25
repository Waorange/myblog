#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/timerfd.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include "epoll.h"
#include "../common/log.h"
#include "thread_pool.hpp"

#define EVENT_NUM 10000
//#define OVERTIME 10        //超时时间60s 

class Singleton;

void SetTime();

class HttpServer
{
public:
    HttpServer(int port)
        : port_(port)
        , listen_sock_(0)
        , tp(nullptr)
    {}
    void InitServer()
    {
        //创建监听套接字
        listen_sock_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if(listen_sock_ == -1)
        {
            LOG(FATAL, "listen_sock_ create error");
            exit(1);
        }
        int opt_ = 1;
        setsockopt(listen_sock_, SOL_SOCKET, SO_REUSEADDR, &opt_, sizeof(opt_));
        
        //服务器地址信息
        struct sockaddr_in server_addr_;
        server_addr_.sin_family = AF_INET;
        server_addr_.sin_port = htons(port_);
        server_addr_.sin_addr.s_addr = INADDR_ANY;

        if(bind(listen_sock_, (struct sockaddr *)&server_addr_, sizeof(server_addr_)) < 0)
        {
            LOG(FATAL, "bind socket error");
            exit(1);
        }
        
        if(listen(listen_sock_, 5) < 0)
        {
            LOG(FATAL, "listen socket error");
            exit(1);
        }
        Event * pevent = new Event(listen_sock_);
        if(!Singleton::GetEpoll()->EventAdd(listen_sock_, \
                    EPOLLIN | EPOLLET, pevent))
        {
            LOG(FATAL, "listen socket error");
            exit(1);
        }
        //初始化线程池
        tp = new ThreadPool();
        tp->InitThreadPool();

        LOG(INFO, "InitServer success");
    }
    void run()
    {
        LOG(INFO, "start server");
        struct epoll_event events[EVENT_NUM];
        for(;;)
        {

            int event_num = Singleton::GetEpoll()->EpollWait\
                            (events, EVENT_NUM);
            for(int i = 0; i < event_num; ++i)
            {
                if (static_cast<Event *>(events[i].data.ptr)->fd_type_ \
                        == Event::FdType::LISTEN_FD)
                {
                    struct sockaddr_in client_addr;
                    socklen_t len = sizeof(client_addr);
                    int client_sock = accept(listen_sock_, \
                            (struct sockaddr*)&client_addr, &len);
                    if(client_sock < 0)
                    {
                        LOG(WARNING, "accept error");
                    }
                    else
                    {
                        LOG(INFO, "new accept");
                        ConnectEventHandler(client_sock);
                    }
                }
                else if(static_cast<Event *>(events[i].data.ptr)->fd_type_\
                        == Event::FdType::CLIENT_FD)
                {
                    ClientEventHandler(&events[i].data);
                } 
                else if(static_cast<Event *>(events[i].data.ptr)->fd_type_\
                        == Event::FdType::PIPE_FD)
                {
                    PipeEventHandler(&events[i].data);
                }
                else if(static_cast<Event *>(events[i].data.ptr)->fd_type_\
                        == Event::FdType::TIME_FD)
                {
                    OvertimeEventHandler(&events[i].data);
                }
            }
        }
    }
    void ClientEventHandler(epoll_data_t * user_data)
    {
        Event * pevent = static_cast<Event *>(user_data->ptr);
        Singleton::GetEpoll()->EventDel(pevent->fd_);

        //关闭定时器
        struct itimerspec zero_value;
        zero_value.it_value.tv_sec = 0;
        zero_value.it_value.tv_nsec = 0;
        zero_value.it_interval.tv_sec = 0;
        zero_value.it_interval.tv_nsec = 0;

        EventClinetOrPipe *pevent_client = \
                dynamic_cast<EventClinetOrPipe*>(pevent);
        timerfd_settime(pevent_client->timefd_, \
                TFD_TIMER_ABSTIME, &zero_value, nullptr);
        std::cout << "关闭定时器: " << pevent_client->timefd_<<std::endl;
        tp->EventPush(pevent);
    }
    void PipeEventHandler(epoll_data_t * user_data)
    {
        Event * event = static_cast<Event *>(user_data->ptr);
        tp->EventPush(event);
    }

    void OvertimeEventHandler(epoll_data_t * user_data)
    {
        Event * pevent = static_cast<Event *>(user_data->ptr);

        EventTime *pevent_time = dynamic_cast<EventTime *>(pevent);
        EventClinetOrPipe *pevent_client = \
            dynamic_cast<EventClinetOrPipe*>(pevent_time->event_);


        Singleton::GetEpoll()->EventDel(pevent_client->fd_);
        Singleton::GetEpoll()->EventDel(pevent_client->timefd_);
        std::cout << "超时关闭连接: " << pevent_client->fd_<<std::endl;
        close(pevent_client->fd_);
        close(pevent_client->timefd_);

        delete pevent_client;
        delete pevent_time;
    }
    void ConnectEventHandler(int client_sock)
    {
        //Singleton::GetEpoll()->SetNoBlock(client_sock);
        int timefd = timerfd_create(CLOCK_REALTIME, 0);
        if(timefd < 0)
        {
            LOG(WARNING, "timerfd_create error");
            return;
        }
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

        Event * pevent_client = new EventClinetOrPipe(client_sock, \
                &Handler::ReadAndDecode, Event::FdType::CLIENT_FD, \
                nullptr, timefd);
        Singleton::GetEpoll()->EventAdd(client_sock, EPOLLIN | EPOLLET, \
                pevent_client);

        //设置定时器当定时器超时时关闭连接
        //如果客户端发送请求则重置定时器
        timerfd_settime(timefd, TFD_TIMER_ABSTIME, &now_value, nullptr);
        std::cout <<"设置定时器: " << timefd<<std::endl;

        Event * pevent_time = new EventTime(timefd, pevent_client);
        Singleton::GetEpoll()->EventAdd(timefd, EPOLLIN | EPOLLET, \
                pevent_time);
    }

    ~HttpServer()
    {
        if(listen_sock_ > 0)
            close(listen_sock_);
        delete tp;
    }
private:
    int port_;
    int listen_sock_;
    ThreadPool * tp;
};


int main(int argc, char * argv[])
{
    if(argc != 2)
    {
        std::cout << "Usage: http_server <port>"<<std::endl;
        exit(1);
    }
    //忽略SIGPIPE信号防止对方提前关闭连接导致服务器被关闭
    signal(SIGPIPE, SIG_IGN);

    //初始化http服务器
    HttpServer * server = new HttpServer(atoi(argv[1]));
    server->InitServer();

    //启动http服务器
    server->run();    
    
    return 0;
}

#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <pthread.h>

#include "epoll.h"
#include "../common/log.h"
#include "thread_pool.hpp"

#define EVENT_NUM 10000

class Singleton;

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
        Event * pevent = new Event(listen_sock_, nullptr, \
                Event::FdType::LISTEN_FD);
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
                else 
                {
                    EventHandler(&events[i].data);
                }
            }
        }
    }
    void EventHandler(epoll_data_t * user_data)
    {
        tp->EventPush(static_cast<Event *>(user_data->ptr));
    }

    void OvertimeEventHandler(int timefd)
    {
        std::cout << "OvertimeEventHandler" <<std::endl;
    }
    void ConnectEventHandler(int client_sock)
    {
        //Singleton::GetEpoll()->SetNoBlock(client_sock);
        Event * pevent = new Event(client_sock, \
                &Handler::ReadAndDecode, Event::FdType::CLIENT_FD);
        Singleton::GetEpoll()->EventAdd(client_sock, EPOLLIN | EPOLLET, pevent);
        //TODO
        //设置时钟
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

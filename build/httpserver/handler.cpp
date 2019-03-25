#include "epoll.h"
#include "../common/log.h"
#include "handler.h"


//处理cgi请求
bool Handler::HandlerCgi()
{
    const std::string & param_ = req_.GetReqParam();

    //输入管道父进程给子进程传递参数
    int fd_input[2];
    //输出管道子进程给父进程传递信息
    int fd_output[2];

    try
    {
        if(pipe(fd_input) < 0 || pipe(fd_output) < 0)
        {
            LOG(WARNING, "create pipe error");
            throw "error";
        }
        pid_t pid = fork();
        if(pid < 0)
        {
            LOG(WARNING, "fock error");
            throw "error";
        }
        else if (pid == 0)
        {
            close(fd_input[1]);
            close(fd_output[0]);

            dup2(fd_input[0], 0);
            dup2(fd_output[1], 1);

            //提供参数的长度
            //Content-Length
            std::string cl_env_="Content-Length=";
            cl_env_ += Replay::IntToString(param_.size());    
            putenv((char *)cl_env_.c_str());

            //const std::string path_ = res_.GetPath();

            std::string path_ = res_.GetPath();
            std::string suffix_ = path_.substr(path_.rfind('.') + 1);
            //std::cout << path_ << " "<<suffix_<< std::endl;
            if(suffix_ == "py")
            {
                execlp("python3", "python3", path_.c_str(), NULL); 
            }
            else
            {
                execl(path_.c_str(), path_.c_str(), NULL);
            }

            LOG(WARNING, "execl error");
            exit(1);  
        }
        else
        {
            close(fd_input[0]);
            close(fd_output[1]);

            size_t size_ = param_.size();
            for(int i = 0; i < size_; ++i)
            {
                if(write(fd_input[1], &param_[i], 1) < 0)
                {
                    LOG(WARNING, "write error");
                    waitpid(pid, NULL, 0);
                    throw "error";
                }
            }
            close(fd_input[1]);
            pid_ = pid;

            Event * pevent = new Event(fd_output[0], &EncodeAndsend, \
                    Event::PIPE_FD, event_->handler_);
            Singleton::GetEpoll()->EventAdd(fd_output[0], EPOLLIN, pevent);

            //添加到事件池;

        }
    }
    catch(...) 
    { 
        if(fd_input[0] > 0)
        {
            close(fd_input[0]);
            close(fd_input[1]);
        }
        if(fd_output[0] > 0)
        {
            close(fd_output[0]);
            close(fd_output[1]);
        }
        rep_.SetCode() = 500;
        return false;
    }
}


//处理cgi程序返回的结果
void Handler::ProcessCgiFollow(int pipe)
{
    char ch_;
    while(read(pipe, &ch_, 1) > 0)
    {
        rep_.MakeReplayText().push_back(ch_);
    }

    close(pipe);
    waitpid(pid_, NULL, 0);
    //释放掉管道事件
    ProcessReplay();
    
    LOG(INFO, "Request handler finish");
}


//通过code构建回复报文并回复
void Handler::ProcessReplay()
{
    switch(rep_.GetCode())
    {
        case 400:
            break;
        case 404:
            res_.SetPath() = "httproot/404.html";
            res_.IsPathLegal(cgi_);
            break;
        case 500:
            break;
        default: break;
    }
    rep_.MakeStatusLine();
    rep_.MakeReplayHand(cgi_, res_);
    rep_.MakeReplayBlank();

    cont_.SendReplay(cgi_, rep_, res_);

    Singleton::GetEpoll()->EventAdd(cont_.GetSock(), EPOLLIN | EPOLLET, event_);
    StartTimer(event_->timefd_);

    std::cout << "user_count: " << event_->handler_.use_count()<<std::endl;
}


//对于非cgi直接处理并且发送
//对于cgi，启动cgi并发送参数，然后添加到s_pepoll中，等待cgi程序处理完结果返回到程序
//重新加入任务队列处理
bool Handler::ReadAndParse()
{

    std::cout << "user_count: " << event_->handler_.use_count()<<std::endl;
    LOG(INFO, "start handler ...");  
    int code_ = 0;
    try
    {
        cont_.ReadRequestLine(req_.SetReqLine());
        req_.RequestLineParse();

        //请求语法错误Bad Request
        if(!req_.IsMathodLegal(cgi_))
        {
            code_ = 400;
            throw "error";
        }

        req_.RequestUriParse(res_.SetPath());

        //找不到资源Not Found
        if(!res_.IsPathLegal(cgi_))
        {
            code_ = 404;
            cont_.ReadRequestHead(req_.SetReqHead());
            throw "error";
        }

        const std::string massage = res_.GetPath();
        LOG(INFO, massage);

        if(!cont_.ReadRequestHead(req_.SetReqHead()))
        {
            code_ = 400;
            throw "error";
        }
        req_.RequestHeadParse();

        if(req_.IsHaveText())
        {
            req_.GetContentLength(cont_.SetContentLength());
            cont_.ReadRequestText(req_.SetReqParam());
        } 
        req_.JudgeCode(rep_.SetCode());

        if(cgi_){
            HandlerCgi();
            return false;
        }
        else
        {
            ProcessReplay();
            LOG(INFO, "Request handler finish");
            return true;
        }  
    }
    catch(...)
    {
        rep_.SetCode() = code_;

        ProcessReplay();
        LOG(INFO, "Error Handler finish"); 
        return true;
    }
}



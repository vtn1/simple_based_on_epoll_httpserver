#include<sys/socket.h>//socket,connect,bind,listen accept
#include<stdio.h>
#include<netinet/in.h>//sockaddr_in
#include<arpa/inet.h>//ttonl
#include<stdlib.h>//bzero();
#include<sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdbool.h>

#include <sys/errno.h>
#include "simplehttp.h"
#define minb  256
int listenfd;
/*
typedef  struct {
    int epfd;
    int curr_fd;
};
*/
void setnoblocking(int fd)
{
    int opts=0;
    opts=fcntl(fd,F_GETFL);
    if(opts<0)
    {
        perror("fcntl(fd,F_GETFL) :");
        return;
    }
    opts=opts|O_NONBLOCK;
    if(fcntl(fd,F_SETFL,opts)<0)
    {
        perror("fcntl(fd,F_SETFL,opts)");
        return;
    }
}
/*void close_sock_fd(int epfd,Conn *conn,struct epoll_event event)
{

    //event.data.ptr=NULL;
    epoll_ctl(epfd,EPOLL_CTL_DEL,conn->fd,0);

    close(conn->fd);
    //free(event);
    free(conn);
}*/


void do_events(void *arg){
    int epfd=*((int *)(arg));
    int nfds;
    struct epoll_event events[100];
    while(1)
    {
        nfds=epoll_wait(epfd,events,300,-1);
       // printf("nfds:%d\n",nfds);
        for(int i=0;i<nfds;++i)
        {
            Conn  *conn=(Conn *)events[i].data.ptr;


            if(events[i].events&EPOLLIN)
            {
                http_read(conn);
                mod_epoll_event(epfd,conn,EPOLLOUT|EPOLLET);
            }

            else if(events[i].events&EPOLLOUT)
            {
                http_write(conn);
                //printf("conn_write function:%s",*conn->conn_write);
                del_epoll_event_close_fd(epfd,conn);
            }
            //else  del_epoll_event_close_fd(epfd,conn);
            //del_epoll_event_close_fd(epfd,conn);
        }



    }
}

int main(int argc ,char *argv[])
{
    int connectfd,backlog;
    int epfd[4];
    int port;
    port=18080;
    int suma;
    int sumr;
    int sumw;
    sumr=sumw=suma=0;
    struct epoll_event ev;
    char buf[1024];
    backlog=5;
    struct sockaddr_in cliaddr;
    struct sockaddr_in serveraddr;
    ;// 加上错误处理
    if((listenfd=socket(AF_INET,SOCK_STREAM,0))<0)
    {
        perror("creat socket error");
        exit(-1);
    }
    printf("Init ok.... port:%d \n",port);
    bzero(&serveraddr,sizeof(serveraddr));
    serveraddr.sin_family=AF_INET;
    serveraddr.sin_addr.s_addr=htonl(INADDR_ANY);
    serveraddr.sin_port=htons(port);
    if(bind(listenfd,(struct sockaddr*)&serveraddr,sizeof(serveraddr))<0)
    {
        perror("bind error");
        exit(-1);
    }
    printf("web test\n");

    if(listen(listenfd,backlog)<0)
    {
        perror("listen error ");
        exit(-1);
    }


    socklen_t clilen=sizeof(cliaddr);
    for (int i = 0; i < 4; ++i) {
    epfd[i]=epoll_create(1024);
    }
    //Conn *webconn=(Conn *)malloc(sizeof(Conn));
    //webconn->fd=listenfd;
    //ev=(struct epoll_event *)malloc(sizeof( struct epoll_event));
    //ev.data.ptr=(void *)webconn;
    //ev.events=EPOLLIN;
    //epoll_ctl(epfd,EPOLL_CTL_ADD,listenfd,&ev);
    int nfds;
    pthread_t tid[4];
    for (int i = 0; i < 4; ++i) {
        pthread_create(&(tid[i]),NULL,do_events,&epfd[i]);
    }

    //listen_and_accept
    int k=0;
    while(1)
    {
        connectfd=accept(listenfd,NULL,NULL);
        if(connectfd<0)
        {
            perror("accept error:");
            return 0;
        }
        ++k;
        setnoblocking(connectfd);
        Conn *newwebconn=(Conn *)malloc(sizeof(Conn));
        newwebconn->conn_read=Simhttp_accept_quest;
        newwebconn->fd=connectfd;

        add_epoll_event(epfd[k-1],newwebconn,EPOLLIN|EPOLLET);
        if(k==4)
         k=0;
    }

    close(epfd);
    close (listenfd);

    return 0;

}


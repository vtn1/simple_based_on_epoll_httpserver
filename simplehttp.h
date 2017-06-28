
#include<sys/socket.h>//socket,connect,bind,listen accept
#include<stdio.h>
#include<netinet/in.h>//sockaddr_in
#include<arpa/inet.h>//ttonl
#include<stdlib.h>//bzero();
#include<string.h>//index,bzero(),strcasecmp(),strstr
#include<sys/stat.h>//struct stat,stat;
#include<fcntl.h>//open,read,fcntl
#include<pthread.h>
#include<sys/wait.h>
#include<signal.h>
#include<sys/epoll.h>
#define maxb 1024
#define minb 256

typedef  struct conn
{
	int fd;
	char uri[minb];
    char filename[minb];
	char version[20];
	char method[10];
	char query[minb];
	char content[maxb];
    int content_length;

	void *(*conn_read)(void *arg);
	void *(*conn_write)(void *arg);
} Conn;

void Simhttp_accept_quest(Conn *conn);
void Simhttp_discard_header(int fd);
int Simhttp_get_line(int sock,char *buf,int size);
int Simhttp_parse_uri(char * method,char *uri,char *filename,char * cgiargs);
void Simhttp_server_static(Conn* conn);
void Simhttp_send_header(int fd,char *filename);
void Simhttp_server_cgi(Conn* conn);
void Simhttp_method_unimplemented(Conn* conn);
void Simhttp_file_not_found(Conn* conn);
void Simhttp_false_request(Conn* conn);
void Simhttp_extract_body(Conn *conn);
void Simhttp_execute_error(Conn* conn);

void Simhttp_get_filetype(char * filename,char *filetype);

void mod_epoll_event(int epfd,Conn *conn, int flags)
{
    struct epoll_event ev;
    ev.events=flags;
    ev.data.ptr=(void *)conn;
    epoll_ctl(epfd,EPOLL_CTL_MOD,conn->fd,&ev);
}
void add_epoll_event(int epfd,Conn *conn,int flags)
{
    struct epoll_event ev;
    ev.events=flags;
    ev.data.ptr=(void *)conn;
    epoll_ctl(epfd,EPOLL_CTL_ADD,conn->fd,&ev);
}
void del_epoll_event_close_fd(int epfd,Conn *conn)
{
    epoll_ctl(epfd,EPOLL_CTL_DEL,conn->fd,0);
    close(conn->fd  );
    free(conn);
}
void http_read(Conn * conn)
{
    signal(SIGPIPE,SIG_IGN);
    (*(conn->conn_read))(conn);
    //Simhttp_accept_quest(conn);
    //printf("%x");

}
void http_write(Conn *conn)
{
    //printf("test conn  write \n");
    //signal(SIGPIPE,SIG_IGN);
    (*(conn->conn_write))(conn);
}
/*请求处理函数，先找出方法和是否是cgi 在获取文件信息，进入相应的处理函数 based on httpd*/
void Simhttp_accept_quest(Conn *conn)
{

    int fd=conn->fd;
    //printf("fd:%d\n",fd);

    char buf[maxb],method[minb],uri[minb],version[minb];//http request line
    char filename[minb],cgiargs[minb];
    int is_cgi;
    //

    //while(n==0)

    Simhttp_get_line(fd,buf,maxb);

    //printf("%s\n",buf);
    sscanf(buf,"%s %s %s ",method,uri,version);//提取方法和资源地址 版本
    if(strcasecmp(method,"GET")&&strcasecmp(method,"POST"))
    {//判断方
        Simhttp_discard_header(fd);
        conn->conn_write=Simhttp_method_unimplemented;
        //目前只写了get post方法有时间在加上

        //close(fd);
        return ;
    }

    is_cgi=Simhttp_parse_uri(method,uri,filename,cgiargs);
    //printf("is cgi ?%d\n",is_cgi);
    struct stat sbuf;
    if(stat(filename,&sbuf)<0)
    {
        Simhttp_discard_header(fd);
        conn->conn_write=Simhttp_file_not_found;

    }
    else
    {

        strcpy(conn->method,method);
        strcpy(conn->uri,uri);
        strcpy(conn->query,cgiargs);
        strcpy(conn->filename,filename);
        strcpy(conn->version,version);
        if(!is_cgi) {

            if (!(S_ISREG(sbuf.st_mode) || !(S_IRUSR & sbuf.st_mode)))//文件没有读取权限
            {
                conn->conn_write = Simhttp_file_not_found;
            } else {
                Simhttp_discard_header(fd);
                conn->conn_write = Simhttp_server_static;
            }//extrca	//
        }
        else

        {
            if(!(S_ISREG(sbuf.st_mode)||!(S_IRUSR&sbuf.st_mode)))//文件没有读取权限
            {
                conn->conn_write=Simhttp_execute_error;
            }
            else
            {
                Simhttp_extract_body(conn);
                conn->conn_write=Simhttp_server_cgi;
            }
        }
    }


//printf("signal has done the next%d \n",sum);
//close(fd);
    return ;
}
/* 从套接字读取一行数据 ，以\r\n为标志提取 最后以\n作为 结尾*/
int Simhttp_get_line(int sock ,char *buf,int size)
{
    int i=0;
    char c='\0';
    int n=0;
    while((i<size-1)&&(c!='\n'))
    {
        //printf(" get line \n");
        n=recv(sock,&c,1,0);
        if(n>0)
        {

            if(c=='\r')
            {
                n=recv(sock,&c,1,MSG_PEEK);
                if((n>0)&&(c=='\n')) recv(sock,&c,1,0);
                else c='\n';
            }
            buf[i]=c;
            i++;
        }else
        {c='\n';}
    }

    buf[i]='\0';
    return i;
}
//跳过头部数据
void Simhttp_discard_header(int fd)
{

    char buf[maxb];
    int n=1;
    n=Simhttp_get_line(fd,buf,maxb);
    while(strcmp(buf,"\n")!=0&&n>0)
    {
        //printf("%s \n","header discard");
        n=Simhttp_get_line(fd,buf,maxb);

        //printf("%s",buf);
    }
    return ;
}
//提取filename和参数（如果有的话）
int Simhttp_parse_uri(char * method,char * uri,char *filename,char *cgiargs)
{

    int cgi=0;
    char *ptr;
//printf("start Simhttp_parse_uri\n");
    strcpy(cgiargs,"");
    if(strcasecmp(method, "GET") == 0)
    {
        ptr=index(uri,'?');//找出？后面的参数
        if(ptr)
        {
            strcpy(cgiargs,ptr+1);
            *ptr='\0';
            cgi=1;
        }
        else strcpy(cgiargs,"");


    }
    else if(strcasecmp(method, "POST") == 0)
    {
        cgi=1;

    }
    //default dir;
    sprintf(filename,"./testfile%s",uri);
    // printf("%s\n",uri);
    if(filename[strlen(filename)-1]=='/')//默认文件地址
    {
        strcat(filename,"default.html");
    }

    if(cgi==0)strcpy(cgiargs,"");
    // printf("%s %s\n",filename,cgiargs);
    return cgi;


}
//http响应 头部信息
void Simhttp_send_header(int fd,char *filename)
{
    //int fd=conn->fd;
    char buf[minb];
    char filetype[minb];
    get_filetype(filename,filetype);
    sprintf(buf,"HTTP/1.0 200 OK\r\n");
    sprintf(buf,"%sServer:server test\r\n",buf);
    //sprintf(buf,"%sContent-length:%d\r\n",buf,filesize);
    sprintf(buf,"%sContent-type:%s\r\n",buf,filetype);
    sprintf(buf,"%s\r\n",buf);

    send(fd,buf,strlen(buf),0);
}
//处理静态文件
void Simhttp_server_static(Conn *conn)
{
    int fd=conn->fd;
    char *filename=conn->filename;
    char * sfile,filetype[maxb],buf[maxb];
    FILE * filefd;
    int n=0;
    //跳过头部
    //Simhttp_send_header(fd,filename,filesize);
    //printf("static \n");

    filefd=fopen(filename,"r");
    if(filefd==NULL)
    {
        perror("file error;");
        Simhttp_file_not_found(fd);
        //return ;
        exit(0);
    }

    Simhttp_send_header(fd,filename);

    fgets(buf,sizeof(buf),filefd);

    while(!feof(filefd)&&n>=0)
    {
        n=send(fd,buf,strlen(buf),0);
        fgets(buf,sizeof(buf),filefd);
    }

    fclose(filefd);


}
void get_filetype(char * filename,char *filetype)
{
    if(strstr(filename,".html"))
        strcpy(filetype,".html");
    else if(strstr(filename,".gif"))
        strcpy(filetype,".gif");
    else if(strstr(filename,".jpg"))
        strcpy(filetype,".jpg");
    else strcpy(filetype,".text/plain");
}
/*based on tinyhttpd */
void Simhttp_file_not_found(Conn *conn)
{
    printf("not found\n");
    int clientfd=conn->fd;
    char buf[1024];

    sprintf(buf, "HTTP/1.0 404 NOT FOUND\r\n");
    send(clientfd, buf, strlen(buf), 0);
    sprintf(buf,"%sServer:server test\r\n",buf);
    send(clientfd, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(clientfd, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(clientfd, buf, strlen(buf), 0);
    sprintf(buf, "<HTML><TITLE>Not Found</TITLE>\r\n");
    send(clientfd, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>not found 404 \r\n");
    send(clientfd, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(clientfd, buf, strlen(buf), 0);
}


/*based on tinyhttpd*/
void Simhttp_false_request(Conn *conn)
{
    char buf[1024];
    int clientfd=conn->fd;
    sprintf(buf, "HTTP/1.0 400 BAD REQUEST\r\n");
    send(clientfd, buf, sizeof(buf), 0);
    sprintf(buf, "Content-type: text/html\r\n");
    send(clientfd, buf, sizeof(buf), 0);
    sprintf(buf, "\r\n");
    send(clientfd, buf, sizeof(buf), 0);
    sprintf(buf, "<P>Your browser sent a bad request, ");
    send(clientfd, buf, sizeof(buf), 0);
    sprintf(buf, "such as a POST without a Content-Length \r\n");
    send(clientfd, buf, sizeof(buf), 0);
}

void Simhttp_execute_error(Conn *conn)
{
    int clientfd=conn->fd;
    char buf[minb];
    sprintf(buf, "HTTP/1.0 500 Internal Server Error\r\n");
    send(clientfd, buf, strlen(buf), 0);
    sprintf(buf, "Content-type: text/html\r\n");
    send(clientfd, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(clientfd, buf, strlen(buf), 0);
    sprintf(buf, "<P>Error prohibited CGI execution.\r\n");
    send(clientfd, buf, strlen(buf), 0);
}

void Simhttp_method_unimplemented(Conn *conn)
{
    printf("uniplement \n");
    int client=conn->fd;
    char buf[1024];

    sprintf(buf, "HTTP/1.0 501 Method Not Implemented\r\n");
    sprintf(buf, "%sServer:server test\r\n",buf);
    sprintf(buf, "%Content-Type: text/html\r\n",buf);

    sprintf(buf, "%s\r\n",buf);

    sprintf(buf, "%s<HTML><HEAD><TITLE>Method Not Implemented\r\n",buf);

    sprintf(buf, "%s</TITLE></HEAD>\r\n",buf);

    sprintf(buf, "%s<BODY><P>HTTP request method not supported.\r\n",buf);

    sprintf(buf, "%s</BODY></HTML>\r\n",buf);
    send(client, buf, strlen(buf), 0);
}
//cgi 处理函数 参照httpd的思路写的
void Simhttp_extract_body(Conn *conn)
{
    int fd=conn->fd;
    char buf[maxb];
    int content_length;
    char c;
    if (strcasecmp(conn->method,"GET") == 0)
    {
        Simhttp_discard_header(fd);//跳过头部

    }else if(strcasecmp(conn->method,"POST")==0)
    {

        int nchars=Simhttp_get_line(fd,buf,minb);
        //printf("%s\n",buf);
        while((nchars>0)&&strcmp(buf,"\n"))
        {
            //printf("%s\n",buf);
            buf[15]='\0';//end id
            if(strcasecmp(buf,"Content-Length:")==0)
            {

                content_length=atoi(&buf[16]);

            }
            nchars=Simhttp_get_line(fd,buf,minb);

        }
        if(content_length==-1)
        {
            conn->conn_write= Simhttp_false_request;
            return ;
        }
        if(strcasecmp(conn->method,"POST")==0)
        {
            int i;
            for(i=0;i<content_length;i++)
            {
                recv(fd,&c,1,0);
                conn->content[i]=c;

            }
            conn->content_length=content_length;
        }
        //post extract the information
    }
}
void Simhttp_server_cgi(Conn *conn)
{
    int fd=conn->fd;
    char *method=conn->method;
    char * cgiargs=conn->query;
    int content_length=conn->content_length;
    char buf[minb];
    int input[2];
    int output[2];
    pid_t pid;
    int nchars;
    int status;
    char c;
    //buf[0]='a';buf[1]='\0';
    printf("%s\n",conn->filename);
    sprintf(buf,"HTTP/1.0 200 OK\r\n");
    sprintf(buf,"%sServer:My Web Server\r\n",buf);
    printf("%s.....%d\n",buf,content_length);
    send(fd,buf,strlen(buf),0);
    if(pipe(input)<0)
    {
        perror("pipe error:");
        Simhttp_execute_error(fd);
        //cgi error;
        return ;
    }
    if(pipe(output)<0)
    {

        perror("pipe error:"); //cgi error;
        Simhttp_execute_error(fd);
        return ;
    }
    if((pid=fork())<0)
    {
        Simhttp_execute_error(fd);
        return ;
    }
    if(pid==0)
    {
        dup2(input[0],0);
        dup2(output[1],1);
        close(input[1]);
        close(output[0]);
        char method_env[minb];
        char query_env[minb];
        char contlength_env[minb];
        sprintf(method_env, "REQUEST_METHOD=%s", method);
        putenv(method_env);
        if(strcasecmp(method,"GET")==0)
        {
            sprintf(query_env, "QUERY_STRING=%s",cgiargs);
            putenv(query_env);
        }
        else  if(strcasecmp(method,"POST")==0)
        {
            sprintf(contlength_env,"Content-Length=%d",content_length);
            putenv(contlength_env);
        }
        execl(conn->filename,conn->filename,NULL);
    }else
    {
        signal(SIGPIPE,SIG_IGN);
        close(input[0]);
        close(output[1]);
        if(strcasecmp(method,"POST")==0)
        {
            for(int i=0;i<content_length;i++)
            {
                //recv(fd,&c,1,0);
                c=conn->content[i];
                write(input[1],&c,1);
            }
        }
        while (read(output[0],&c,1) > 0)
            send(fd,&c,1,0);
        //printf("do all \n");//debug
        close(output[0]);
        close(input[1]);
        waitpid(pid,&status, 0);

    }

}
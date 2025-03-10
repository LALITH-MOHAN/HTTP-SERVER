#include<stdio.h>
#include<stdlib.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<netinet/in.h>
#include<sys/epoll.h>
#include<unistd.h>
#include<fcntl.h>

#define MAX 10

void set_nonblock(int fd)
{
    int flags=fcntl(fd,F_GETFL,0);
    fcntl(fd,F_SETFL,O_NONBLOCK|flags);
}

int main()
{
    int server_fd,client_fd;
    int epfd,nfds;
    struct epoll_event event,events[MAX];
    server_fd=socket(AF_INET,SOCK_STREAM,0);
    printf("SERVER_FD:%d\n",server_fd);

    struct sockaddr_in server_addr;
    server_addr.sin_family=AF_INET;
    server_addr.sin_port=htons(8080);
    server_addr.sin_addr.s_addr=INADDR_ANY;

    if(bind(server_fd,(struct sockaddr*)&server_addr,sizeof(server_addr))<0)
    {
        perror("BINDING");
        exit(EXIT_FAILURE);
    }
    if(listen(server_fd,MAX)<0)
    {
        perror("LISTENING");
        exit(EXIT_FAILURE);
    }
    set_nonblock(server_fd);
    epfd=epoll_create1(0);
    event.events=EPOLLIN;
    event.data.fd=server_fd;
    printf("EPFD:%d\n",epfd);
    if(epoll_ctl(epfd,EPOLL_CTL_ADD,server_fd,&event)<0)
    {
        perror("EPOLL_CTL");
        exit(EXIT_FAILURE);
    }
    printf("LISTENING...\n");
    while(1)
    {
        nfds=epoll_wait(epfd,events,MAX,-1);
        for(int i=0;i<nfds;i++)
        {
            if(events[i].data.fd==server_fd)
            {
                client_fd=accept(server_fd,NULL,NULL);
                set_nonblock(client_fd);
                event.events=EPOLLIN|EPOLLET;
                event.data.fd=client_fd;
                if(epoll_ctl(epfd,EPOLL_CTL_ADD,client_fd,&event)<0)
                {
                    perror("FAILED TO ADD");
                    continue;
                }
                printf("NEW CLIENT FD:%d\n",client_fd);
            }
            else
            {
                char buff[1024];
                int rec=recv(events[i].data.fd,buff,sizeof(buff)-1,0);
                if(rec<=0)
                {
                    printf("CLIENT DISCONNECTED\n");
                    close(events[i].data.fd);
                    epoll_ctl(epfd,EPOLL_CTL_DEL,events[i].data.fd,NULL);
                }
                else
                {
                    buff[rec]='\0';
                    printf("CLIENT MESSAGE: %s\n",buff);
                    send(events[i].data.fd,"RECEIVED",8,0);
                }
            }
        }
    }
    close(server_fd);
    close(epfd);
}
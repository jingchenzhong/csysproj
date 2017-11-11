//
// Created by root on 17-11-8.
//

/* server.c */

/**
#include <poll.h>
int poll(struct pollfd *fds, nfds_t nfds, int timeout);

struct pollfd {
    int fd;         // 文件描述符
    short events;   // 监控的事件
    short revents;  // 监控事件中满足条件返回的事件
};
        POLLIN普通或带外优先数据可读,即POLLRDNORM | POLLRDBAND
        POLLRDNORM-数据可读
        POLLRDBAND-优先级带数据可读
        POLLPRI 高优先级可读数据

        POLLOUT普通或带外数据可写
        POLLWRNORM-数据可写
        POLLWRBAND-优先级带数据可写

        POLLERR 发生错误
        POLLHUP 发生挂起
        POLLNVAL 描述字不是一个打开的文件

nfds 监控数组中有多少文件描述符需要被监控

timeout 毫秒级等待

-1：阻塞等，#define INFTIM -1 Linux中没有定义此宏
0：立即返回，不阻塞进程
>0：等待指定毫秒数，如当前系统时间精度不够毫秒，向上取值
 */



#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include <ctype.h>
#include <errno.h>
#include "../wrap.h"
#define MAXLINE 80
#define SERV_PORT 8000
#define OPEN_MAX 1024

int main(int argc, char *argv[])
{
    int i, j, maxi, listenfd, connfd, sockfd;
    int nready;
    ssize_t n;
    char buf[MAXLINE], str[INET_ADDRSTRLEN];
    socklen_t clilen;
    struct pollfd client[OPEN_MAX];
    struct sockaddr_in cliaddr, servaddr;
    listenfd = Socket(AF_INET, SOCK_STREAM, 0);//创建接口                                                          socket

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(SERV_PORT);

    Bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr));//绑定                                          bind

    Listen(listenfd, 20);//监听                                                                                    listen

    client[0].fd = listenfd;//文件描述符 用于接受连接
    client[0].events = POLLRDNORM;//listenfd上等待发生的事 （POLLRDNORM 有普通数据可读） 或者改成POLLIN
    //还想监控其他事件 可以用或逻辑运算符

    for (i = 1; i < OPEN_MAX; i++)
        client[i].fd = -1;	//用-1初始化client[]里剩下元素

    maxi = 0;//client[]数组有效元素中最大元素下标

    for ( ; ; ) {//默认语句2为真
        // 阻塞( 没有数据不会执行后面的语句)  获取可用文件描述符的个数  client的数组首地址 监控maxi+1个文件描述符，出错返回-1
        nready = poll(client, maxi+1, -1);

        if (client[0].revents & POLLRDNORM) //revents为传出参数， 有客户端链接请求
        {
            clilen = sizeof(cliaddr);//编译的时候就计算了
            connfd = Accept(listenfd, (struct sockaddr *)&cliaddr, &clilen);//接收连接                              Accept
            printf("received from %s at PORT %d\n", inet_ntop(AF_INET, &cliaddr.sin_addr, str, sizeof(str)), ntohs(cliaddr.sin_port));
            for (i = 1; i < OPEN_MAX; i++)
            {
                if (client[i].fd < 0)
                {
                    client[i].fd = connfd;//1. 找到client[]中空闲的位置,存放accept返回的connfd
                    break;
                }
            }
            if (i == OPEN_MAX)
                perr_exit("too many clients");
            client[i].events = POLLRDNORM;	// 2. 设置刚刚返回的connfd,监控读事件
            if (i > maxi)
                maxi = i; // 更新client[]中最大元素下标
            if (--nready <= 0)
                continue;// 没有更多就绪事件时,继续回到poll阻塞
        }

        for (i = 1; i <= maxi; i++) // 检测client[] 轮寻判断
        {
            if ( (sockfd = client[i].fd) < 0)
                continue;
            if (client[i].revents & (POLLRDNORM | POLLERR)) //传出数据是否有读数据到达 和 异常
            {
                if ( (n = Read(sockfd, buf, MAXLINE)) < 0) //读包
                {
                    if (errno == ECONNRESET) //当收到 RST标志时 ，是不是客户端断线重连 重置了 connection reset by client
                    {
                        printf("client[%d] aborted connection\n", i);
                        Close(sockfd);
                        client[i].fd = -1;
                    }
                    else
                        perr_exit("read error");
                }
                else if (n == 0)// connection closed by client
                {
                    printf("client[%d] closed connection\n", i);
                    Close(sockfd);
                    client[i].fd = -1;
                }
                else //处理数据
                {
                    for (j = 0; j < n; j++)
                        buf[j] = toupper(buf[j]);
                    Writen(sockfd, buf, n);
                }
                if (--nready <= 0)
                    break;
            }
        }
    }
    return 0;
}

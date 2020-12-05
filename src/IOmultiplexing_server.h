#ifndef __IO_SERVER_H
#define __IO_SERVER_H

#include "common.h"



typedef struct
{
    int maxfd;
    fd_set read_set;
    fd_set ready_set;
    int nready;
    int maxi;
    int clientfd[FD_SETSIZE];
    rio_t clientrio[FD_SETSIZE];
} pool;

int byte_cnt = 0;


void handle(int fd);
void serve_static(int fd, char *filename, int filesize);   //提供静态服务。
void serve_dynamic(int fd, char *cause, char *cgiargs);    //提供动态服务。
void sigchld_handler(int sig);

void init_pool(int listenfd,pool *p);
void add_client(int connfd,pool *p);
void check_clients(pool *p);


#endif

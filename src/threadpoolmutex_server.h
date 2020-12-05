#ifndef __THREAD_POOL_MUTEX_SERVER_H
#define __THREAD_POOL_MUTEX_SERVER_H

#include "common.h"

typedef struct
{
    int *buf;
    int n;		    /*Maximum number of slots*/
    int front;      /*buf[(front+1)%n] is first item*/
    int rear;		/*buf[rear%n] is last item*/
    int nready;/*item to ready to consume*/
    int nslots;/*avaliable slots equals n - nready*/
    pthread_mutex_t buf_mutex;
    pthread_mutex_t nready_mutex;
    pthread_cond_t cond;
}sbuf_t;


void handle(int fd);
void serve_static(int fd, char *filename, int filesize);   //提供静态服务。
void serve_dynamic(int fd, char *cause, char *cgiargs);    //提供动态服务。
void sigchld_handler(int sig);
void sbuf_init(sbuf_t *sp, int n);
void sbuf_deinit(sbuf_t *sp);
void sbuf_insert(sbuf_t *sp, int item);
int sbuf_remove(sbuf_t *sp);


#endif

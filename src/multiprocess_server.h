#ifndef __MULTI_PROCESS_SERVER_H
#define __MULTI_PROCESS_SERVER_H


#include "common.h"


void handle(int fd);
void serve_static(int fd, char *filename, int filesize);   //提供静态服务。
void serve_dynamic(int fd, char *cause, char *cgiargs);    //提供动态服务。
void sigchld_handler(int sig);


#endif

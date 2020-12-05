#ifndef __UTILS_H
#define __UTILS_H

#include "common.h"

void read_requesthdrs(rio_t *rp);  //读并忽略请求报头
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
void get_filetype(char *filename, char *filetype);
int parse_uri(char *uri, char *filename, char *cgiargs);

#endif

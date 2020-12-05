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


void doit(int fd);
void read_requesthdrs(rio_t *rp);  //读并忽略请求报头
int parse_uri(char *uri, char *filename, char *cgiargs);   //解析uri，得文件名存入filename中，参数存入cgiargs中.
void serve_static(int fd, char *filename, int filesize);   //提供静态服务。
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *cause, char *cgiargs);    //提供动态服务。
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
void sigchld_handler(int sig);

void init_pool(int listenfd,pool *p);
void add_client(int connfd,pool *p);
void check_clients(pool *p);

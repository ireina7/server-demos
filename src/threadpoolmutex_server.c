#include "threadpoolmutex_server.h"
#include "utils.h"


sbuf_t sbuf; /*global shared buffer of connected fd*/



void serve_static(int fd, char *filename, int filesize)
{
    int  srcfd;
    char *srcp,filetype[MAXLINE],buf[MAXBUF];

    get_filetype(filename,filetype);
    sprintf(buf,"HTTP/1.0 200 OK\r\n");
    sprintf(buf,"%sServer:Tiny Web Server\r\n", buf);
    sprintf(buf,"%sConnection:close\r\n", buf);
    sprintf(buf,"%sContent-length:%d\r\n", buf, filesize);
    sprintf(buf,"%sContent-type:%s\r\n\r\n", buf, filetype);
    Rio_writen(fd, buf, strlen(buf));
    printf("Response headers:\n");
    printf("%s", buf);

    srcfd = open(filename, O_RDONLY, 0);
    srcp = mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
    Close(srcfd);
    Rio_writen(fd, srcp, filesize);
    Munmap(srcp, filesize);
}


void serve_dynamic(int fd, char *filename, char *cgiargs)
{
    char buf[MAXLINE],*emptylist[] = { NULL };

    sprintf(buf,"HTTP/1.0 200 OK\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf,"Server:Web Server\r\n");
    rio_writen(fd, buf, strlen(buf));

    if(Fork() == 0) {
        setenv("QUERY_STRING", cgiargs, 1);
        Dup2(fd, STDOUT_FILENO);
        Execve(filename, emptylist, environ);
    }
    Wait(NULL);
}

void sigchld_handler(int sig)
{
    while(waitpid(-1, 0, WNOHANG) > 0)
        ;
    return;
}

void sbuf_init(sbuf_t *sp, int n)
{
    sp->buf = Calloc(n,sizeof(int));
    sp->n = n;
    sp->front = sp->rear = 0;
    sp->nready = 0;
    sp->nslots = n;
    pthread_mutex_init ( &(sp->buf_mutex), NULL);
    pthread_mutex_init ( &(sp->nready_mutex), NULL);
    pthread_cond_init ( &(sp->cond), NULL);
}

void sbuf_deinit(sbuf_t *sp)
{
    Free(sp->buf);
}
void sbuf_insert(sbuf_t *sp, int item)
{
	/*write to the buffer*/
    Pthread_mutex_lock(&sp->buf_mutex);
    if(sp->nslots == 0)
    {
	Pthread_mutex_unlock(&sp->buf_mutex);
	return ;
    }
    sp->buf[(++sp->rear)%(sp->n)] = item;
    sp->nslots--;
    Pthread_mutex_unlock(&sp->buf_mutex);

    int dosignal = 0;
    Pthread_mutex_lock(&sp->nready_mutex);
    if(sp->nready == 0)
    {
	dosignal = 1;
    }
    sp->nready++;
    Pthread_mutex_unlock(&sp->nready_mutex);
    if(dosignal)
    {
	Pthread_cond_signal(&sp->cond);
    }
}
int sbuf_remove(sbuf_t *sp)
{
    int item;
    Pthread_mutex_lock(&sp->nready_mutex);
    while(sp->nready == 0)
	Pthread_cond_wait(&sp->cond,&sp->nready_mutex);
    item = sp->buf[(++sp->front) % (sp->n)];
    Pthread_mutex_unlock(&sp->nready_mutex);
    if(item == 0)fprintf(stderr, "error!!!!fd item%d\n", item);
        return item;
}


void handle(int fd)
{
    int is_static;
    struct stat sbuf;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char filename[MAXLINE],cgiargs[MAXLINE];
    rio_t rio;

    Rio_readinitb(&rio, fd);
    Rio_readlineb(&rio, buf, MAXLINE);
    printf("Request headers:\n");
    printf("%s", buf);
    sscanf(buf, "%s %s %s", method, uri, version);
    if(strcasecmp(method,"GET")) {
        clienterror(fd, method, "501", "Not Implemented", "The WebServer does not implement this method");
        return;
    }
    read_requesthdrs(&rio);

    is_static = parse_uri(uri, filename, cgiargs);
    if(stat(filename, &sbuf) < 0) {
        clienterror(fd, filename, "404", "Not found", "The WebServer coundn't find this file");
        return;
    }

    if(is_static) {
        if(!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
            clienterror(fd,filename, "403", "Forbidden","The WebServer coundn't read the file");
            return;
        }
        serve_static(fd, filename, sbuf.st_size);
    }
    else {
        if(!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {
            clienterror(fd, filename, "403", "Forbidden","The WebServer coundn't run the CGI program");
            return;
        }
        serve_dynamic(fd, filename, cgiargs);
    }
}


/* $end tinymain */
void *run(void *vargp)
{
    Pthread_detach(pthread_self());
    while(1)
    {
        int connfd = sbuf_remove(&sbuf);
        handle(connfd);
        Close(connfd);
    }
}

int main(int argc, char **argv) {
    int listenfd, connfd, port, clientlen;
    struct sockaddr_in clientaddr;
    clientlen = sizeof(clientaddr);
    pthread_t tid;
    int NTHREADS,SBUFSIZE;

    if (argc != 4) {
        fprintf(stderr, "usage: %s <port> <num/thread> <fdbufsize>\n", argv[0]);
        exit(0);
    }
    port = atoi(argv[1]);
    NTHREADS = atoi(argv[2]);
    SBUFSIZE = atoi(argv[3]);

    Signal(SIGPIPE, SIG_IGN);
    Signal(SIGCHLD, sigchld_handler);

    sbuf_init(&sbuf, SBUFSIZE);
    listenfd = Open_listenfd(port);

    int i;
    for(i=0; i<NTHREADS; i++)/*create worker threads*/
    Pthread_create(&tid, NULL, run, NULL);

    while (1) {
    //connfd = Malloc(sizeof(int));//avoid race condition
        connfd = Accept(listenfd, (SA *)&clientaddr,&clientlen);
        sbuf_insert(&sbuf,connfd);
    }
}

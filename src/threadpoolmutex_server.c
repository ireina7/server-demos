#include "threadpoolmutex_server.h"



sbuf_t sbuf; /*global shared buffer of connected fd*/


void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
    char buf[MAXLINE], body[MAXBUF];

    sprintf(body, "<html><title>WebServer Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The Web server</em>\r\n", body);

    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, longmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    Rio_writen(fd, buf, strlen(buf));
    Rio_writen(fd, body, strlen(body));
}

void read_requesthdrs(rio_t *rp)
{
    char buf[MAXLINE];
    Rio_readlineb(rp, buf, MAXLINE);
    while(strcmp(buf,"\r\n")) {
        Rio_readlineb(rp, buf, MAXLINE);
        printf("%s", buf);
    }
    return;
}

int parse_uri(char *uri, char *filename,char *cgiargs)
{
    char *ptr;

    if(!strstr(uri,"cgi-bin")) {
        strcpy(cgiargs,"");
        strcpy(filename,".");
        strcat(filename, uri);
        if(uri[strlen(uri)-1] == '/') {
            strcat(filename,"home.html");
        }
        return 1;
    }
    else {
        ptr = index(uri,'?');
        if(ptr) {
            strcpy(cgiargs,ptr+1);
            *ptr = '\0';
        }
        else {
            strcpy(cgiargs, "");
        }
        strcpy(filename, ".");
        strcat(filename, uri);
        return 0;
    }
}

/* 
    打开文件名为filename的文件，把它映射到一个虚拟存储器空间，将文件的前filesize字节映射到从地址srcp开始的 
    虚拟存储区域。关闭文件描述符srcfd，把虚拟存储区的数据写入fd描述符，最后释放虚拟存储器区域。 
*/
void get_filetype(char *filename, char *filetype)
{
    if(strstr(filename, ".html"))
        strcpy(filetype, "text/html");
    else if(strstr(filename, ".gif"))
        strcpy(filetype, "image/gif");
    else if(strstr(filename, ".jpg"))
        strcpy(filetype, "image/jpeg");
    else if(strstr(filename, ".png"))
        strcpy(filetype, "image/png");
    else 
        strcpy(filetype, "text/plain");
}

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

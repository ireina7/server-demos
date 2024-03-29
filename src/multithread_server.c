#include "multithread_server.h"
#include "utils.h"




void serve_static(int fd, char *filename, int filesize)
{
    int  srcfd;
    char *srcp,filetype[MAXLINE],buf[MAXBUF];

    get_filetype(filename,filetype);
    sprintf(buf,"HTTP/1.0 200 OK\r\n");
    sprintf(buf,"%sServer: Web Server\r\n", buf);
    sprintf(buf,"%sConnection: close\r\n", buf);
    sprintf(buf,"%sContent-length: %d\r\n", buf, filesize);
    sprintf(buf,"%sContent-type: %s\r\n\r\n", buf, filetype);
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

void sigchld_handler(int sig){
    while(waitpid(-1, 0, WNOHANG) > 0)
        ;
    return;
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
        clienterror(fd, method, "501", "Not Implemented", "The Web Server does not implement this method");
        return;
    }
    read_requesthdrs(&rio);

    is_static = parse_uri(uri, filename, cgiargs);
    if(stat(filename, &sbuf) < 0) {
        clienterror(fd, filename, "404", "Not found", "The Web Server coundn't find this file");
        return;
    }

    if(is_static) {
        if(!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
            clienterror(fd,filename, "403", "Forbidden","The Web Server coundn't read the file");
            return;
        }
        serve_static(fd, filename, sbuf.st_size);
    }
    else {
        if(!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {
            clienterror(fd, filename, "403", "Forbidden","The Web Server coundn't run the CGI program");
            return;
        }
        serve_dynamic(fd, filename, cgiargs);
    }
}

/* $end tinymain */
void *run(void *vargp)
{
    int connfd = *((int *)vargp);
    Pthread_detach(pthread_self());
    Free(vargp);
    handle(connfd);
    Close(connfd);
    return NULL;
}


int main(int argc, char **argv)
{
    int listenfd, *connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    pthread_t tid;

  /* Check command line args */
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(0);
    }

    Signal(SIGPIPE,SIG_IGN);
    Signal(SIGCHLD,sigchld_handler);

    listenfd = Open_listenfd(atoi(argv[1]));
    while (1) {
        clientlen = sizeof(struct sockaddr_storage);
        connfd = Malloc(sizeof(int));//avoid race condition
        *connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        Pthread_create(&tid, NULL, run, connfd);
    }
}

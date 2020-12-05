#include "IOmultiplexing_server.h"
#include "utils.h"


void init_pool(int listenfd,pool *p)
{
    int i;
    p->maxi = -1;
    for(i=0; i<FD_SETSIZE; i++)
	p->clientfd[i] = -1;

    p->maxfd = listenfd;
    FD_ZERO(&(p->read_set));
    FD_SET(listenfd,&p->read_set);
}

void add_client(int connfd,pool *p)
{
    int i;
    p->nready--;
    for(i =0; i<FD_SETSIZE; i++)
    {
	if(p->clientfd[i] < 0)
	{
	    p->clientfd[i] = connfd;
            Rio_readinitb(&p->clientrio[i], connfd);

	    FD_SET(connfd,&p->read_set);

	    if(connfd > p->maxfd)
		p->maxfd = connfd;
	    if(i > p->maxi)
		p->maxi = i;
	    break;
	}
    }
    if(i == FD_SETSIZE)
	app_error("add_client error: Too many clients");
}



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
    char buf[MAXLINE], *emptylist[] = { NULL };

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

void check_clients(pool *p)
{
    int i, connfd, n;
    char buf[MAXLINE];
    rio_t rio;

    for(i=0; (i<=p->maxi) && (p->nready > 0); i++)
    {
	connfd = p->clientfd[i];
        rio = p->clientrio[i];
	if((connfd > 0) && (FD_ISSET(connfd,&p->ready_set))){
	    p->nready--;
	    handle(connfd);
            if((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0)
            {
		byte_cnt += n;
                printf("Server received %d (%d total) bytes on fd %d\n", n, byte_cnt, connfd);
            }
            else{
	        Close(connfd);
	        FD_CLR(connfd, &p->read_set);
	        p->clientfd[i] = -1;
            }
	}
    }
}





int main(int argc,char **argv)
{
    int listenfd,connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    clientlen = sizeof(clientaddr);
    static pool client_pool;
    if(argc != 2)
    {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }

    Signal(SIGCHLD,sigchld_handler);
    listenfd = Open_listenfd(atoi(argv[1]));
    init_pool(listenfd,&client_pool);

    sigset_t sig_chld;
    sigemptyset(&sig_chld);
    sigaddset(&sig_chld,SIGCHLD);

    while(1)
    {
	client_pool.ready_set = client_pool.read_set;
        while((client_pool.nready = Select(client_pool.maxfd+1,&client_pool.ready_set,NULL,NULL,NULL)) < 0)
	{
	    if(errno == EINTR)
		printf("got a signal restart pselect!!! \n");
	}
	if(FD_ISSET(listenfd, &client_pool.ready_set)){
            clientlen = sizeof(struct sockaddr_storage);
	    connfd = Accept(listenfd,(SA *)&clientaddr,&clientlen);
            add_client(connfd,&client_pool);
	}
	check_clients(&client_pool);
    }
}

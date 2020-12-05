#include "iterate_server.h"
#include "utils.h"



void serve_static(int fd, char *filename, int filesize)
{
    int  srcfd;
    char *srcp,filetype[MAXLINE],buffer[MAXBUF];

    get_filetype(filename,filetype);
    sprintf(buffer,"HTTP/1.0 200 OK\r\n");
    sprintf(buffer,"%sServer: Web Server\r\n", buffer);
    sprintf(buffer,"%sConnection: close\r\n", buffer);
    sprintf(buffer,"%sContent-length: %d\r\n", buffer, filesize);
    sprintf(buffer,"%sContent-type: %s\r\n\r\n", buffer, filetype);
    Rio_writen(fd, buffer, strlen(buffer));
    printf("Response headers:\n");
    printf("%s", buffer);

    srcfd = open(filename, O_RDONLY, 0);
    srcp = mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
    Close(srcfd);
    Rio_writen(fd, srcp, filesize);
    Munmap(srcp, filesize);
}


void serve_dynamic(int fd, char *filename, char *cgiargs)
{
    char buffer[MAXLINE],*emptylist[] = { NULL };

    sprintf(buffer,"HTTP/1.0 200 OK\r\n");
    Rio_writen(fd, buffer, strlen(buffer));
    sprintf(buffer,"Server:Web Server\r\n");
    rio_writen(fd, buffer, strlen(buffer));

    if(Fork() == 0) {
        setenv("QUERY_STRING", cgiargs, 1);
        Dup2(fd, STDOUT_FILENO);
        Execve(filename, emptylist, environ);
    }
    Wait(NULL);
}



void handle(int fd)
{
    int is_static;
    struct stat sbuffer;
    char buffer[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char filename[MAXLINE],cgiargs[MAXLINE];
    rio_t rio;

    Rio_readinitb(&rio, fd);
    Rio_readlineb(&rio, buffer, MAXLINE);
    printf("Request headers:\n");
    printf("%s", buffer);
    sscanf(buffer, "%s %s %s", method, uri, version);
    if(strcasecmp(method,"GET")) {
        clienterror(fd, method, "501", "Not Implemented", "The Web Server does not implement this method");
        return;
    }
    read_requesthdrs(&rio);

    is_static = parse_uri(uri, filename, cgiargs);
    if(stat(filename, &sbuffer) < 0) {
        clienterror(fd, filename, "404", "Not found", "The Web Server coundn't find this file");
        return;
    }

    if(is_static) {
        if(!(S_ISREG(sbuffer.st_mode)) || !(S_IRUSR & sbuffer.st_mode)) {
            clienterror(fd,filename, "403", "Forbidden","The Web Server coundn't read the file");
            return;
        }
        serve_static(fd, filename, sbuffer.st_size);
    }
    else {
        if(!(S_ISREG(sbuffer.st_mode)) || !(S_IXUSR & sbuffer.st_mode)) {
            clienterror(fd, filename, "403", "Forbidden","The Web Server coundn't run the CGI program");
            return;
        }
        serve_dynamic(fd, filename, cgiargs);
    }
}



int main(int argc, char const *argv[])
{
    int listenfd, connfd;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t  clientlen;
    struct sockaddr_storage clientaddr;

    if(argc != 2) {
        fprintf(stderr, "usage: %s\n", argv[0]);
        exit(0);
    }

    listenfd = Open_listenfd(atoi(argv[1]));
    while(1) {
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd,(SA *)&clientaddr, &clientlen);
        getnameinfo((SA*)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
        printf("Accept connection from [%s : %s]\n", hostname, port);
        handle(connfd);
        Close(connfd);
    }
}


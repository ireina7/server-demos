#include "iterate_server.h"



/**
    打开文件名为filename的文件，把它映射到一个虚拟存储器空间，
    将文件的前filesize字节映射到从地址srcp开始的
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
    char *srcp,filetype[MAXLINE],buffer[MAXBUF];

    get_filetype(filename,filetype);
    sprintf(buffer,"HTTP/1.0 200 OK\r\n");
    sprintf(buffer,"%sServer:Tiny Web Server\r\n", buffer);
    sprintf(buffer,"%sConnection:close\r\n", buffer);
    sprintf(buffer,"%sContent-length:%d\r\n", buffer, filesize);
    sprintf(buffer,"%sContent-type:%s\r\n\r\n", buffer, filetype);
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




void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
    char buffer[MAXLINE], body[MAXBUF];

    sprintf(body, "<html><title>WebServer Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The Web server</em>\r\n", body);

    sprintf(buffer, "HTTP/1.0 %s %s\r\n", errnum, longmsg);
    Rio_writen(fd, buffer, strlen(buffer));
    sprintf(buffer, "Content-type: text/html\r\n");
    Rio_writen(fd, buffer, strlen(buffer));
    sprintf(buffer, "Content-length: %d\r\n\r\n", (int)strlen(body));
    Rio_writen(fd, buffer, strlen(buffer));
    Rio_writen(fd, body, strlen(body));
}

void read_requesthdrs(rio_t *rp)
{
    char buffer[MAXLINE];
    Rio_readlineb(rp, buffer, MAXLINE);
    while(strcmp(buffer,"\r\n")) {
        Rio_readlineb(rp, buffer, MAXLINE);
        printf("%s", buffer);
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


void doit(int fd)
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
        clienterror(fd, method, "501", "Not Implemented", "The WebServer does not implement this method");
        return;
    }
    read_requesthdrs(&rio);

    is_static = parse_uri(uri, filename, cgiargs);
    if(stat(filename, &sbuffer) < 0) {
        clienterror(fd, filename, "404", "Not found", "The WebServer coundn't find this file");
        return;
    }

    if(is_static) {
        if(!(S_ISREG(sbuffer.st_mode)) || !(S_IRUSR & sbuffer.st_mode)) {
            clienterror(fd,filename, "403", "Forbidden","The WebServer coundn't read the file");
            return;
        }
        serve_static(fd, filename, sbuffer.st_size);
    }
    else {
        if(!(S_ISREG(sbuffer.st_mode)) || !(S_IXUSR & sbuffer.st_mode)) {
            clienterror(fd, filename, "403", "Forbidden","The WebServer coundn't run the CGI program");
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
        printf("Accept connection from (%s , %s)\n", hostname, port);
        doit(connfd);
        Close(connfd);
    }
}


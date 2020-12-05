#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <signal.h>
#include <sys/wait.h>
#include <pthread.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#define PRECHILD     5
#define MAXCHILD    50
#define BUFSIZE   4096
#define PIDPATH   "pid"
#define head503   "HTTP/1.1 503 Service unavailable\r\n"
#define head404   "HTTP/1.1 404 Not Found\r\n"
#define head200   "HTTP/1.1 200 0K\n\rContent—Type: text/html\n\rContent—Length: "

int len503, len404, len200;
int fd1[2], fd2[2];

typedef struct
{
        pid_t pid;
        char status; // 'n' means new request; 'f' means finish the request
} REPORT;

int s_pipe(int fd[2])
{
    return (socketpair(AF_UNIX, SOCK_STREAM, 0, fd));
}

void answer(int listenfd)
{
        int connfd;
        char buf[BUFSIZE];
        int count;
        int pid = getpid();
        struct sockaddr_in cliaddr;
        int size = sizeof(cliaddr);

        char comm;
        REPORT rep;
        rep.pid = pid;
        while (1)
        {
                connfd = accept(listenfd, (struct sockaddr *) &cliaddr,
                                (socklen_t *) &size);    //子进程accept请求
                rep.status = 'n';
                if (write(fd1[1], &rep, sizeof(rep)) < 0)
                { //通知父进程已经accept了请求
                        perror("write pipe new failed");
                        exit(-1);
                }

                count = read(connfd, buf, BUFSIZE);
                char req[10];
                char filepath[256];

                sscanf(buf, "%s%s", req, filepath + 1);
                filepath[0] = '.';
                if (strcmp("GET", req) != 0)
                { //503
                        write(connfd, head503, len503);
                        //goto err_out;
                        close(connfd);
                        exit(-1);
                }
                char content[BUFSIZE];
                struct stat stbuf;
                if (lstat(filepath, &stbuf) != 0)
                {
                        int err = errno;
                        if (err == ENOENT)
                        { //404
                                write(connfd, head404, len404);
                        }
                        close(connfd);
                        exit(-1);
                }
                count = write(connfd, head200, len200);
                u_int filesize = stbuf.st_size;
                sprintf(content, "%u\n\r\n\r", filesize);
                count = write(connfd, content, strlen(content));
                FILE *fp = fopen(filepath, "r");
                if (fp == NULL)
                {
                        printf("open file %s failed\n", filepath);
                        close(connfd);
                        exit(-1);
                }
                while ((count = fread(content, 1, sizeof(content), fp)) > 0)
                {
                        //printf("%s", content);
                        if (write(connfd, content, count) != count)
                        {
                                printf("write failed\n");
                        }
                }
                fclose(fp);
                close(connfd);

                rep.status = 'f';
                if (write(fd1[1], &rep, sizeof(rep)) < 0)
                { //告诉父进程自己处理完了请求
                        perror("write pipe finish failed");
                        exit(-1);
                }

                if (read(fd2[0], &comm, 1) < 1)
                { //等待来自父进程的命令
                        perror("read pipe failed");
                        exit(-1);
                }
                //printf("[%d] reve %c from pa\n", pid, comm);
                if (comm == 'e')
                { //收到exit命令
                        printf("[%d] exit\n", pid);
                        exit(-1);
                }
                else if (comm == 'c')
                { //收到继续accept的命令
                        printf("[%d] continue\n", pid);
                }
                else
                {
                        printf("[%d] comm : %c illeagle\n", pid, comm);
                }
        }
}

void usage()
{
        printf("Usage: http-serv port\n");
}

int write_pid()
{
        int fd;
        if ((fd = open(PIDPATH, O_WRONLY | O_TRUNC | O_CREAT, S_IWUSR)) < 0)
        {
                perror("open pidfile faild");
                return -1;
        }
        struct flock lock;
        lock.l_type = F_WRLCK;
        lock.l_start = 0;
        lock.l_whence = SEEK_SET;
        lock.l_len = 0;

        if (fcntl(fd, F_SETLK, &lock) == -1)
        {
                int err = errno;
                perror("fcntl faild");
                if (err == EAGAIN)
                {
                        printf("Another http-serv process is running now!\n");
                }
                return -1;
        }
        return 0;
}

void daemon_init()
{
        //clear file creation mask;
        umask(0);

        //become a session leader
        if (fork() != 0)
                exit(-1);
        if (setsid() < 0)
                exit(-1);

        //make sure can be never get the TTY control
        if (fork() != 0)
                exit(-1);

        //may chdir here

        int i;
        for (i = 0; i < 1024; i++)
                close(i);

        /*
         * Attach file descriptors 0, 1, and 2 to /dev/null.
         */
        int fd0, fd1, fd2;
        fd0 = open("/dev/null", O_RDWR);
        fd1 = dup(0);
        fd2 = dup(0);
        if (fd0 != 0 || fd1 != 1 || fd2 != 2)
        {
                printf("init failed\n");
                exit(-1);
        }

}

int main(int argc, char **argv)
{
        int listenfd;
        struct sockaddr_in servaddr;
        pid_t pid;

        if (argc != 2)
        {
                usage();
                return -1;
        }

        signal(SIGCHLD, SIG_IGN);

        len200 = strlen(head200);
        len404 = strlen(head404);
        len503 = strlen(head503);

        //daemon_init();    //转为后台程序，如需打印调试，把这行注释掉
        if (write_pid() < 0) //避免同时有多个该程序在运行
                return -1;
        if (pipe(fd1) < 0)
        {
                perror("pipe failed");
                exit(-1);
        }
        if (s_pipe(fd2) < 0)
        {
                perror("pipe failed");
                exit(-1);
        }
        int port = atoi(argv[1]);
        //initialize servaddr and listenfd...
        bzero(&servaddr, sizeof(servaddr));
        servaddr.sin_family = AF_INET;
        servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
        servaddr.sin_port = htons(port);

        listenfd = socket(AF_INET, SOCK_STREAM, 0);
        bind(listenfd, (struct sockaddr *) &servaddr, sizeof(servaddr));
        listen(listenfd, 1000);
        int i;
        for (i = 0; i < PRECHILD; i++)
        { //父进程预fork 子进程
                if ((pid = fork()) < 0)
                {
                        perror("fork faild");
                        exit(3);
                }
                else if (pid == 0)
                {
                        answer(listenfd);
                }
                else
                {
                        printf("have create child %d\n", pid);
                }
        }

        char e = 'e';
        char c = 'c';
        int req_num = 0;
        int child_num = PRECHILD;
        REPORT rep;
        while (1)
        {
                //printf("req_num = %d, child_num = %d\n", req_num, child_num);
                if (read(fd1[0], &rep, sizeof(rep)) < sizeof(rep))
                { //等待子进程发来消息
                        perror("parent read pipe failed");
                        exit(-1);
                }
                //printf("parent: receive from %d\n", pid);
                if (rep.status == 'n')
                { //子进程刚accept了新的请求
                        req_num++;
                        printf("parent: %d have receive new request\n", rep.pid);
                        if (req_num >= child_num && child_num <= MAXCHILD)
                        { //请求数过多，创建更多子进程
                                if ((pid = fork()) < 0)
                                {
                                        perror("fork faild");
                                        exit(3);
                                }
                                else if (pid == 0)
                                {
                                        answer(listenfd);
                                }
                                else
                                {
                                        printf("have create child %d\n", pid);
                                        child_num++;
                                }
                        }
                }
                else if (rep.status == 'f')
                { //子进程刚处理完了一个请求
                        req_num--;
                        //printf("parent: %d have finish a request\n", rep.pid);
                        if (child_num > (req_num + 1) && child_num > PRECHILD)
                        { //子进程数过多，删除多余的子进程
                                if (write(fd2[1], &e, sizeof(e)) < sizeof(e))
                                {
                                        perror("pa write pipe failed");
                                        exit(-2);
                                }
                                //printf("tell child exit\n");
                                child_num--;
                        }
                        else
                        {
                                if (write(fd2[1], &c, sizeof(c)) < sizeof(c))
                                { //让子进程继续等待accept
                                        perror("pa write pipe failed");
                                        exit(-2);
                                }
                                //printf("tell child continue\n");
                        }
                }
        }
        return 0;
}

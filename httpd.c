/* J. David's webserver */
/* This is a simple webserver.
 * Created November 1999 by J. David Blackstone.
 * CSE 4344 (Network concepts), Prof. Zeigler
 * University of Texas at Arlington
 */
/* This program compiles for Sparc Solaris 2.6.
 * To compile for Linux:
 *  1) Comment out the #include <pthread.h> line.
 *  2) Comment out the line that defines the variable newthread.
 *  3) Comment out the two lines that run pthread_create().
 *  4) Uncomment the line that runs accept_request().
 *  5) Remove -lsocket from the Makefile.
 */
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ctype.h>
#include <strings.h>
#include <string.h>
#include <sys/stat.h>
#include <pthread.h>
#include <sys/wait.h>
#include <stdlib.h>

#define ISspace(x) isspace((int)(x))

#define SERVER_STRING "Server: jdbhttpd/0.1.0\r\n"//HTTP服务器信息

void *accept_request(void *);
void bad_request(int);
void cat(int, FILE *);
void cannot_execute(int);
void error_die(const char *);
void execute_cgi(int, const char *, const char *, const char *);
int get_line(int, char *, int);
void headers(int, const char *);
void not_found(int);
void serve_file(int, const char *);
int startup(u_short *);
void unimplemented(int);

/**********************************************************************/
/* A request has caused a call to accept() on the server port to
 * return.  Process the request appropriately.在服务端口返回一个调用accept()的请求，适当的请求处理。
 * Parameters: the socket connected to the client */
/**********************************************************************/
void *accept_request(void* tclient) {
    int client = *(int *)tclient;
    char buf[1024];
    int numchars;
    char method[255];
    char url[255];
    char path[512];
    size_t i, j;
    struct stat st;
    int cgi = 0;      /* becomes true if server decides this is a CGI
                    * program */
    char *query_string = NULL;

    numchars = get_line(client, buf, sizeof(buf));//得到请求第一行。
    i = 0;
    j = 0;
    while (!ISspace(buf[j]) && (i < sizeof(method) - 1)) {//存取请求方法到method。
        method[i] = buf[j];
        i++;
        j++;
    }
    method[i] = '\0';
    //既不是GET也不是POST，无法处理。
    if (strcasecmp(method, "GET") && strcasecmp(method, "POST")) {//strcasecmp：忽略大小写比较字符串，相同时返回0。
        unimplemented(client);//通知客户端无法处理。
        return NULL;
    }
    //POST请求时。
    if (strcasecmp(method, "POST") == 0)
        cgi = 1;//开启cgi。

    i = 0;
    while (ISspace(buf[j]) && (j < sizeof(buf)))
        j++;
    while (!ISspace(buf[j]) && (i < sizeof(url) - 1) && (j < sizeof(buf))) {//存取请求URI到url
        url[i] = buf[j];
        i++;
        j++;
    }
    url[i] = '\0';
    //GET请求时。
    if (strcasecmp(method, "GET") == 0) {
        query_string = url;
        while ((*query_string != '?') && (*query_string != '\0'))
            query_string++;
        if (*query_string == '?') {//有参数
            cgi = 1;
            *query_string = '\0';
            query_string++;
        }
    }

    sprintf(path, "htdocs%s", url);//格式化url到path数组，html文件都在htdocs目录下
    if (path[strlen(path) - 1] == '/')//默认index.html补全
        strcat(path, "index.html");
    if (stat(path, &st) == -1) {//stat:通过文件名获取文件信息，并保存在st所指的结构体stat中。成功0，失败-1。
        while ((numchars > 0) && strcmp("\n", buf))  /* read & discard headers */ //丢弃所有headers信息
            numchars = get_line(client, buf, sizeof(buf));
        not_found(client);//返回404
    } else {
        if ((st.st_mode & S_IFMT) == S_IFDIR)//st_mode:文件的类型和存取的权限。S_IFMT：文件类型的位遮罩，S_IFDIR：目录。
            strcat(path, "/index.html");
        if ((st.st_mode & S_IXUSR) ||//文件所有者具可执行权限
                (st.st_mode & S_IXGRP) ||//用户组具可执行权限
                (st.st_mode & S_IXOTH)    )//其他用户具可执行权限
            cgi = 1;//启动cgi
        if (!cgi)
            serve_file(client, path);//直接把服务器文件返回
        else
            execute_cgi(client, path, method, query_string);//执行cgi
    }

    close(client);
    return NULL;
}

/**********************************************************************/
/* Inform the client that a request it has made has a problem.通知客户端请求有问题
 * Parameters: client socket */
/**********************************************************************/
void bad_request(int client) {//400
    char buf[1024];

    sprintf(buf, "HTTP/1.0 400 BAD REQUEST\r\n");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "Content-type: text/html\r\n");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "<P>Your browser sent a bad request, ");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "such as a POST without a Content-Length.\r\n");
    send(client, buf, sizeof(buf), 0);
}

/**********************************************************************/
/* Put the entire contents of a file out on a socket.  This function
 * is named after the UNIX "cat" command, because it might have been
 * easier just to do something like pipe, fork, and exec("cat").
 * Parameters: the client socket descriptor
 *             FILE pointer for the file to cat */
/**********************************************************************/
void cat(int client, FILE *resource) {
    char buf[1024];

    fgets(buf, sizeof(buf), resource);//fgets：从文件结构体指针resource中读取数据，每次读取一行。
    while (!feof(resource)) {
        send(client, buf, strlen(buf), 0);
        fgets(buf, sizeof(buf), resource);
    }
}

/**********************************************************************/
/* Inform the client that a CGI script could not be executed.
 * Parameter: the client socket descriptor. */
/**********************************************************************/
void cannot_execute(int client) {//500
    char buf[1024];

    sprintf(buf, "HTTP/1.0 500 Internal Server Error\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<P>Error prohibited CGI execution.\r\n");
    send(client, buf, strlen(buf), 0);
}

/**********************************************************************/
/* Print out an error message with perror() (for system errors; based
 * on value of errno, which indicates system call errors) and exit the
 * program indicating an error. */
/**********************************************************************/
void error_die(const char *sc) {
    perror(sc);
    exit(1);
}

/**********************************************************************/
/* Execute a CGI script.  Will need to set environment variables as
 * appropriate.
 * Parameters: client socket descriptor
 *             path to the CGI script */
/**********************************************************************/
void execute_cgi(int client, const char *path,
                 const char *method, const char *query_string) {
    char buf[1024];
    int cgi_output[2];
    int cgi_input[2];
    pid_t pid;
    int status;
    int i;
    char c;
    int numchars = 1;
    int content_length = -1;

    buf[0] = 'A';
    buf[1] = '\0';
    if (strcasecmp(method, "GET") == 0)//get请求
        while ((numchars > 0) && strcmp("\n", buf))  /* read & discard headers *///丢弃所有headers信息。
            numchars = get_line(client, buf, sizeof(buf));
    else {  /* POST */
        numchars = get_line(client, buf, sizeof(buf));
        while ((numchars > 0) && strcmp("\n", buf)) {//丢弃hearders信息。
            buf[15] = '\0';
            if (strcasecmp(buf, "Content-Length:") == 0)//存储下Content-Length。
                content_length = atoi(&(buf[16]));//atoi：把字符串转换成整型。
            numchars = get_line(client, buf, sizeof(buf));
        }
        if (content_length == -1) {//没有获取到Content-Length
            bad_request(client);
            return;
        }
    }

    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    send(client, buf, strlen(buf), 0);

    if (pipe(cgi_output) < 0) {//pipe：建立管道，由参数cgi_output数组返回。cgi_output[0]为管道里的读取端,cgi_output[1]则为管道的写入端。
        cannot_execute(client);
        return;
    }
    if (pipe(cgi_input) < 0) {
        cannot_execute(client);
        return;
    }

    if ( (pid = fork()) < 0 ) {//fork：创建一个与原来进程几乎完全相同的进程。把原来的进程的所有值都复制到新的新进程中，只有少数值与原来的进程的值不同。
        cannot_execute(client);
        return;
    }
    if (pid == 0) { /* child: CGI script */
        char meth_env[255];
        char query_env[255];
        char length_env[255];

        dup2(cgi_output[1], 1);//重定向写入端到cgi_output那一管道的写入端。
        dup2(cgi_input[0], 0);//重定向读取端到cgi_input那一管道的读取端。
        close(cgi_output[0]);//关闭cgi_output那一管道的读取端。
        close(cgi_input[1]);//关闭cgi_input那一管道的写入端。
        sprintf(meth_env, "REQUEST_METHOD=%s", method);
        putenv(meth_env);//向环境表中添加环境变量REQUEST_METHOD。
        if (strcasecmp(method, "GET") == 0) {
            sprintf(query_env, "QUERY_STRING=%s", query_string);//GET请求url中问号后的请求参数。
            putenv(query_env);
        } else { /* POST */
            sprintf(length_env, "CONTENT_LENGTH=%d", content_length);
            putenv(length_env);
        }
        execl(path, path, NULL);//execl：执行参数path文件路径，执行参数为这一可执行文件，最后一个参数必须用空指针(NULL)作结束. 
        exit(0);//结束当前子进程
    } else {    /* parent */
        close(cgi_output[1]);//关闭cgi_output那一管道的写入端。
        close(cgi_input[0]);//关闭cgi_input那一管道的读取端。
        if (strcasecmp(method, "POST") == 0)
            for (i = 0; i < content_length; i++) {
                recv(client, &c, 1, 0);//recv函数就把客户端sock接收缓冲中的数据copy到c中。
                write(cgi_input[1], &c, 1);//写入到cgi_input管道中
            }
        while (read(cgi_output[0], &c, 1) > 0)//读取cgi_output管道中。
            send(client, &c, 1, 0);

        close(cgi_output[0]);
        close(cgi_input[1]);
        waitpid(pid, &status, 0);//使父进程阻塞，直到子进程pid结束。
    }
}

/**********************************************************************/
/* Get a line from a socket, whether the line ends in a newline,
 * carriage return, or a CRLF combination.  Terminates the string read
 * with a null character.  If no newline indicator is found before the
 * end of the buffer, the string is terminated with a null.  If any of
 * the above three line terminators is read, the last character of the
 * string will be a linefeed and the string will be terminated with a
 * null character.从套接字获取一行，无论该行是以换行符结束，回车还是CRLF组合（\r,\n,\r\n)。
 * Parameters: the socket descriptor
 *             the buffer to save the data in
 *             the size of the buffer
 * Returns: the number of bytes stored (excluding null) */
/**********************************************************************/
int get_line(int sock, char *buf, int size) {
    int i = 0;
    char c = '\0';
    int n;

    while ((i < size - 1) && (c != '\n')) {
        n = recv(sock, &c, 1, 0);//recv：当协议把数据接收完毕，recv函数就把sock接收缓冲中的数据copy到buf中。
        //sock：接收端套接字描述符，&c:存放recv函数接收到的数据的缓冲区，1：缓冲区的长度，0：参数可用于影响函数调用的行为。返回其实际copy的字节数。
        /* DEBUG printf("%02X\n", c); */
        if (n > 0) {
            if (c == '\r') {//回车
                n = recv(sock, &c, 1, MSG_PEEK);//MSG_PEEK：窥视传入的数据。数据将复制到缓冲区中，但不会从输入队列中删除。 这是为什么呢？
                /* DEBUG printf("%02X\n", c); */
                if ((n > 0) && (c == '\n'))
                    recv(sock, &c, 1, 0);
                else
                    c = '\n';
            }
            buf[i] = c;
            i++;
        } else
            c = '\n';//结束
    }
    buf[i] = '\0';

    return(i);
}

/**********************************************************************/
/* Return the informational HTTP headers about a file. *///返回有关文件的信息HTTP标头。
/* Parameters: the socket to print the headers on
 *             the name of the file */
/**********************************************************************/
void headers(int client, const char *filename) {//200
    char buf[1024];
    (void)filename;  /* could use filename to determine file type */

    strcpy(buf, "HTTP/1.0 200 OK\r\n");
    send(client, buf, strlen(buf), 0);
    strcpy(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    strcpy(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
}

/**********************************************************************/
/* Give a client a 404 not found status message. */
/**********************************************************************/
void not_found(int client) {//404
    char buf[1024];

    sprintf(buf, "HTTP/1.0 404 NOT FOUND\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<HTML><TITLE>Not Found</TITLE>\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>The server could not fulfill\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "your request because the resource specified\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "is unavailable or nonexistent.\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(client, buf, strlen(buf), 0);
}

/**********************************************************************/
/* Send a regular file to the client.  Use headers, and report
 * errors to client if they occur.
 * Parameters: a pointer to a file structure produced from the socket
 *              file descriptor
 *             the name of the file to serve */
/**********************************************************************/
void serve_file(int client, const char *filename) {
    FILE *resource = NULL;
    int numchars = 1;
    char buf[1024];

    buf[0] = 'A';
    buf[1] = '\0';
    while ((numchars > 0) && strcmp("\n", buf))  /* read & discard headers */ //丢弃所有headers信息
        numchars = get_line(client, buf, sizeof(buf));

    resource = fopen(filename, "r");
    if (resource == NULL)
        not_found(client);
    else {
        headers(client, filename);//响应首部
        cat(client, resource);
    }
    fclose(resource);
}

/**********************************************************************/
/* This function starts the process of listening for web connections
 * on a specified port.  If the port is 0, then dynamically allocate a
 * port and modify the original port variable to reflect the actual
 * port.
 * Parameters: pointer to variable containing the port to connect on
 * Returns: the socket */
/**********************************************************************/
int startup(u_short *port) {
    int httpd = 0;
    struct sockaddr_in name;//sockaddr_in:sin_family(地址族)，sin_port(16位端口)，sin_addr(32位IP地址)，sin_zero

    httpd = socket(PF_INET, SOCK_STREAM, 0);//建立TCP socket
    if (httpd == -1)
        error_die("socket");
    memset(&name, 0, sizeof(name));//结构体初始化
    name.sin_family = AF_INET;//地址族
    name.sin_port = htons(*port);//htons(将主机字节顺序转换为网络字节顺序,host to network short)
    name.sin_addr.s_addr = htonl(INADDR_ANY);//htons(将主机字节顺序转换为网络字节顺序,host to network long),INADDR_ANY(任意地址,一般0.0.0.0)
    if (bind(httpd, (struct sockaddr *)&name, sizeof(name)) < 0)//bind:把一个本地协议地址赋予一个套接字
        error_die("bind");
    if (*port == 0) { /* if dynamically allocating a port */
        socklen_t namelen = sizeof(name);
        if (getsockname(httpd, (struct sockaddr *)&name, &namelen) == -1)//getsockname()函数用于获得一个与socket相关的地址。
            error_die("getsockname");
        *port = ntohs(name.sin_port);//ntohs(将网络字节顺序转化为主机字节顺序，network to host short)
    }
    if (listen(httpd, 5) < 0)//让一个套接字处于监听到来的连接请求的状态
        error_die("listen");
    return(httpd);
}

/**********************************************************************/
/* Inform the client that the requested web method has not been
 * implemented.
 * Parameter: the client socket */
/**********************************************************************/
void unimplemented(int client) {//501
    char buf[1024];

    sprintf(buf, "HTTP/1.0 501 Method Not Implemented\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<HTML><HEAD><TITLE>Method Not Implemented\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</TITLE></HEAD>\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>HTTP request method not supported.\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(client, buf, strlen(buf), 0);
}

/**********************************************************************/

int main(void) {
    int server_sock = -1;
    u_short port = 0;//端口号，0表示随机
    int client_sock = -1;
    struct sockaddr_in client_name;
    socklen_t client_name_len = sizeof(client_name);
    pthread_t newthread;
    //初始化 httpd 服务
    server_sock = startup(&port);
    printf("httpd running on port %d\n", port);

    while (1) {//accept:接收一个套接字中已建立的连接
        client_sock = accept(server_sock,
                             (struct sockaddr *)&client_name,
                             &client_name_len);
        if (client_sock == -1)
            error_die("accept");
        /* accept_request(client_sock); */
        if (pthread_create(&newthread , NULL, accept_request, (void*)&client_sock) != 0)//创建线程。
            //int pthread_create(pthread_t *tidp,const pthread_attr_t *attr,(void*)(*start_rtn)(void*),void *arg);
            //成功时，tidp（指向新线程的ID），attr（指定各种不同的线程属性）,start_rtn函数（新创建的线程从start_rtn函数的地址开始运行），arg（指向start_rtn函数传递的参数）。
            perror("pthread_create");
    }

    close(server_sock);

    return(0);
}


#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/epoll.h>
#include <fcntl.h>

#define MAX_SIZE 1024 // 最大事件数
#define BUF_SIZE 202048 // 缓冲区大小

void* request_handler(void *arg);  // 处理客户端请求
void send_data(FILE *fp,char *content,char *file_name); // 发送数据
const char * content_type(char *file); // 获取文件类型
void send_error(FILE *fp_write); // 发送错误信息

int main()
{
    int serv_sock=socket(AF_INET,SOCK_STREAM,0);
    struct sockaddr_in serv_addr,client_addr;
    memset(&serv_addr,0,sizeof(serv_addr));
    serv_addr.sin_family=AF_INET;
    serv_addr.sin_port=htons(10000);
    serv_addr.sin_addr.s_addr=htonl(INADDR_ANY);
    int opt=1;
    setsockopt(serv_sock,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt)); // 设置端口复用
    if(bind(serv_sock,(struct sockaddr * ) & serv_addr,sizeof (serv_addr))==-1)
    {
        perror("bind");
        exit(-1);
    }
    if(listen(serv_sock,10)==-1)
    {
        perror("listen");
        exit(-1);
    }
    socklen_t len;
    while(1)
    {
        int client_sock=accept(serv_sock,(struct sockaddr*)&client_addr,&len);
        if(client_sock==-1)
        {
            perror("accept");
            exit(-1);
        }
        pthread_t tid;
        pthread_create(&tid,NULL,request_handler,&client_sock);
        pthread_detach(tid);
    }
}
void *request_handler(void *arg)
{
    int client_sock=*(int*)arg;
    char buf[BUF_SIZE];
    FILE *fp_read,*fp_write;
    fp_read=fdopen(client_sock,"r");
    fp_write=fdopen(dup(client_sock),"w"); // 复制文件描述符,I/O分流
    setbuf(fp_write,NULL); // 设置为无缓冲，立即输出

    fgets(buf,sizeof(buf),fp_read);
    printf("%s",buf);
    if(strstr(buf,"HTTP/")==NULL) //strstr()函数用于判断字符串是否包含另一个字符串，如果包含则返回第一个字符串在第二个字符串中首次出现的地址，否则返回NULL
    {
        shutdown(fileno(fp_read),SHUT_RD); // 关闭套接字
        send_error(fp_write); // 发送错误信息
        fclose(fp_write);
        fclose(fp_read);
        printf("非HTTP请求\n");
        exit(1);
    }
    else
    {
        char method[10]={0},path[100]={0},type[10]={0},path_file[100]; // 请求方法，请求路径，协议版本
        strcpy(method,strtok(buf,"/")); // 分割字符串 ,将第一个/替换为\0
#ifndef DEBUG
        printf("method:%s\n",method);
#endif
        strcpy(path,strtok(NULL,"/")); // 将第二个/替换为\0
#ifndef DEBUG
        printf("path:%s\n",path);
#endif
        if(strncmp(method,"GET",3)!=0)
        {
            shutdown(fileno(fp_read),SHUT_RD);
            fputs("HTTP/1.x 501 Not Implemented\r\n",fp_write); // 未实现请求方法
            fclose(fp_write);
            printf("不支持的请求方法\n");
            exit(0);
        }
        strcpy(path_file,path);
        strcpy(type, content_type(path)); // 获取请求文件类型
        printf("type:%s\n",type);
        fclose(fp_read);
        send_data(fp_write,type,path); // 发送数据
    }
    return NULL;
}
const char* content_type(char *file)
{
    strtok(file,".");
    char *type=strtok(NULL,".");
    if(strncmp(type,"html",4)==0||strncmp(type,"htm",3)==0)
    {
        return "text/html";
    }
    else if(strcmp(type,"jpg")==0||strcmp(type,"jpeg")==0)
    {
        return "image/jpeg";
    }
    else if(strcmp(type,"gif")==0)
    {
        return "image/gif";
    }
    else if(strcmp(type,"png")==0)
    {
        return "image/png";
    }
    else if(strcmp(type,"css")==0)
    {
        return "text/css";
    }
    else if(strcmp(type,"au")==0)
    {
        return "audio/basic";
    }
    else if(strcmp(type,"wav")==0)
    {
        return "audio/wav";
    }
    else if(strcmp(type,"avi")==0)
    {
        return "video/x-msvideo";
    }
    else if(strcmp(type,"mpeg")==0||strcmp(type,"mpg")==0)
    {
        return "video/mpeg";
    }
    else if(strcmp(type,"mp3")==0)
    {
        return "audio/mpeg";
    }
    else
    {
        return "text/plain";
    }
}
void send_data(FILE *fp,char *type,char *path)
{
    char status[]="HTTP/1.1 200 OK\r\n"; // 响应状态行
    // 首部行
    char server[]="Server: Linux Web Server\r\n";// \r\n表示回车换行
    char content_len[]="Content-Length: 2048\r\n";
    char content_type[50]={0};
    sprintf(content_type,"Content-Type: %s\r\n\r\n",type); // 格式化字符串,响应类型
    fputs(status,fp);
    fputs(server,fp);
    fputs(content_len,fp);
    fputs(content_type,fp);
    FILE * fp_file = fopen("index.txt","r");
    if(fp_file==NULL)
    {
        send_error(fp);
        printf("文件打开失败\n");
        exit(1);
    }
    else
    {
        char buf[BUF_SIZE]={0};
        while(!feof(fp_file))
        {
            fgets(buf,sizeof(buf),fp_file);
            fputs(buf,fp);
            fflush(fp);
        }
    }
    fclose(fp);
}
void send_error(FILE *fp)
{
    char status[]="HTTP/1.x 404 Not Found\r\n";
    char server[]="Server: Linux Web Server\r\n";
    char content_len[]="Content-Length: 1024\r\n";
    char content_type[]="Content-Type: text/html\r\n\r\n";
    char body[]="<html><head><title>Not Found</title></head><body><p>404,Not Found</p></body></html>";
    fputs(status,fp);
    fputs(server,fp);
    fputs(content_len,fp);
    fputs(content_type,fp);
    fputs(body,fp);
    fflush(fp);
    fclose(fp);
}
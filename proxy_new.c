#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <netdb.h>
#include <sys/time.h>

#define BUFFSIZE 2048
#define PROXYIP "192.168.56.101"
#define SERVIP "192.168.56.1"
#define CLTIP "192.168.56.1"
#define CONTROLPORT 21
#define DATAPORT 20
#define RANDOMPORT 25000//192.168.56.101.97.168


 
    //bind() and listen()
    int bindAndListenSocket(int port){
        int sock;
        struct sockaddr_in addr;

        //create socket
        if((sock=socket(PF_INET,SOCK_STREAM,0))<0){
            printf("socket() failed.\n");
            exit(0);
        }

        memset(&addr,0,sizeof(addr));
        addr.sin_family=AF_INET;
        addr.sin_addr.s_addr=inet_addr(PROXYIP);
        addr.sin_port=htons(port);
     
        //bind()
        if((bind(sock,(struct sockaddr *) &addr,sizeof(addr)))<0){
            printf("bind() failed.\n");
            exit(0);
        }

        //listen()
        int qlength=200;
        if((listen(sock,qlength))<0){
            printf("listen() failed.\n");
            exit(0);
        }
        printf("bindAndListenSocket(): port %d successfully.\n", port);
        return sock;
    }

    int acceptSocket(int sock){
        int sock_new;
        struct sockaddr_in addr;
        int addrLen;
        addrLen=sizeof(addr);   
        if((sock_new=accept(sock,(struct sockaddr *) &addr,&addrLen))<0){
            printf("accept() failed.\n");
            exit(0);
        }
        printf("acceptSocket() successfully.\n");
        return sock_new;
    }//proxy 
    
    int connectTo(char *ip, int port){
        int sock;
        struct sockaddr_in addr;

        memset(&addr,0,sizeof(addr));
        addr.sin_family=AF_INET;
        addr.sin_addr.s_addr=inet_addr(ip);
        addr.sin_port=htons(port);

        //create a new socket for control connection to server or data connection with cilent
        if((sock=socket(PF_INET,SOCK_STREAM,0))<0){
            printf("socket() failed.\n");
            exit(0);
        }
        //control connection with server or data connection with client
        if((connect(sock,(struct sockaddr *) &addr, sizeof(addr)))<0){
            printf("connectTo(): port %d failed.\n",port);
            close(sock);
            exit(0);
        }
        printf("connectTo(): port %d successfully.\n", port); 
        return sock;
    }//proxy 


int main(int argc, const char *argv[])
{
    fd_set master_set, working_set;  //文件描述符集合
    struct timeval timeout;          //select 参数中的超时结构体

    int proxy_cmd_socket    = 0;     //proxy listen控制连接
    int accept_cmd_socket   = 0;     //proxy accept客户端请求的控制连接
    int connect_cmd_socket  = 0;     //proxy connect服务器建立控制连接
    int proxy_data_socket   = 0;     //proxy listen数据连接
    int accept_data_socket  = 0;     //proxy accept得到请求的数据连接（主动模式时accept得到服务器数据连接的请求，被动模式时accept得到客户端数据连接的请求）
    int connect_data_socket = 0;     //proxy connect建立数据连接 （主动模式时connect客户端建立数据连接，被动模式时connect服务器端建立数据连接）
    int cache_socket=0;//socket for cache

    int selectResult = 0;     //select函数返回值
    int select_sd = 10;    //select 函数监听的最大文件描述符

    int dataConnectPort_s;
    int dataConnectPort_c;

    int download;
    int isCached;
    int active;//??¨¨??a¡À??¡¥?¡ê¨º?¡ê?3y¡¤?PORT

    char *fileName;

    FD_ZERO(&master_set);   //????master_set?¡¥o?
    bzero(&timeout, sizeof(timeout));
    
    proxy_cmd_socket = bindAndListenSocket(CONTROLPORT);  //?a??proxy_cmd_socket?¡ébind¡ê¡§¡ê??¡élisten2¨´¡Á¡Â,?¨²21???¨²
    //proxy_data_socket = bindAndListenSocket(RANDOMPORT);//?a??proxy_cmd_socket?¡ébind¡ê¡§¡ê??¡élisten2¨´¡Á¡Â,?¨²???¨²???¨²
    //FD_SET(proxy_data_socket, &master_set);//??proxy_data_socket?¨®¨¨?master_set?¡¥o?  
    FD_SET(proxy_cmd_socket, &master_set);  //??proxy_cmd_socket?¨®¨¨?master_set?¡¥o?
    
    timeout.tv_sec = 2000;    //Select¦Ì?3?¨º¡À?¨¢¨º?¨º¡À??
    timeout.tv_usec = 0;    //ms
    
    while(1){
        FD_ZERO(&working_set); //????working_set???t?¨¨¨º?¡¤??¡¥o?
        memcpy(&working_set, &master_set, sizeof(master_set)); //??master_set?¡¥o?copy¦Ì?working_set?¡¥o?
        
        //select循环监听 这里只对读操作的变化进行监听（working_set为监视读操作描述符所建立的集合）,第三和第四个参数的NULL代表不对写操作、和误操作进行监听
        selectResult = select(select_sd, &working_set, NULL, NULL, &timeout);
        
        // fail
        if (selectResult < 0) {
            perror("select() failed\n");
            exit(1);
        }
        
        // timeout
        if (selectResult == 0) {
            printf("select() timed out.\n");
            continue;
        }
        
        // selectResult > 0 时 开启循环判断有变化的文件描述符为哪个socket
        int i;
        for (i = 0; i < select_sd; i++) {
            //判断变化的文件描述符是否存在于working_set集合
            if (FD_ISSET(i, &working_set)) {
                if (i == proxy_cmd_socket) {
                    printf("\ni=proxy_cmd_socket.\n");

                    if (FD_ISSET(accept_cmd_socket, &master_set)) {
                        close(accept_cmd_socket);
                        FD_CLR(accept_cmd_socket, &master_set);
                    }
                    if (FD_ISSET(connect_cmd_socket, &master_set)) {
                        close(connect_cmd_socket);
                        FD_CLR(connect_cmd_socket, &master_set);
                    }
                    accept_cmd_socket = acceptCmdSocket();  //执行accept操作,建立proxy和客户端之间的控制连接
                    connect_cmd_socket = connectToServer(); //执行connect操作,建立proxy和服务器端之间的控制连接
                    //将新得到的socket加入到master_set结合中
                    FD_SET(accept_cmd_socket, &master_set);
                    FD_SET(connect_cmd_socket, &master_set);

                }//end if
                
                if (i == accept_cmd_socket) {
                   printf("\ni=accept_cmd_socket.\n");
                   char buff[BUFFSIZE]={0};//Ã¿´ÎÇå¿Õ
                   int readLen;
                   
                    if ((readLen=read(i, buff, BUFFSIZE)) == 0) {
                        close(i); //如果接收不到内容,则关闭Socket
                        close(connect_cmd_socket);
                        
                        //socket关闭后，使用FD_CLR将关闭的socket从master_set集合中移去,使得select函数不再监听关闭的socket
                        FD_CLR(i, &master_set);
                        FD_CLR(connect_cmd_socket, &master_set);

                    } else {
                        //如果接收到内容,则对内容进行必要的处理，之后发送给服务器端（写入connect_cmd_socket）

                        //处理客户端发给proxy的request，部分命令需要进行处理，如PORT、RETR、STOR 
                        //PORT
                        if(strncmp(buff,"PORT",4)==0){
                            active=1;
                            int p1,p2;
                            char* delim=",";//¡¤???¡¤?¡Á?¡¤?¡ä?
                            char* p=strtok(buff,delim);//¦Ì¨²¨°?¡ä?¦Ì¡Â¨®?strtok
                            int count=1;
                            while(p!=NULL){//¦Ì¡À¡¤¦Ì???¦Ì2??aNULL¨º¡À¡ê??¨¬D??-?¡¤
                                p=strtok(NULL,delim);//?¨¬D?¦Ì¡Â¨®?strtok¡ê?¡¤??a¨º¡ê??¦Ì?¡Á?¡¤?¡ä?
                                count++;
                                if(count==5){
                                    p1=atoi(p);
                                }
                                if(count==6){
                                    p2=atoi(p);
                                }
                            }
                            dataConnectPort_c=p1*256+p2;//client's data connection port 
                            
                            if (FD_ISSET(proxy_data_socket, &master_set)) {
                                close(proxy_data_socket);
                                FD_CLR(proxy_data_socket, &master_set);
                            }
                            int new_port;
                            if(dataConnectPort_c>50000)
                                new_port=dataConnectPort_c-5000;
                            else
                                new_port=dataConnectPort_c+5000;
                            sprintf(buff, "PORT 192,168,56,101,%d,%d\r\n", (new_port)/256, (new_port)%256);
                            proxy_data_socket = bindAndListenSocket(new_port);        
                            FD_SET(proxy_data_socket, &master_set);
                                                    
                        }
                        
                        //RETR download
                        if(strncmp(buff,"RETR",4)==0){
                            download=1;
                            char buff_new[BUFFSIZE];
                            strcpy(buff_new,buff);
                            strtok(buff_new," \r");
                            fileName=strtok(NULL," \r");
                            char fileBuffer[BUFFSIZE];
                            int fileLen;
                            int fd;

                            //determine whether the file exist in proxy's cache(f:/sharefile)
                            if(fd=open(fileName,O_RDONLY)!=-1){
                                if(active==1){
                                    printf("File %s in cache.(active mode)\n",fileName);
                                    connect_data_socket = connectTo(CLTIP, dataConnectPort_c);//establish the data connection with client
                                    while((fileLen=read(fd,fileBuffer,BUFFSIZE-1))>0){
                                        write(connect_data_socket,fileBuffer,fileLen);
                                    }
                                    close(connect_data_socket);
                                    printf("Download(active) from cache successfully.\n");
                                }//active
                                else{
                                    active=0;
                                    isCached=1;
                                    printf("File %s in cache. Waiting for download(passive).\n",fileName);   
                                }//passive,waiting the client to establish the data connection
                                continue;//don't send the RETR to the server
                            }else{
                                isCached=0;
                                printf("File %s does not exist in cache.\n", fileName);
                            }//file does not exist in cache,find it in server
                        }
                        
                        //STOR  upload
                        if(strncmp(buff,"STOR",4)==0){
                            download=0;       
                        }//end if

                        //PASV
                        if(strncmp(buff,"PASV",4)==0){
                            active=0;
                        }
                        write(connect_cmd_socket,buff,strlen(buff));
                        buff[strlen(buff)]='\0';
                        printf("Transfer CMD: %s to server\n",buff);  
                        
                          
                    }//end if
                }//end if
                
                if (i == connect_cmd_socket) {
                    
                    printf("\ni=connect_cmd_socket.\n");
                    char buff[BUFFSIZE]={0};
                    int readLen;
                   
                    if ((readLen=read(i, buff, BUFFSIZE)) == 0) {
                        close(i); 
                        close(accept_cmd_socket);
                        
                        FD_CLR(i, &master_set);
                        FD_CLR(accept_cmd_socket, &master_set);

                    } else {

                        if(strncmp(buff,"227",3)==0){
                            int p1,p2;
                            active=0;//passive
                            char* delim=",";
                            char* p=strtok(buff,delim);
                            int count=1;
                            while(p!=NULL){
                                p=strtok(NULL,delim);
                                count++;
                                if(count==5){
                                    p1=atoi(p);
                                }
                                if(count==6){
                                    p2=atoi(p);
                                }
                            }
                            dataConnectPort_s=p1*256+p2;//server's data connection port

                            if (FD_ISSET(proxy_data_socket, &master_set)) {
                                close(proxy_data_socket);
                                FD_CLR(proxy_data_socket, &master_set);
                            }

                            int new_port;
                            if(dataConnectPort_s>50000)
                                new_port=dataConnectPort_s-5000;
                            else
                                new_port=dataConnectPort_s+5000;
                            proxy_data_socket = bindAndListenSocket(new_port);
                            FD_SET(proxy_data_socket, &master_set);
                            sprintf(buff, "227 Entering Passive Mode (192,168,56,101,%d,%d)\r\n", (new_port)/256,(new_port)%256);

                        }//end if
                        write(accept_cmd_socket, buff, strlen(buff));
                        buff[strlen(buff)]='\0';
                        printf("Reply CMD: %s to client.\n", buff);

                    }//end if
                }//end if
                
                if (i == proxy_data_socket) {
                    printf("\ni=proxy_data_socket.\n");

                    //active mode
                    if(active==1){
                        printf("Transfer mode: active.\n");
                        accept_data_socket = acceptSocket(proxy_data_socket);                        
                        connect_data_socket = connectTo(CLTIP,dataConnectPort_c);
                        FD_SET(accept_data_socket, &master_set);
                        FD_SET(connect_data_socket, &master_set);
                        printf("Active: client's port %d.\n",dataConnectPort_c);
                    }//active mode

                    //passive mode
                    if(active==0){
                        printf("Transfer mode: passive.\n");
                        accept_data_socket = acceptSocket(proxy_data_socket);//random
                        connect_data_socket = connectTo(SERVIP, dataConnectPort_s);//proxy actively connect to server

                        FD_SET(accept_data_socket, &master_set);
                        FD_SET(connect_data_socket, &master_set);
                        printf("Passive: server's port %d.\n",dataConnectPort_s);

                        if(download==1 && isCached==1){
                            char fileBuffer[BUFFSIZE];
                            int readLen;
                            int fd=open(fileName,O_RDONLY,0);
                            while((readLen=read(fd,fileBuffer,BUFFSIZE-1))>0){
                                write(accept_data_socket,fileBuffer,readLen);
                            }    
                            close(accept_data_socket);
                            printf("download(passive).\n");
                        }else{
                            isCached=0;
                        }

                    }//passive mode                    
                }//end if, proxy_data_socket is just for listening
                
                if (i == accept_data_socket) {
                    printf("\ni=accept_data_socket.\n");
                    char buff[BUFFSIZE]={0};
                    int readLen;

                    printf("download: %d, isCached: %d, filename: %s.\n", download,isCached,fileName);
                   
                    if(download==1 && isCached==0){
                        cache_socket=open(fileName,O_RDWR|O_CREAT,0);
                        if(cache_socket == -1){
                            printf("Open file in cache failed.\n");
                            exit(0);
                        }
                    }

                    while((readLen=read(i, buff, BUFFSIZE-1)) > 0) {
                        
                        write(connect_data_socket,buff,readLen);
                        printf("transfer %d successfully.\n",readLen);    
                        //send file to client

                        if(cache_socket>0){
                            write(cache_socket,buff,readLen);
                        }

                    }
                    
                    if(cache_socket>0){
                        close(cache_socket);
                        printf("store file %s in cache successfully.\n",fileName);
                    }

                    close(i); 
                    close(connect_data_socket);
                        
                    FD_CLR(i, &master_set);
                    FD_CLR(connect_data_socket, &master_set);
                    
                    printf("No data transfer.\n");                          
                }//end if
                
                if (i == connect_data_socket){
                    printf("\ni=connect_data_socket.\n");
                    char buff[BUFFSIZE]={0};
                    int readLen;

                    printf("download: %d, isCached: %d, filename: %s.\n", download,isCached,fileName);

                    if(download==1 && isCached==0){
                        cache_socket=open(fileName,O_RDWR|O_CREAT,0);
                        if(cache_socket == -1){
                            printf("Open file in cache failed.\n");
                            exit(0);
                        }
                    }

                    while((readLen=read(i, buff, BUFFSIZE-1))>0) {
                        
                        write(accept_data_socket,buff,readLen);
                        if(cache_socket>0){
                            write(cache_socket,buff,readLen);
                        }
                    }

                    if(cache_socket>0){
                        close(cache_socket);
                        printf("store file %s in cache successfully.\n",fileName);
                    }

                    close(i); 
                    close(accept_data_socket);
            
                    FD_CLR(i, &master_set);
                    FD_CLR(accept_data_socket, &master_set);

                    printf("No data transfer.\n");         
                }//end if
            }//end if 
        }//end for
    }//end while
    return 0;
}

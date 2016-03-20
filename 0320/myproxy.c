# include <stdio.h>
# include <stdlib.h>
# include <unistd.h>
# include <string.h>
# include <errno.h>
# include <sys/socket.h>
# include <sys/types.h>
# include <netinet/in.h>
# include <dirent.h>
# include <pthread.h>
# include <netdb.h>
 
# define THREADNUM 100
# define REQUEST_SIZE 8192
 
struct arguement {
    int sd;
    int browser_sd;
} __attribute__ ((packed));
 
char ** splitString(char *line, int type){
    char * temp;
    //here 64 can change to other number, in this program its minimum should be 4
    char cutter0[] = "\r\n";
    char cutter1[] = " ";
    char * cutter;
    char ** buf0 = malloc(sizeof(char *) * 32);
    char ** buf1 = malloc(sizeof(char *) * 8);
    char ** tokens;
    if (type == 0 ){
        tokens = buf0;
        cutter = cutter0;
        free(buf1);
    }
    else {
        tokens = buf1;
        cutter = cutter1;
        free(buf0);
    }
     
    temp = strtok(line, cutter);
    if (!tokens){
        fprintf(stderr, "tokens: allocation error\n");
        exit(EXIT_FAILURE);
      }
    int i = 0;
    while(temp != NULL){
        tokens[i] = temp;
        i++;
        temp = strtok(NULL, cutter);
    }
    tokens[i] = NULL;
    return tokens;
}
 
void* workerThread(void* args){
    struct arguement* args_p = (struct arguement*) args;
    struct arguement args2 = *args_p;
    int proxy_sd = args2.sd;
    int browser_sd = args2.browser_sd;
 
    //the following try to receive http request char by char...but failed, it will cause segmentation fault
 
    // char * buffer = (char *)malloc(sizeof(char) * 2);
    // memset(buffer, 0, 2);
    // char * temp = (char *)malloc(sizeof(char) * 1);
    // int bufferSize = 2;
    // while(1){
    //     printf("haha1\n");
    //     memset(temp, 0, 1);
    //     printf("haha2\n");
    //     int result = recv(browser_sd, temp, 1, 0);
    //     strncpy(buffer + bufferSize - 2, temp, 1);
    //     printf("haha3\n");
    //     if(strstr(buffer, "\r\n\r\n") != NULL){
    //         break;
    //     }
    //     bufferSize++;
    //     printf("haha4\n");
    //     buffer = (char *) realloc(buffer, bufferSize);
    //     printf("haha5\n");
    // }
    // temp = NULL;
    // strncpy(buffer + bufferSize - 1, temp, 1);
     
 
    //receive the http request
    char buffer[REQUEST_SIZE]; //= (char *)malloc(sizeof(char) * REQUEST_SIZE);
    memset(buffer, 0, REQUEST_SIZE);
    int result;
    int n = 0;
    while (result = recv(browser_sd, &buffer[n], 1, 0)) { // receive byte by byte
        if (n > 4 && strstr(buffer, "\r\n\r\n") != NULL) {
            break;
        }
        n++;
    }
    buffer[n+1] = '\0';
    //int result = recv(browser_sd, buffer, 1, 0);
    //Later when we split http request, we will change buffer, so we need to make a copy here
    char * requestBuffer = (char *)malloc(sizeof(char) * (result + 1));
    strncpy(requestBuffer, buffer, (result+1));
    if(result < 0){
        close(browser_sd);
        pthread_exit(NULL);
        printf("received fail!\n");
        return 0;
    }
    // printf("~~~~~~~~~\n");
    // printf("%s", buffer);
    // printf("~~~~~~~~~\n");
 
    //check the existence of IFS in http request
    int haveIfs = 0;
    //check the existence of Cache-Control in http request
    int haveCache = 0;
 
    //pointer for important information in http request
    char **firstLine;
    char **hostLine;
    char **cacheLine;
    char **ifsLine;
    char **proxyLine;
    char subdir[256];// = "~csci4430/";
    char ptl[10]; // http1.0 or 1.1
    char requestType[10]; // get or post
    char url[256];
    char hostname[256];
    char objectType[10]; // html, jpg
    
    //split the http request
    char ** lines = splitString(buffer, 0);

    int i = 0;
    while(lines[i] != NULL){
        //get the first line of http request, get the request url
        if (i == 0){
            firstLine = splitString(lines[i], 1);
            //requested object will be in firstLine[1]
            // int j = 0;
            // while(firstLine[j] != NULL){
                // printf("------\n%d\t%s\n", j,firstLine[j]);
                // j++;
            // }
            memset(url, 0, 256);
            strcpy(url, firstLine[1]);
            memset(subdir, 0, 256);
            char *token;
            char temp[256];
            
            if (strstr(url, "//") != NULL){
                token = strtok(url, "//");
                token = strtok(NULL, "//");
            } else {
                token = strtok(url, "/");
            }
            token = strtok(NULL, "/");
            while( token != NULL ) 
            {
                strcat(subdir,"/");
                strcat(subdir,token );
                memset(temp, 0, 256);
                strcpy(temp, token);
                token = strtok(NULL, "/");
            }
            memset(requestType, 0, 10);
            strcpy(requestType, firstLine[0]);
            memset(ptl, 0, 10);
            strcpy(ptl, firstLine[2]);
            if (strstr(temp, ".") != NULL){
                token = strtok(temp, ".");
                token = strtok(NULL, ".");
                memset(objectType, 0, 10);
                strcpy(objectType, token);
            }
            printf("subdir: %s---\n", subdir);
            printf("obj: %s---\n", objectType);
        }
        //get the host name from http request
        if (strstr(lines[i], "Host") != NULL){
            hostLine = splitString(lines[i], 1);
            memset(hostname, 0, 256);
            strcpy(hostname, hostLine[1]);
            //hostname will be in hostLine[1]
            // int j = 0;
            // while(hostLine[j] != NULL){
                // printf("------\n%d\t%s\n",j, hostLine[j]);
                // j++;
            // }
        }
        //check whether http request has a cache-control
        if (strstr(lines[i], "Cache-Control") != NULL){
            haveCache = 1;
            cacheLine = splitString(lines[i], 1);
            // int j = 0;
            // while(cacheLine[j] != NULL){
                // printf("------\n%s\n", cacheLine[j]);
                // j++;
            // }
        }
        //check whether http request has a If-Modified-Since
        if (strstr(lines[i], "If-Modified-Since") != NULL){
            haveIfs = 1;
            ifsLine = splitString(lines[i], 1);
            // int j = 0;
            // while(ifsLine[j] != NULL){
                // printf("------\n%s\n", ifsLine[j]);
                // j++;
            // }
        }
        //check whether http request has a proxy-connection
        if (strstr(lines[i], "Proxy-Connection") != NULL){
            proxyLine = splitString(lines[i], 1);
            // int j = 0;
            // while(proxyLine[j] != NULL){
                // printf("------\n%s\n", proxyLine[j]);
                // j++;
            // }
        }
        i++;
    }
 
    //check whether the request object has already cached on the server
    int existCache = 0;
    //do things here.....
    
    struct hostent *hp;
    struct sockaddr_in addr;
    hp = gethostbyname(hostLine[1]);
    struct in_addr ** addr_list = (struct in_addr **)hp->h_addr_list;
    //printf("ip addr: %s\n", inet_ntoa(*addr_list[0]));
    memset(&addr,0,sizeof(addr));
    addr.sin_family=AF_INET;
    addr.sin_addr.s_addr=inet_addr(inet_ntoa(*addr_list[0]));
    //bcopy((char*)hp->h_addr,(char*)&addr.sin_addr.s_addr,hp->h_length);
    addr.sin_port=htons(80);
    // set a socket to communicate to remote server
    int server_sd = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    // Requested object isn't cached on the proxy, so pass the request to web server.
    if (existCache == 0){
        if(connect(server_sd,(struct sockaddr*)&addr,sizeof(struct sockaddr_in)) < 0){
            printf("Error in connecting to remote server\n");
        } else {
            printf("Connected to %s  IP - %s\n",firstLine[1], inet_ntoa(addr.sin_addr));
            result = send(server_sd, requestBuffer, strlen(requestBuffer), 0);
            printf("forward http request, size: %d\n", strlen(requestBuffer));
        }
        //memset(buffer, 0, REQUEST_SIZE);
        memset(buffer, 0, REQUEST_SIZE);
        if(subdir!=NULL)
            sprintf(buffer,"GET %s %s\r\nHost: %s\r\nConnection: close\r\n\r\n",subdir,ptl,hostname);
        else
            sprintf(buffer,"GET / %s\r\nHost: %s\r\nConnection: close\r\n\r\n",ptl,hostname);
        int res=send(server_sd, buffer, strlen(buffer), 0);
        printf("\n%d\n", strlen(buffer));
        printf("\n%s\n", buffer);
        if(res<0)
            error("Error writing to socket");
        else{
            do {
                memset(buffer, 0, REQUEST_SIZE);
                res = recv(server_sd, buffer, REQUEST_SIZE, 0);
                
                // char statCode[10];
                // int keepAlive = 0;
                // int chunked = 0;
                // int contLen = -1;
                // char filename[23];
                
                // printf("%s\n", buffer);
                // lines = splitString(buffer, 0);
                
                // int i = 0;
                // while(lines[i] != NULL){
                    // printf("%d\t%s\n",i, lines[i]);
                    //get the first line of http request, get the request url
                    // char **line = splitString(lines[i], 1);
                    // if (i == 0 && line[1]){
                        // memset(statCode, 0, 10);
                        // printf("%s\n", line[1]);
                        // strcpy(statCode, line[1]);
                    // }
                    //get the host name from http request
                    // if (strstr(lines[i], "Content-Length") != NULL){
                        // contLen = strtol(line[1], NULL, 10);
                    // }
                    // if (strstr(lines[i], "keep-alive") != NULL){
                        // keepAlive = 1;
                    // }
                    // if (strstr(lines[i], "chunked") != NULL){
                        // chunked = 1;
                    // }
                // }
                if(!(res<=0))
                    send(browser_sd, buffer, res, 0);
            }while(res>0);
        }
        close(server_sd);
        printf("Connection closed\n");
    }
 
    //Requested object already cached on the proxy
    //check IFS and Cache-Control  Also check Proxy-Connection????
 
 
 
    free(lines);
    //free(buffer);
    free(requestBuffer);
    //printf("Haha successful connection!\n");
 
     
    close(browser_sd);
    pthread_exit(NULL);
 
    exit(0);
    return 0;
}
 
int main(int argc, char** argv){
    int sd=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    if (sd < 0) {
        perror("socket()");
        exit(1);
    }
  
    //reusing server port
    long val = 1;
    if(setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(long))==-1){
        perror("setsockopt()");
        exit(1);
    }
  
    struct sockaddr_in proxy_addr;
    struct sockaddr_in browser_addr;
 
    memset(&proxy_addr,0,sizeof(proxy_addr));
    proxy_addr.sin_family=AF_INET;
    proxy_addr.sin_addr.s_addr=htonl(INADDR_ANY);
    proxy_addr.sin_port=htons(atoi(argv[1]));
 
    if(bind(sd,(struct sockaddr *) &proxy_addr,sizeof(proxy_addr))<0){
        printf("bind error: %s (Errno:%d)\n",strerror(errno),errno);
        exit(0);
    }
     
    if(listen(sd,3)<0){
        printf("listen error: %s (Errno:%d)\n",strerror(errno),errno);
        exit(0);
    }
  
    int addr_len=sizeof(browser_addr);
    int browser_sd;
    pthread_t thread[THREADNUM];
    int i = 0;
    while(i < THREADNUM){
        printf("before connection!\n");
        if((browser_sd=accept(sd,(struct sockaddr *) &browser_addr,&addr_len))<0){
            printf("accept erro: %s (Errno:%d)\n",strerror(errno),errno);
            close(browser_sd);
            continue;
        }
        struct arguement args;
        args.sd = sd;
        args.browser_sd = browser_sd;
        pthread_create(&thread[i], NULL, workerThread, &args);
        pthread_detach(thread[i]);
        i++;
    }
    close(sd);
    return 0;
}
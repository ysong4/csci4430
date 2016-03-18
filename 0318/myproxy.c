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
    int client_sd;
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
    int sd = args2.sd;
    int client_sd = args2.client_sd;
 
    //receive the http request
    // char * buffer = (char *)malloc(sizeof(char) * REQUEST_SIZE);
    // memset(buffer, 0, REQUEST_SIZE);
    //int result = recv(client_sd, buffer, REQUEST_SIZE, 0);
    char * buffer = (char *)malloc(sizeof(char) * REQUEST_SIZE);
    memset(buffer, 0, REQUEST_SIZE);
    int result;
    int n = 0;
    while (result = recv(client_sd, &buffer[n], 1, 0)) {
        if (n > 4 && strstr(buffer, "\r\n\r\n") != NULL) {
            break;
        }
        n++;
    }
    buffer[n+1] = '\0';

    //Later when we split http request, we will change buffer, so we need to make a copy here
    char * requestBuffer = (char *)malloc(sizeof(char) * (strlen(buffer)));
    strncpy(requestBuffer, buffer, (strlen(buffer)));
    printf("buffer length: %d\n", strlen(buffer));
    if(result < 0){
        close(client_sd);
        pthread_exit(NULL);
        printf("received fail!\n");
        return 0;
    }
    printf("~~~~~~~~~\n");
    printf("%s", buffer);
    printf("~~~~~~~~~\n");
 
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
            //     printf("------\n%s\n", firstLine[j]);
            //     j++;
            // }
        }
        //get the host name from http request
        if (strstr(lines[i], "Host") != NULL){
            hostLine = splitString(lines[i], 1);
            //hostname will be in hostLine[1]
            // int j = 0;
            // while(hostLine[j] != NULL){
            //     printf("------\n%s\n", hostLine[j]);
            //     j++;
            // }
        }
        //check whether http request has a cache-control
        if (strstr(lines[i], "Cache-Control") != NULL){
            haveCache = 1;
            cacheLine = splitString(lines[i], 1);
            int j = 0;
            while(cacheLine[j] != NULL){
                printf("------\n%s\n", cacheLine[j]);
                j++;
            }
        }
        //check whether http request has a If-Modified-Since
        if (strstr(lines[i], "If-Modified-Since") != NULL){
            haveIfs = 1;
            ifsLine = splitString(lines[i], 1);
            int j = 0;
            while(ifsLine[j] != NULL){
                printf("------\n%s\n", ifsLine[j]);
                j++;
            }
        }
        //check whether http request has a proxy-connection
        if (strstr(lines[i], "Proxy-Connection") != NULL){
            proxyLine = splitString(lines[i], 1);
            int j = 0;
            while(proxyLine[j] != NULL){
                printf("------\n%s\n", proxyLine[j]);
                j++;
            }
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
    addr.sin_port=htons(80);
    // set a socket to communicate to remote server
    int server_sd = socket(AF_INET,SOCK_STREAM,0);
    // Requested object isn't cached on the proxy, so pass the request to web server.
    if (existCache == 0){
        if(connect(server_sd,(struct sockaddr*)&addr,sizeof(struct sockaddr_in)) < 0){
            printf("Error in connecting to remote server\n");
        }
        //forward the http request to server
        result = send(server_sd, requestBuffer, strlen(requestBuffer), 0);
        printf("forward http request, size: %d\n", strlen(requestBuffer));
        //receive the http response from server
        memset(buffer, 0, REQUEST_SIZE);
        result = recv(server_sd, buffer, REQUEST_SIZE, 0);
        printf("++++++++++++++\n");
        printf("%s", buffer);
        printf("++++++++++++++\n");
        close(server_sd);
    }
 
    //Requested object already cached on the proxy
    //check IFS and Cache-Control  Also check Proxy-Connection????
 
 
 
    free(lines);
    free(buffer);
    free(requestBuffer);
    //printf("Haha successful connection!\n");
 
     
    close(client_sd);
    pthread_exit(NULL);
 
    exit(0);
    return 0;
}
 
int main(int argc, char** argv){
    int sd=socket(AF_INET,SOCK_STREAM,0);
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
    int client_sd;
    pthread_t thread[THREADNUM];
    int i = 0;
    while(i < THREADNUM){
        printf("before connection!\n");
        if((client_sd=accept(sd,(struct sockaddr *) &browser_addr,&addr_len))<0){
            printf("accept erro: %s (Errno:%d)\n",strerror(errno),errno);
            close(client_sd);
            continue;
        }
        struct arguement args;
        args.sd = sd;
        args.client_sd = client_sd;
        pthread_create(&thread[i], NULL, workerThread, &args);
        pthread_detach(thread[i]);
        i++;
    }
    close(sd);
    return 0;
}
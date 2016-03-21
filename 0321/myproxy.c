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
# include <sys/stat.h>
# include <crypt.h>


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

    //receive the http request
    char buffer[REQUEST_SIZE]; //= (char *)malloc(sizeof(char) * REQUEST_SIZE);
    char bufferCopy[REQUEST_SIZE];
    int result;
    int n = 0;
    while (result = recv(browser_sd, &buffer[n], 1, 0)) { // receive byte by byte
        if (n > 4 && strstr(buffer, "\r\n\r\n") != NULL) {
            break;
        }
        n++;
    }
    buffer[n+1] = '\0';
    result = n;
        if(result < 0){
            close(browser_sd);
            printf("received fail!\n");
            pthread_exit(NULL);
        }
    //Later when we split http request, we will change buffer, so we need to make a copy here
    // char * requestBuffer = (char *)malloc(sizeof(char) * (result + 1));
    // strncpy(requestBuffer, buffer, (result+1));
    memset(bufferCopy, 0, REQUEST_SIZE);
    strcpy(bufferCopy, buffer);
 
    //check the existence of IMS in http request
    int haveIMS = 0;
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
            memset(url, 0, 256);
            char temp0[256];
            memset(temp0, 0, 256);
            strcpy(url, firstLine[1]);
            strcpy(temp0, firstLine[1]);
            memset(subdir, 0, 256);
            char *token;
            char temp[256];
            
            if (strstr(temp, "//") != NULL){
                token = strtok(temp0, "//");
                token = strtok(NULL, "//");
            } else {
                token = strtok(temp0, "/");
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
        }
        //get the host name from http request
        if (strstr(lines[i], "Host") != NULL){
            hostLine = splitString(lines[i], 1);
            memset(hostname, 0, 256);
            strcpy(hostname, hostLine[1]);
        }
        //check whether http request has a cache-control
        if (strstr(lines[i], "Cache-Control") != NULL){
            haveCache = 1;
            cacheLine = splitString(lines[i], 1);
        }
        //check whether http request has a If-Modified-Since
        if (strstr(lines[i], "If-Modified-Since") != NULL){
            printf("IMS: %s\n", lines[i]);
            haveIMS = 1;
            ifsLine = splitString(lines[i], 1);
        }
        //check whether http request has a proxy-connection
        if (strstr(lines[i], "Proxy-Connection") != NULL){
            proxyLine = splitString(lines[i], 1);
        }
        i++;
    }
    // handle only get
    if(strstr(requestType, "POST") != NULL){
        close(browser_sd);
        printf("POST request not supported!\n");
        pthread_exit(NULL);
    }
 
    //check whether the request object has already cached on the server
    int existCache = 0;
    //do things here.....
    
    struct hostent *hp;
    struct sockaddr_in addr;
    hp = gethostbyname(hostLine[1]);
    struct in_addr ** addr_list = (struct in_addr **)hp->h_addr_list;
    memset(&addr,0,sizeof(addr));
    addr.sin_family=AF_INET;
    addr.sin_addr.s_addr=inet_addr(inet_ntoa(*addr_list[0]));
    addr.sin_port=htons(80);
    // set a socket to communicate to remote server
    int server_sd = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    // Requested object isn't cached on the proxy, so pass the request to web server.
    if (existCache == 0){
        if(connect(server_sd,(struct sockaddr*)&addr,sizeof(struct sockaddr_in)) < 0){
            printf("Error in connecting to remote server\n");
        } else {
            printf("Connected to %s  IP - %s\n",firstLine[1], inet_ntoa(addr.sin_addr));
            //printf("forward http request, size: %d\n", strlen(bufferCopy));
        }
        memset(buffer, 0, REQUEST_SIZE);
        strcpy(buffer, bufferCopy);
        //printf("\n%s\n", buffer);
        int res = send(server_sd, buffer, strlen(buffer), 0);
        //printf("\n%d\n", strlen(buffer));
        if(res<0)
            error("Error writing to socket");
        else{
                memset(buffer, 0, REQUEST_SIZE);
                int n = 0;
                while (res = recv(server_sd, &buffer[n], 1, 0)) { // receive byte by byte
                    if (n > 4 && strstr(buffer, "\r\n\r\n") != NULL) {
                        break;
                    }
                    n++;
                }
                buffer[n+1] = '\0';
                //printf("%s\n", buffer);
                res = n;
                    
                char statCode[10];
                int keepAlive = 0;
                int chunked = 0;
                int contLen = -1;
                char filename[23];
                
                memset(bufferCopy, 0, REQUEST_SIZE);
                strcpy(bufferCopy, buffer);
                lines = splitString(buffer, 0);
                
                int i = 0;
                while(lines[i] != NULL){
                    if (i == 0)
                        printf("%d\t%s\n",i, lines[i]);
                    char **line = splitString(lines[i], 1);
                    if (i == 0 && line[1]){
                        memset(statCode, 0, 10);
                        strcpy(statCode, line[1]);
                    }
                    if (strstr(lines[i], "Content-Length") != NULL){
                        contLen = strtol(line[1], NULL, 10);
                        //printf("%d\t%d\n",i, contLen);
                    }
                    if (strstr(lines[i], "keep-alive") != NULL){
                        keepAlive = 1;
                    }
                    if (strstr(lines[i], "chunked") != NULL){
                        chunked = 1;
                    }
                    i++;
                }
                if (strstr(statCode, "200") != NULL) {
                    // get the file name put into cache
                    //printf("url: %s\n", url);
                    strncpy(filename, (char *)(crypt(url, "$1$00$")+6), 23);
                    for (i=0; i<22; ++i){
                        if (filename[i] == '/'){
                            filename[i] = '_';
                        }
                    }

                    struct stat st = {0};

                    if (stat("./proxyFiles", &st) == -1){
                        mkdir("./proxyFiles", 0777);
                    }

                    char partOne[] = "./proxyFiles/";
                    char * dirName;
                    int fileExist = 0;
                    
                    dirName = malloc((strlen(partOne)+ strlen(filename))* sizeof(char));
                    dirName = strcat(partOne, filename);
                    if (stat(dirName, &st) == -1){
                        fileExist = 0;
                    } else {
                        fileExist = 1;
                        //remove(dirName);
                    }

                    // file header and write it in file, chunked or not
                    char* fileHeader = malloc(512);
                    memset(fileHeader, 0, 512);
                    if (contLen != -1) {
                        sprintf(fileHeader, "HTTP/1.1 200 OK\r\nContent-Length: %d\r\n\r\n", contLen);
                    } else {
                        sprintf(fileHeader, "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n");
                    }
                    // send http response header to browser
                    res = send(browser_sd, bufferCopy, strlen(bufferCopy), 0);
                    // receive web object from server and send to browser
                    char* block = malloc(sizeof(char) * 512);
                    memset(block, 0, 512);
                    FILE *fp = fopen(dirName, "wb");
                    fwrite(fileHeader, sizeof(char), strlen(fileHeader), fp);
                    int blockSize = 0;
                    
                    if(res<0)
                        error("Error writing to socket");
                    else{
                        do {
                            bzero((char*)block,512);
                            blockSize = recv(server_sd,block,512,0);
                            if(!(blockSize<=0))
                                send(browser_sd,block,blockSize,0);
                            int writeSize = fwrite(block, sizeof(char), blockSize, fp);
                            if(writeSize < blockSize)
                            {
                                perror("File write failed on server.\n");
                            }
                            contLen -= blockSize;
                            if (contLen <= 0)
                            {
                                break;
                            }
                        }while(blockSize>0);
                    }
                    fclose(fp);
                    //printf("contLen: %d\n", contLen);
                    // if(blockSize < 0)
                    // {
                        // if (errno == EAGAIN)
                        // {
                            // printf("recv() timed out.\n");
                        // }
                        // else
                        // {
                            // fprintf(stderr, "recv() failed due to errno = %d\n", errno);
                            // exit(1);
                        // }
                    // }
                } else if ((strstr(statCode, "304") != NULL) && haveIMS && haveCache) {
                    // one more check: Compare the IMS time with the update time of file on our proxy cache
                    // if the file on cache is most up to date, then
                    if (1) {
                        printf("\ntest here!\n");
                        strncpy(filename, (char *)(crypt(url, "$1$00$")+6), 23);
                        for (i=0; i<22; ++i){
                            if (filename[i] == '/'){
                                filename[i] = '_';
                            }
                        }
                        struct stat st = {0};
                        if (stat("./proxyFiles", &st) == -1){
                            mkdir("./proxyFiles", 0777);
                        }
                        char partOne[] = "./proxyFiles/";
                        char * dirName;
                        dirName = malloc((strlen(partOne)+ strlen(filename))* sizeof(char));
                        dirName = strcat(partOne, filename);
                        if (stat(dirName, &st) == -1){
                            printf("file not exist on cache!\n");
                        } else {
                            char block[512];
                            FILE *fp = fopen(dirName, "rb");
                            int readSize = 0;
                            do {
                                bzero((char*)block,512);
                                //blockSize = recv(server_sd,block,512,0);
                                int readSize = fread(block, sizeof(char), 512, fp);
                                if(!(readSize<=0))
                                    send(browser_sd, block, readSize, 0);
                            }while(readSize > 0);
                            fclose(fp);
                        }
                    }
                } else {
                    // send http response header to browser
                    res = send(browser_sd, bufferCopy, strlen(bufferCopy), 0);
                    //printf("\n%s\n",buffer);
                    char block[512];
                    if(res<0)
                        error("Error writing to socket");
                    else{
                        do {
                            bzero((char*)block,512);
                            res=recv(server_sd,block,512,0);
                            if(!(res<=0))
                                send(browser_sd,block,res,0);
                        }while(res>0);
                    }
                }
        }
        close(server_sd);
        printf("Connection closed\n");
    }
 
    //Requested object already cached on the proxy
    //check IFS and Cache-Control  Also check Proxy-Connection????

 
    free(lines);
    //free(buffer);
    //free(requestBuffer);
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
        printf("New thread [%d]!\n", i);
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
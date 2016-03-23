# define _XOPEN_SOURCE
# define _GNU_SOURCE
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
# include <stdbool.h> // for boolean
# include <time.h> // for struct tm

# define THREADNUM 100
# define REQUEST_SIZE 8192
 
pthread_mutex_t mutex;

struct arguement {
    int sd;
    int browser_sd;
    int id;
} __attribute__ ((packed));

time_t get_mtime(const char *path)
{
    struct stat statbuf;
    if (stat(path, &statbuf) == -1) {
        perror(path);
        exit(1);
    }
    return statbuf.st_mtime;
}
 
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

bool compareName(char* fileName, char* inDate){
    struct tm *tm;
    struct tm tm1;
    struct tm tm2;
    time_t time1;
    time_t time2;
    double seconds;
    struct stat st = {0};
    char datestring[256];

    stat(fileName, &st);

    tm = gmtime(&st.st_mtime);
    strftime(datestring, sizeof(datestring), "%a, %d %b %Y %X %Z", tm); 

    //printf("%s\n",datestring);
    tm1 = *tm;
    strptime(inDate, "%a, %d %b %Y %X %Z", &tm2);
    //printf("%s\n", inDate);

    time1 = mktime(&tm1);
    time2 = mktime(&tm2);

    seconds = difftime(time1, time2);

    //printf("%f\n", seconds);

    if (seconds >= 0){
        printf("200\n");
        return true;
    }
    else{
        printf("304\n");
        return false;
    }
}
 
void* workerThread(void* args){
    struct arguement* args_p = (struct arguement*) args;
    struct arguement args2 = *args_p;
    int proxy_sd = args2.sd;
    int browser_sd = args2.browser_sd;
    int id = args2.id;
    char buffer[REQUEST_SIZE]; //= (char *)malloc(sizeof(char) * REQUEST_SIZE);
    char bufferCopy[REQUEST_SIZE];
    char bufferAnother[REQUEST_SIZE];
    char newRequestBuffer[REQUEST_SIZE];

    int checkServerConnection = 0;
    int server_sd;

    while(1){
        
        //receive the http request
        int result;
        memset(buffer, 0, REQUEST_SIZE);
        result = recv(browser_sd, buffer, REQUEST_SIZE, 0);
        if (result == 0){
            continue;
        }
        printf("received http request, size: %d\n", result);
        if(result < 0){
            close(browser_sd);
            printf("received fail!\n");
            pthread_exit(NULL);
        }
        memset(bufferCopy, 0, REQUEST_SIZE);
        strcpy(bufferCopy, buffer);
        strcpy(bufferAnother, buffer);
        printf("that's the request i got: id is [%d]\n%s", id, buffer);
     
        //check the existence of IMS in http request
        int haveIMS = 0;
        //check the existence of no-cache in http request
        int haveCache = 0;
        //check the existence of keep-alive in http request
        int haveKeepAlive = 0;
     
        //pointer for important information in http request
        char **firstLine;
        char **hostLine;
        char **cacheLine;
        char **imsLine;
        char requestType[10]; // get or post
        char url[256];
        char hostname[256];
        char IMS[256];// If-modified-since
        int supportedFileType = 0;
        
        //split the http request
        char ** lines = splitString(buffer, 0);

        int i = 0;
        while(lines[i] != NULL){
            //get the first line of http request, get the request url
            if (i == 0){
                firstLine = splitString(lines[i], 1);
                //requested object will be in firstLine[1]
                memset(requestType, 0, 10);
                strcpy(requestType, firstLine[0]);
                memset(url, 0, 256);
                strcpy(url, firstLine[1]);
                if(strcasestr(firstLine[1], "html") != NULL || strcasestr(firstLine[1], "jpg") != NULL || 
                    strcasestr(firstLine[1], "gif") != NULL || strcasestr(firstLine[1], "txt") != NULL || 
                    strcasestr(firstLine[1], "pdf") != NULL || strcasestr(firstLine[1], "jpeg") != NULL) {
                    supportedFileType = 1;
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
                if (strstr(lines[i], "no-cache") != NULL){
                    haveCache = 1;
                }
            }
            //check whether http request has a If-Modified-Since
            if (strstr(lines[i], "If-Modified-Since") != NULL){
                printf("IMS: %s\n", lines[i]);
                haveIMS = 1;
                imsLine = splitString(lines[i], 1);
                memset(IMS, 0, 256);
                strcpy(IMS, imsLine[1]);
            }
            //check whether http request has a proxy-connection
            if (strstr(lines[i], "Proxy-Connection") != NULL){
                if (strstr(lines[i], "keep-alive") != NULL){
                    haveKeepAlive = 1;
                }
            }
            i++;
        }
        // handle only get
        if(strstr(requestType, "POST") != NULL){
            printf("POST request not supported!\n");
            continue;
            //close(browser_sd);
            //pthread_exit(NULL);
        }

        //check whether the request object has already cached on the server
        int existCache = 0;
        char filename[23];
        strncpy(filename, crypt(url, "$1$00$")+6, 23);
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
        char *dirName;
        dirName = malloc((strlen(partOne)+ strlen(filename))* sizeof(char));
        dirName = strcat(partOne, filename);
        if (stat(dirName, &st) == -1){
            existCache = 0;
        }else{
        existCache = 1;
        }
        // int existCache = 0;
        // char *dirName;
        
        // the request have already cached on the proxy
        int caseType = 0;
        if(existCache == 1 && supportedFileType == 1){
            printf("+++++++++++++yoyo! already cached!\n");
            if(haveIMS == 0 && haveCache == 0){
                // send back the web object.
                // find the cached object, find out its size
                caseType = 1;
                printf("~~~~~~Now the first case.\n");
                struct stat st = {0};
                if (stat(dirName, &st) == -1){
                    printf("file not exist on cache!\n");
                } else {
                    char block[512];
                    pthread_mutex_lock(&mutex);
                    FILE *fp = fopen(dirName, "rb");
                    int readSize = 0;
                    do {
                        memset(block, 0, 512);
                        readSize = fread(block, sizeof(char), 512, fp);
                        if(!(readSize<=0))
                            send(browser_sd, block, readSize, MSG_NOSIGNAL);
                    }while(readSize > 0);
                    fclose(fp);
                    pthread_mutex_unlock(&mutex);
                }
                continue;

            }
            if(haveIMS == 1 && haveCache == 0){
                caseType = 2;
                printf("~~~~~~Now the second case.\n");
                //bool checkNeedModified = compareName(dirName, IMS);
                bool checkNeedModified = false;
                if (checkNeedModified == true){
                    char block[512];
                    pthread_mutex_lock(&mutex);
                    FILE *fp = fopen(dirName, "rb");
                    int readSize = 0;
                    do {
                        memset(block, 0, 512);
                        readSize = fread(block, sizeof(char), 512, fp);
                        if(!(readSize<=0))
                            send(browser_sd, block, readSize, MSG_NOSIGNAL);
                    }while(readSize > 0);
                    fclose(fp);
                    pthread_mutex_unlock(&mutex);
                }else{
                    char responseHeader[256] = "HTTP/1.1 304 Not Modified\r\n\r\n";
                    send(browser_sd, responseHeader, 256, MSG_NOSIGNAL);
                    printf("haha+++++++++++++finished sending 304 response!\n");
                }
                continue;            }
            if(haveIMS == 0 && haveCache == 1){
                caseType = 3;
                char ** headerlines = splitString(bufferAnother, 0);
            
                struct tm* mytm;
                char mydatestring[256];
                char IMSconcat[256] = "If-Modified-Since: ";
                char * tmpline;

                stat(dirName, &st);
                mytm = gmtime(&st.st_mtime);
                strftime(mydatestring, sizeof(mydatestring), "%a, %d %b %Y %X %Z", mytm); 
                tmpline = strcat(IMSconcat, mydatestring);
                int b = 1;

                memset(newRequestBuffer, 0, REQUEST_SIZE);
                strcpy(newRequestBuffer, headerlines[0]);
                strcat(newRequestBuffer, "\r\n");

                while(headerlines[b] != NULL){
                    strcat(newRequestBuffer, headerlines[b]);
                    strcat(newRequestBuffer, "\r\n");
                    b++;
                }
                strcat(newRequestBuffer, tmpline);
                strcat(newRequestBuffer, "\r\n");
                strcat(newRequestBuffer, "\r\n");


                printf("%s", newRequestBuffer);
                strcpy(bufferCopy, newRequestBuffer);
                existCache = 0;
                printf("~~~~~~Now the third case.\n");
            }
            if(haveIMS == 1 && haveCache == 1){
                caseType = 4;
                printf("~~~~~~Now the fourth case.\n");
                continue;
            }
        }else{
            printf("+++++++++++++sosad! no cache!\n");

        }

        
        
        // Requested object isn't cached on the proxy, so pass the request to web server.
        if (existCache == 0){
            if (checkServerConnection == 0){
                struct hostent *hp;
                struct sockaddr_in addr;
                hp = gethostbyname(hostLine[1]);
                struct in_addr ** addr_list = (struct in_addr **)hp->h_addr_list;
                memset(&addr,0,sizeof(addr));
                addr.sin_family=AF_INET;
                addr.sin_addr.s_addr=inet_addr(inet_ntoa(*addr_list[0]));
                addr.sin_port=htons(80);
                // set a socket to communicate to remote server
                server_sd = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
                // if (server_sd < 0) {
                    // perror("socket()");
                    // exit(1);
                // }
                if(connect(server_sd,(struct sockaddr*)&addr,sizeof(struct sockaddr_in)) < 0){
                    printf("Error in connecting to remote server\n");
                } else {
                    checkServerConnection = 1;
                    printf("Connected to %s  IP - %s\n",firstLine[1], inet_ntoa(addr.sin_addr));
                    //printf("forward http request, size: %d\n", strlen(bufferCopy));
                }
            }
            memset(buffer, 0, REQUEST_SIZE);
            strcpy(buffer, bufferCopy);
            //printf("\n%s\n", buffer);
            int res = send(server_sd, buffer, strlen(buffer), MSG_NOSIGNAL);
            //printf("\n%d\n", strlen(buffer));
            if(res<0){
                perror("Error in sending HTTP request to server");
            }
            else{
                memset(buffer, 0, REQUEST_SIZE);
                int n = 0;
                while (1) { // receive byte by byte
                    res = recv(server_sd, &buffer[n], 1, 0);
                    if (res < 0){
                        perror("Error in receiving HTTP response!");
                        break;
                    }
                    if (n > 4 && strstr(buffer, "\r\n\r\n") != NULL) {
                        break;
                    }
                    n++;
                }
                buffer[n+1] = '\0';
                //printf("%s\n", buffer);
                res = n;
                    
                char statCode[10];
                int haveClose = 0;
                int chunked = 0;
                int contLen = -1;
                char filename[23];
                int supportedFileType = 0;
                int uptodate = 0;
                
                memset(bufferCopy, 0, REQUEST_SIZE);
                strcpy(bufferCopy, buffer);
                lines = splitString(buffer, 0);
                printf("id[%d]+++++this is response++++\n%s", id, bufferCopy);
                
                //printf("+++++this is response++++\n");
                int i = 0;
                while(lines[i] != NULL){
                    
                    if (i == 0){
                    printf("%d\t%s\n",i, lines[i]);
                        char **line = splitString(lines[i], 1);
                        memset(statCode, 0, 10);
                        strcpy(statCode, line[1]);
                    }
                    if (strstr(lines[i], "Content-Length") != NULL){
                        char **line = splitString(lines[i], 1);
                        contLen = strtol(line[1], NULL, 10);
                        //printf("%d\t%d\n",i, contLen);
                    }
                    if (strstr(lines[i], "Connection") != NULL){
                        if(strstr(lines[i], "close") != NULL){
                            haveClose = 1;
                        }
                    }
                    if (strstr(lines[i], "Proxy-Connection") != NULL){
                        if(strstr(lines[i], "close") != NULL){
                            haveClose = 1;
                        }
                    }
                    if (strstr(lines[i], "chunked") != NULL){
                        chunked = 1;
                    }
                    if (strstr(lines[i], "Content-Type") != NULL){
                        if(strcasestr(lines[i], "html") != NULL || strcasestr(lines[i], "jpg") != NULL || 
                            strcasestr(lines[i], "gif") != NULL || strcasestr(lines[i], "text") != NULL || 
                            strcasestr(lines[i], "pdf") != NULL || strcasestr(lines[i], "jpeg") != NULL) {
                            supportedFileType = 1;
                        } else {
                            supportedFileType = 0;
                        }
                    }
                    i++;
                }
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
                //int fileExist = 0;
                dirName = malloc((strlen(partOne)+ strlen(filename))* sizeof(char));
                dirName = strcat(partOne, filename);
                // Compare the IMS time with the modified time of file on our proxy cache
                if (stat(dirName, &st) != -1){
                    time_t file = get_mtime(dirName);
                    //char* ims = "Sat, 02 Jan 2016 07:19:53 GMT";
                    struct tm imsTime;
                    strptime(IMS, "%A, %d %B %Y %H:%M:%S", &imsTime);
                    time_t imss = mktime(&imsTime);
                    double seconds = difftime(file, imss);
                    if (seconds > 0) {
                        uptodate = 1;
                    }
                }
                if (strstr(statCode, "200") != NULL && supportedFileType) {
                    // get the file name and put into cache
                    //printf("url: %s\n", url);
                    // if (stat(dirName, &st) == -1){
                        // fileExist = 0;
                    // } else {
                        // fileExist = 1;
                    // }

                    // file header and write it in file, chunked or not
                    char* fileHeader = malloc(REQUEST_SIZE);
                    memset(fileHeader, 0, REQUEST_SIZE);
                    if (contLen != -1) {
                        sprintf(fileHeader, bufferCopy);
                    } else {
                        sprintf(fileHeader, bufferCopy);
                    }
                    // send http response header to browser
                    res = send(browser_sd, bufferCopy, strlen(bufferCopy), MSG_NOSIGNAL);
                    // receive web object from server and send to browser
                    char* block = malloc(sizeof(char) * 512);
                    memset(block, 0, 512);
                    //lock the mutex, write the cache object here
                    pthread_mutex_lock(&mutex);
                    FILE *fp = fopen(dirName, "wb");
                    fwrite(fileHeader, sizeof(char), strlen(fileHeader), fp);
                    int blockSize = 0;
                    
                    if(res<0)
                        perror("Error writing to socket");
                    else{
                        do {
                            memset(block, 0, 512);
                            blockSize = recv(server_sd,block,512,0);
                            if(blockSize >= 0)
                                send(browser_sd,block,blockSize,MSG_NOSIGNAL);
                            int writeSize = fwrite(block, sizeof(char), blockSize, fp);
                            if(writeSize < blockSize)
                            {
                                perror("File write failed on server.\n");
                            }
                            //printf("contLen: %d\n", contLen);
                            if (contLen != -1) {
                                contLen -= blockSize;
                                if (contLen <= 0)
                                    break;
                            }else { 
                                if(strstr(block, "\r\n0\r\n\r\n") != NULL || blockSize < 14)
                                    break;
                            }
                        }while(blockSize >= 0);
                    }
                    //printf("after content loop: %d\n", contLen);
                    fclose(fp);
                    pthread_mutex_unlock(&mutex);
                } else if ((strstr(statCode, "304") != NULL) && supportedFileType && haveIMS && haveCache && uptodate) {
                    // Compare the IMS time with the update time of file on our proxy cache
                    // if the file on cache is most up to date, then
                    if (stat(dirName, &st) == -1){
                        printf("file not exist on cache!\n");
                    } else {
                        pthread_mutex_lock(&mutex);
                        char block[512];
                        FILE *fp = fopen(dirName, "rb");
                        int readSize = 0;
                        do {
                            memset(block, 0, 512);
                            readSize = fread(block, sizeof(char), 512, fp);
                            if(!(readSize<=0))
                                send(browser_sd, block, readSize, MSG_NOSIGNAL);
                        }while(readSize > 0);
                        fclose(fp);
                        pthread_mutex_unlock(&mutex);
                    }
                } else { // transfer data from server directly to browser
                    // send http response header to browser
                    res = send(browser_sd, bufferCopy, strlen(bufferCopy), MSG_NOSIGNAL);
                    //printf("\n%s\n",buffer);
                    char block[512];
                    if(res<0)
                        perror("Error writing to socket");
                    else{
                        do {
                            memset(block, 0, 512);
                            res=recv(server_sd,block,512,0);
                            if(!(res<=0))
                                send(browser_sd,block,res,MSG_NOSIGNAL);
                        }while(res>0);
                    }
                }
                // 304 break both two connection
                if (strstr(statCode, "304") != NULL) {
                    printf("close both browser and server connection\n");
                    close(server_sd);
                    break;
                }
                if (haveClose == 1){
                    printf("close server connection, maintain browser connection\n");
                    close(server_sd);
                    continue;
                }
                else{
                    printf("maintain two connection\n");
                    checkServerConnection = 1;
                    continue;
                }
            }
        }
    }
        
    // printf("Connection closed\n");
    close(browser_sd);
    pthread_exit(NULL);
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
        args.id = i;
        pthread_create(&thread[i], NULL, workerThread, &args);
        pthread_detach(thread[i]);
        i++;
    }
    close(sd);
    return 0;
}
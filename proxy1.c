#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <resolv.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include<unistd.h>
#include<netdb.h> //hostent
#include<arpa/inet.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "myqueue.h"

#define NUM_THREADS 8

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_t thread_pool[NUM_THREADS]; // reference to threads
int telemetry;

int hostname_to_ip(char * , char *);
int check_against_blacklist(char *uri, char **lines, size_t num_lines);
// A structure to maintain client fd, and server ip address and port address
// client will establish connection to server using given IP and port
struct serverInfo {
    int client_fd;
    char uri[100];
    char ip[100];
    char port[100];
    int version;
    char* msg;
    int is_init_connection;
};


char** str_split(char* a_str, const char a_delim) {
    char** result    = 0;
    size_t count     = 0;
    char* tmp        = a_str;
    char* last_comma = 0;
    char delim[2];
    delim[0] = a_delim;
    delim[1] = 0;

    /* Count how many elements will be extracted. */
    while (*tmp) {
        if (a_delim == *tmp) {
            count++;
            last_comma = tmp;
        }
        tmp++;
    }

    /* Add space for trailing token. */
    count += last_comma < (a_str + strlen(a_str) - 1);

    /* Add space for terminating null string so caller
    knows where the list of returned strings ends. */
    count++;

    result = malloc(sizeof(char*) * count);

    if (result) {
        size_t idx  = 0;
        char* token = strtok(a_str, delim);

        while (token) {
            //assert(idx < count);
            *(result + idx++) = strdup(token);
            token = strtok(0, delim);
        }
        //assert(idx == count - 1);
        *(result + idx) = 0;
    }

    return result;
}


// A thread function
// A thread for each client request
void *runSocket(struct serverInfo *info) {
    char bufferS[4096];
    char bufferC[4096];
    int bytes = 0;
    int bytesS = 0;
    int bytesC = 0;
    unsigned long int byteTotal = 0;
    int count = 0;
    clock_t start, end;
    double totalTime = 0;
    char msg1[] = "HTTP/1.1 200 Connection established\r\n\r\n";
    char msg0[] = "HTTP/1.0 200 Connection established\r\n\r\n";

    start = clock();
    //printf("client:%d\n",info->client_fd);
    //fputs(info->ip,stdout);
    //fputs(info->port,stdout);
    //code to connect to main server via this proxy server
    int server_fd = 0;
    struct sockaddr_in server_sd;
    // create a socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(server_fd < 0) {
        //printf("server socket not created\n");
    }
    //printf("server socket created\n");
    memset(&server_sd, 0, sizeof(server_sd));
    // set socket variables
    server_sd.sin_family = AF_INET;
    server_sd.sin_port = htons(atoi(info->port));
    server_sd.sin_addr.s_addr = inet_addr(info->ip);
    //connect to main server from this proxy server
    if((connect(server_fd, (struct sockaddr *)&server_sd, sizeof(server_sd)))<0) {
        // printf("server connection not established\n");
        end = clock();
        totalTime = ((double) (end - start)) / CLOCKS_PER_SEC;
        if (telemetry) {
            printf("Hostname: %s, Size: %lu bytes, Time: %f sec\n", info->uri, byteTotal, totalTime);
        }
        close(info->client_fd);
        close(server_fd);
    }
    // printf("server socket connected\n");

    if (info->version == 0) {
        write(info->client_fd, msg0, sizeof(msg0));
    } else {
        write(info->client_fd, msg1, sizeof(msg1));
    }

    struct timeval tv;
    int max = info->client_fd + 1;
    if (server_fd >= max) {
        max = server_fd + 1;
    }
    //printf("client: %d, server: %d, max: %d\n", info->client_fd, server_fd, max);
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    fd_set allSockets, readySockets;
    FD_ZERO(&allSockets);
    FD_ZERO(&readySockets);
    FD_SET(info->client_fd, &allSockets);
    FD_SET(server_fd, &allSockets);

    memset(&bufferS, '\0', sizeof(bufferS));
    memset(&bufferC, '\0', sizeof(bufferC));
    while(count < 5) {

        bytes = 0;

        count++;
        readySockets = allSockets;
        tv.tv_sec = count;
        // The select function blocks the calling process until there is activity on any of the
        // specified sets of file descriptors, or until the timeout (tv) period has expired.
        int x = select(max, &readySockets, NULL, NULL, &tv);

        //printf("select: %d\n", x);

        if (x > 0) {
            //memset(&buffer, '\0', sizeof(buffer));
            count = 0;
            if (FD_ISSET(info->client_fd, &readySockets)) {
                bytes = read(info->client_fd, bufferC + bytesC, sizeof(bufferC) - bytesC);
                if(bytes <= 0) {
                    close(info->client_fd);
                    close(server_fd);
                } else {
                    bytesC += bytes;
                    byteTotal += bytes;
                    // send data to main server
                    bytes = write(server_fd, bufferC, bytesC);
                    if (bytes <= 0) {

                    } else if (bytes != bytesC) {
                        memmove(bufferC, bufferC + bytes, bytesC - bytes);
                    } else {
                        memset(&bufferC, '\0', sizeof(bufferC));
                    }
                    bytesC -= bytes;
                    //printf("client fd is : %d\n",c_fd);s
                    // printf("From client :\n");
                    //fputs(buffer,stdout);
                    //fflush(stdout);
                }
            }

            if (FD_ISSET(server_fd, &readySockets)) {
                bytes = read(server_fd, bufferS + bytesS, sizeof(bufferS) - bytesS);
                if(bytes <= 0) {
                    close(info->client_fd);
                    close(server_fd);
                } else {
                    bytesS += bytes;
                    byteTotal += bytes;
                    // send response back to client
                    bytes = write(info->client_fd, bufferS, bytesS);
                    if (bytes <= 0) {

                    } else if (bytes != bytesC) {
                        memmove(bufferS, bufferS + bytes, bytesS - bytes);
                    } else {
                        memset(&bufferS, '\0', sizeof(bufferS));
                    }
                    bytesS -= bytes;
                    // printf("From server :\n");
                    //fputs(buffer,stdout);
                }
            }
        }
    }

    end = clock();
    totalTime = ((double) (end - start)) / CLOCKS_PER_SEC;
    if (telemetry) {
        printf("Hostname: %s, Size: %lu bytes, Time: %f sec\n", info->uri, byteTotal, totalTime);
    }

    close(info->client_fd);
    close(server_fd);

    free(info);
}

void *runSocketHTTP(struct serverInfo *info) {
    char buffer[65535];
    int bytes = 0;
    int count = 0;
    //printf("client:%d\n",info->client_fd);
    //fputs(info->ip,stdout);
    //fputs(info->port,stdout);
    //code to connect to main server via this proxy server
    int server_fd = 0;
    struct sockaddr_in server_sd;
    // create a socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(server_fd < 0) {
        //printf("server socket not created\n");
    }
    //printf("server socket created\n");
    memset(&server_sd, 0, sizeof(server_sd));
    // set socket variables
    server_sd.sin_family = AF_INET;
    server_sd.sin_port = htons(atoi(info->port));
    server_sd.sin_addr.s_addr = inet_addr(info->ip);
    //connect to main server from this proxy server
    if((connect(server_fd, (struct sockaddr *)&server_sd, sizeof(server_sd)))<0) {
        // printf("server connection not established\n");
        close(info->client_fd);
        close(server_fd);
    }
    // printf("server socket connected\n");
    write(server_fd, info->msg, sizeof(info->msg));
    struct timeval tv;
    long int elapsed_time;
    int max = info->client_fd + 1;
    if (server_fd >= max) {
        max = server_fd + 1;
    }
    //printf("client: %d, server: %d, max: %d\n", info->client_fd, server_fd, max);
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    fd_set allSockets, readySockets;
    FD_ZERO(&allSockets);
    FD_ZERO(&readySockets);
    FD_SET(info->client_fd, &allSockets);
    FD_SET(server_fd, &allSockets);
    while(count < 5) {

        count++;
        readySockets = allSockets;
        tv.tv_sec = count;
        int x = select(max, &readySockets, NULL, NULL, &tv);

        // printf("select: %d\n", x);

        if (x >= 0) {
            memset(&buffer, '\0', sizeof(buffer));
            
            if (FD_ISSET(info->client_fd, &readySockets)) {
                bytes = read(info->client_fd, buffer, sizeof(buffer));
                if(bytes < 0) {
                    close(info->client_fd);
                    close(server_fd);
                } else if (bytes == 0) {

                } else {
                    count = 0;
                    // send data to main server
                    write(server_fd, buffer, sizeof(buffer));
                    //printf("client fd is : %d\n",c_fd);
                    // printf("From client :\n");
                    // fputs(buffer,stdout);
                    fflush(stdout);
                }
            }

            if (FD_ISSET(server_fd, &readySockets)) {
                bytes = read(server_fd, buffer, sizeof(buffer));
                if(bytes < 0) {
                    close(info->client_fd);
                    close(server_fd);
                } else if (bytes == 0) {

                } else {
                    count = 0;
                    // send response back to client
                    write(info->client_fd, buffer, sizeof(buffer));
                    // printf("From server :\n");
                    // fputs(buffer,stdout);
                }
            }
        }
    }

    close(info->client_fd);
    close(server_fd);
    free(info);
}



void *thread_function(void *arg) {
    while (1) {
        pthread_mutex_lock(&mutex);
        struct serverInfo *info = dequeue();
        pthread_mutex_unlock(&mutex);
        if (info != NULL) {
            // connection
            if (info->is_init_connection) {
                runSocket(info);
            } else {
                runSocketHTTP(info);
            }
        }
    }
}


// main entry point  
int main(int argc,char *argv[]) {
    pthread_t tid;
    char port[100],ip[100], uri[100];
    char *hostname = argv[1];
    char proxy_port[100];
    char telemetry_str[100];
    char buffer[65535];
    char backup[65535];
    char keyword[32];
    char version[32];
    char** lines;
    char** firstLine;
    char** host;

    for (int i = 0; i < NUM_THREADS; ++i) {
        pthread_create(&thread_pool[i], NULL, thread_function, NULL);
    }

    strcpy(proxy_port,argv[1]); // proxy port
    printf("proxy port is %s\n",proxy_port);

    strcpy(telemetry_str, argv[2]); // telemetry?
    printf("Telemetry is %s\n", telemetry_str);
    telemetry = atoi(telemetry_str);
    if (telemetry != 0 && telemetry != 1) {
	perror("Telemetry should be either 0 or 1");
	exit(EXIT_FAILURE);
    }

    

    /* read blacklist */
    char buf[1024], **host_names = NULL;
    size_t nptrs = 2, used = 0;
    FILE *fp = fopen(argv[3], "r");
    if (!fp) {
        perror("file open failed\n");
        exit(EXIT_FAILURE);
    }

    if ((host_names = malloc(nptrs * sizeof *host_names)) == NULL) {
        perror("malloc-host_names failed\n");
        exit(EXIT_FAILURE);
    }

    while (fgets(buf, 1024, fp)) {                 /* read each line into buf */
        size_t len;
        buf[(len = strcspn(buf, "\n"))] = 0;       /* trim new line and save length */

        if (used == nptrs) {                       /* check if realloc of host_names is needed */
            void *tmp = realloc(host_names, (2 * nptrs) * sizeof *host_names);
            if (!tmp) {
                perror("realloc-host_names failed\n");
                break;
            }
            host_names = tmp;
            nptrs *= 2;
        }

        if (!(host_names[used] = malloc(len + 1))) {
            perror("malloc-host_names[used] failed\n");
            break;
        }
        memcpy(host_names[used], buf, len + 1);          /* Copy line from buf to host_names arr */
        used += 1;
    }

    fclose(fp);
    printf("Blacklist file ready\n");

    //socket variables
    int proxy_fd =0, client_fd=0;
    struct sockaddr_in proxy_sd;
    // add this line only if server exits when client exits
    signal(SIGPIPE,SIG_IGN);
    // create a socket
    if((proxy_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\nFailed to create socket\n");
    }
    printf("Proxy created\n");
    memset(&proxy_sd, 0, sizeof(proxy_sd));
    // set socket variables
    proxy_sd.sin_family = AF_INET;
    proxy_sd.sin_port = htons(atoi(proxy_port));
    proxy_sd.sin_addr.s_addr = INADDR_ANY;
    // bind the socket
    if((bind(proxy_fd, (struct sockaddr*)&proxy_sd,sizeof(proxy_sd))) < 0) {
        printf("Failed to bind a socket\n");
    }
    // start listening to the port for new connections
    if((listen(proxy_fd, 2)) < 0) {
        printf("Failed to listen\n");
    }
    printf("waiting for connection..\n");
    //accept all client connections continuously
    while(1) {
        client_fd = accept(proxy_fd, (struct sockaddr*)NULL ,NULL);
        // printf("client no. %d connected\n",client_fd);
        if(client_fd > 0) {
            int n = read(client_fd, buffer, 65535);
            strcpy(backup, buffer);
            // printf("received:\n%s\n", buffer);
            lines = str_split(buffer, '\n');
            firstLine = str_split(lines[0], ' ');
            strcpy(keyword, firstLine[0]);
            strcpy(version, firstLine[2]);
            host = str_split(firstLine[1], ':');
            strcpy(uri, host[0]);
            strcpy(port, host[1]);

            strcpy(buffer, backup);
            lines = str_split(buffer, '\n');

            /* check if uri is in blacklist */
            if (check_against_blacklist(uri, host_names, used)) {
                close(client_fd);
                continue;
            }

            if (lines) {
                for (int i = 0; *(lines + i); i++) {
                    
                    char line[1024];
                    strcpy(line, lines[i]);
                    
                    firstLine = str_split(line, ' ');
                    //printf("test:\n%s\n", firstLine[1]);
                    if (strcmp(firstLine[0], "Host:") == 0) {
                        if (strcmp(keyword, "CONNECT") == 0) {
                            host = str_split(firstLine[1], ':');
                            strcpy(uri, host[0]);
                            strcpy(port, host[1]);
                        } else {
                            strcpy(uri, firstLine[1]);
                            strcpy(port, "80");
                        }
                        
                        break;
                    }
                }
            }

            

            // printf("uri:    %s\n", uri);

            hostname_to_ip(uri, ip);

            //std::string str(buffer);
            //multithreading variables
            struct serverInfo *item = malloc(sizeof(struct serverInfo));
            item->client_fd = client_fd;
            strcpy(item->ip,ip);
            strcpy(item->port,port);
            strcpy(item->uri,uri);
            item->version = strcmp(version, "HTTP/1.0");
            item->is_init_connection = 0;

            pthread_mutex_lock(&mutex);
            if (strcmp(keyword, "CONNECT") == 0) {
                item->is_init_connection = 1;
                enqueue(item);
            } else {
                (item->msg) = malloc(sizeof(backup));
                strcpy(item->msg, backup);
                enqueue(item);
            }
            pthread_mutex_unlock(&mutex);
        }
    }

    // cleanup
    for (size_t l = 0; l < used; ++l) {
        free(host_names[l]);
    }
    free(host_names);

    printf("Proxy shutting down!\n");
    return 0;
}

int check_against_blacklist(char *uri, char **lines, size_t num_lines) {
    for (size_t i = 0; i < num_lines; ++i) {
        char *hostname = lines[i];
        if ((strstr(uri, hostname) != NULL) || (strstr(hostname, uri) != NULL))
            return 1;
    }
    return 0;
}

int hostname_to_ip(char * hostname , char* ip) {
struct hostent *he;
struct in_addr **addr_list;
int i;
if ( (he = gethostbyname( hostname ) ) == NULL) {
    // get the host info
    herror("gethostbyname");
    return 1;
}
addr_list = (struct in_addr **) he->h_addr_list;
for(i = 0; addr_list[i] != NULL; i++) {
    //Return the first one;  
    strcpy(ip , inet_ntoa(*addr_list[i]) );
    return 0;
}
    return 1;
}


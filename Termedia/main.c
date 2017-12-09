//
//  main.c
//  Termedia
//
//  Created by Kelvin Zhang on 12/5/17.
//  Copyright © 2017 Kelvin Zhang. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <ctype.h>
#include <stdbool.h>
#include <malloc/malloc.h>
#include <assert.h>
#include <pthread.h>

#define QueueLength 10
#define max(a, b) (a>b?a:b)
#define SCR_WIDTH 80

typedef struct{
    int capacity;
    int i;
    char *data;
    int duration;
} Frame;


void processClient(int, Frame*, int);
bool isNumeric(const char *str);
void addToFrame(Frame *fp, const char *line, int len);
void printSockAddr(struct sockaddr_in *sa);


void loadFrames(const char *path, Frame **frameList, int *len)
{
    FILE *fp = fopen(path, "r");
    
    int i = -1;
    int capacity = 4;
    Frame *frames = malloc(capacity * sizeof(Frame));
    memset(frames, 0, capacity * sizeof(Frame));
    
    while(!feof(fp))
    {
        char *line = NULL;
        size_t linecap = 0;
        size_t read = getline(&line, &linecap, fp);
        if(isNumeric(line))
        {
            i++;
            if(i >= capacity)
            {
                //double the buffer size
                capacity <<= 1;
                Frame *tmp = realloc(frames, capacity*sizeof(Frame));
                if(tmp == NULL)
                {
                    perror("malloc");
                    exit(0);
                }
                frames = tmp;
            }
            if(i >= 1)
            {
                char *tmp = realloc(frames[i-1].data, frames[i-1].i);
                if(tmp == NULL)
                {
                    perror("malloc");
                    exit(0);
                }
                frames[i-1].data = tmp;
            }
            frames[i].duration = atoi(line);
            frames[i].data = NULL;
            frames[i].i = 0;
        }else
        {
            addToFrame(frames+i, line, (int)read);
        }
        free(line);
    }
    fclose(fp);
    *frameList = frames;
    *len = i+1;
}

int utf8len(const char *s)
{
    int len = 0;
    for (; *s; ++s) if ((*s & 0xC0) != 0x80) len++;
    return len;
}

void addToFrame(Frame *fp, const char *line, int len)
{
    int charCount = (int)utf8len(line);
    int pad = SCR_WIDTH - charCount - 1;
    int bytes = len + pad;
    
    assert(charCount < SCR_WIDTH);
    if(fp->data == NULL)
    {
        //default buffer size
        fp->capacity = bytes+1;
        fp->i = 0;
        fp->data = malloc(fp->capacity);
    }else if(fp->i + bytes >= fp->capacity)
    {
        fp->capacity = fp->i + bytes+1;
        char *tmp = realloc(fp->data, fp->capacity);
        if(tmp == NULL)
        {
            perror("malloc");
            exit(0);
        }
        fp->data = tmp;
    }
    int rounded = fp->i + bytes;
    
    //append the data to frame buffer
    strcpy(fp->data + fp->i, line);
    
    //pad the buffer with space chars to
    fp->data[rounded] = 0;
    fp->data[rounded-1] = '\n';
    
    memset(fp->data + fp->i + len-1, ' ', rounded - fp->i - len);
    fp->i = rounded;
}

bool isNumeric(const char *str)
{
    if(*str == 0 || *str == '\n')return false;
    while(*str && *str != '\n')
    {
        if(!isdigit(*str++))return false;
    }
    return true;
}

void freeFrames(Frame *frames)
{
    free(frames);
}

int main(int argc, const char * argv[])
{
    signal(SIGPIPE, SIG_IGN);
    signal(SIGCHLD, SIG_IGN);
    
    Frame *frames = NULL;
    int len = 0;
    loadFrames(argv[1], &frames, &len);
    
    int port = 2333;
    
    // Set the IP address and port for this server
    struct sockaddr_in serverIPAddress;
    memset(&serverIPAddress, 0, sizeof(serverIPAddress));
    serverIPAddress.sin_family = AF_INET;
    serverIPAddress.sin_addr.s_addr = INADDR_ANY;
    serverIPAddress.sin_port = htons(port);
    
    int masterSocket =  socket(PF_INET, SOCK_STREAM, 0);
    if (masterSocket < 0)
    {
        perror("socket");
        exit(-1);
    }
    
    // Set socket options to reuse port. Otherwise we will
    // have to wait about 2 minutes before reusing the same port number
    int optval = 1;
    setsockopt(masterSocket, SOL_SOCKET, SO_REUSEADDR,
               (char *) &optval, sizeof(int));
    
    // Bind the socket to the IP address and port
    int error = bind( masterSocket,
                     (struct sockaddr *)&serverIPAddress,
                     sizeof(serverIPAddress));
    if (error)
    {
        perror("bind");
        exit(-1);
    }
    
    // Put socket in listening mode and set the
    // size of the queue of unprocessed connections
    error = listen(masterSocket, QueueLength);
    if (error)
    {
        perror("listen");
        exit( -1 );
    }
    
    while(1)
    {
        // Accept incoming connections
        struct sockaddr_in clientIPAddress;
        int alen = sizeof( clientIPAddress );
        int slaveSocket = accept( masterSocket,
                                 (struct sockaddr *)&clientIPAddress,
                                 (socklen_t*)&alen);
        
        if (slaveSocket < 0)
        {
            perror( "accept");
            exit(-1);
        }
        printSockAddr(&clientIPAddress);
        if(fork() == 0)
        {
            processClient(slaveSocket, frames, len);
            exit(0);
        }
        close(slaveSocket);
    }
    return 0;
}

void printSockAddr(struct sockaddr_in *sa)
{
    char buffer[20];
    
    inet_ntop(AF_INET, &sa->sin_addr, buffer, sizeof(buffer));
    printf("%s: %d\n", buffer, sa->sin_port);
}

#define CLEARSCRN "\x1B[2J"
#define JMPHOME "\x1B[H"

void processClient(int fd, Frame *frames, int len)
{
    write(fd, CLEARSCRN, sizeof(CLEARSCRN)-1);
    
    for(int i = 0; i < len; i ++)
    {
        if(write(fd, JMPHOME, sizeof(JMPHOME)-1) <= 0)return;
        if(write(fd, frames[i].data, frames[i].i) <= 0)return;
        for(int j = frames[i].duration; j > 0; j--)
        {
            usleep(66666);
        }
    }
}

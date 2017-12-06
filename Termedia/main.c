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
#include <netinet/in.h>

#define QueueLength 10
void processClient(int);


int main(int argc, const char * argv[])
{
    signal(SIGPIPE, SIG_IGN);
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
        processClient(slaveSocket);
        close(slaveSocket);
    }
    return 0;
}

#define CLEARSCRN "\x1B[2J"
#define JMPHOME "\x1B[H"

void processClient(int fd)
{
    write(fd, CLEARSCRN, sizeof(CLEARSCRN)-1);
    FILE *fp = fdopen(dup(fd), "a+");
    
    for(int i = 0; i < 100; i ++)
    {
        if(fprintf(fp, "%s%d\n", JMPHOME, i) < 0)return;
        fflush(fp);
        usleep(1000 * 200);
    }
    fclose(fp);
}

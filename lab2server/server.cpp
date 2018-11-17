/* 
 * File:   server.cpp
 * Author: anileeli
 *
 * Created on November 12, 2018, 8:45 PM
 */

#include <cstdlib>
#include <string>
#include <sstream>
#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

// Inserted into data when sending the list of clients and sessions to a client
#define CLIENT_LIST_STRING "Clients Online:"
#define SESSION_LIST_STRING "Available Clients:"

#define BACKLOG 10       // How many pending connections queue will hold
#define MAXDATASIZE 1380 // Max number of bytes we can get at once 

using namespace std;

// Defines control packet types
enum msgType
{
    LOGIN,
    LO_ACK,
    LO_NAK,
    EXIT,
    JOIN,
    JN_ACK,
    JN_NAK,
    LEAVE_SESS,
    NEW_SESS,
    NS_ACK,
    MESSAGE,
    QUERY,
    QU_ACK
};


// Message structure to be serialized when sending messages
struct message
{
    unsigned int type;
    unsigned int size;
    string source;
    string data;
};


// Get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*) sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*) sa)->sin6_addr);
}



// Create a packet string from a message structure
string stringifyMessage(const struct message* data)
{
    string dataStr = to_string(data->type) + " " + to_string(data->size) 
                     + " " + data->source + " " + data->data;
    return dataStr;
}


// Sends a message to client in the following format:
//   message = "<type> <data_size> <source> <data>"
// Returns true if message is successfully sent
bool sendToClient(struct message *data, int sockfd)
{
    int numBytes;
    string dataStr = stringifyMessage(data);

    if(dataStr.length() > MAXDATASIZE) return false;
    if((numBytes = send(sockfd, dataStr.c_str(), dataStr.length(), 0)) == -1)
    {
        perror("send");
        return false;
    }
    return true;
}


// Creates socket that listens for new connections and returns the file descriptor
int createListenerSocket(const char* portNum)
{
    int listener;     // listening socket descriptor
    int yes=1;        // for setsockopt() SO_REUSEADDR, below
    int rv;
    
    struct addrinfo hints, *ai, *p;
    
    // get us a socket and bind it
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    
    if ((rv = getaddrinfo(NULL, portNum, &hints, &ai)) != 0)
    {
        fprintf(stderr, "server: %s\n", gai_strerror(rv));
        exit(1);
    }
    
    for(p = ai; p != NULL; p = p->ai_next)
    {
        listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (listener < 0) continue;
        
        // lose the pesky "address already in use" error message
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

        if (bind(listener, p->ai_addr, p->ai_addrlen) < 0)
        {
            close(listener);
            continue;
        }

        break;
    }

    // if we got here, it means we didn't get bound
    if (p == NULL) {
        fprintf(stderr, "selectserver: failed to bind\n");
        exit(2);
    }

    freeaddrinfo(ai); // all done with this

    // listen
    if (listen(listener, BACKLOG) == -1) {
        perror("listen");
        exit(3);
    }
    
    return listener;
}


// Send an acknowledge to a client if their login is successful
void acknowledgeLogin(int sockfd)
{
    struct message loginAck;
    loginAck.type = LO_ACK;
    loginAck.size = 0;
    loginAck.source = "SERVER";
    loginAck.data = "";
    
    sendToClient(&loginAck, sockfd);
}


int main(int argc, char** argv) {

    fd_set master;    // Master file descriptor list
    fd_set read_fds;  // Temp file descriptor list for select()
    int fdmax;        // Maximum file descriptor number

    char buf[MAXDATASIZE];    // Buffer for client data
    char remoteIP[INET6_ADDRSTRLEN];

    int listener = createListenerSocket(argv[1]);
    
    // Clear master and temp sets and add the listener socket to master
    FD_ZERO(&master);
    FD_ZERO(&read_fds);
    FD_SET(listener, &master);

    // Keep track of the biggest file descriptor
    fdmax = listener;

    // Main loop
    while(1)
    {
        read_fds = master; // copy master list
        if (select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1)
        {
            perror("select");
            exit(4);
        }

        // Run through the existing connections looking for data to read
        for(int i = 0; i <= fdmax; i++)
        {
            if (FD_ISSET(i, &read_fds)) // Part of the tracked file descriptors
            { 
                if (i == listener) // Handle new connections
                {
                    struct sockaddr_storage remoteaddr; // client address
                    socklen_t addrlen = sizeof(remoteaddr);
                    int newfd = accept(listener, (struct sockaddr *)&remoteaddr, &addrlen);

                    if (newfd == -1) perror("accept");
                    else
                    {
                        FD_SET(newfd, &master); // add to master set
                        if (newfd > fdmax) fdmax = newfd;
                        
                        acknowledgeLogin(newfd);
                        
                        printf("server: new connection from %s on socket %d\n",
                            inet_ntop(remoteaddr.ss_family,
                                get_in_addr((struct sockaddr*)&remoteaddr),
                                remoteIP, INET6_ADDRSTRLEN),
                            newfd);
                    }
                }
                
                else // Handle data from a client
                {
                    int nbytes;
                    if ((nbytes = recv(i, buf, sizeof(buf), 0)) <= 0)
                    {
                        // Got error or connection closed by client
                        if (nbytes == 0) // Connection closed
                        {
                            printf("server: socket %d hung up\n", i);
                        }
                        else perror("recv");
                        
                        close(i);
                        FD_CLR(i, &master); // remove from master set
                    }
                    
                    else // We got some data from a client
                    {
//                        for(int j = 0; j <= fdmax; j++) // Send to everyone!
//                        {
//                            if (FD_ISSET(j, &master)) // Except the listener and ourselves
//                            {
//                                if (j != listener && j != i)
//                                {
//                                    if (send(j, buf, nbytes, 0) == -1)
//                                    {
//                                        perror("send");
//                                    }
//                                }
//                            }
//                        }
                    }
                } // END handle data from client
            } // END got new incoming connection
        } // END looping through file descriptors
    } // END while

    return 0;
}
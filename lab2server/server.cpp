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
#include <unordered_map>
#include <unordered_set>

// Inserted into data when sending the list of clients and sessions to a client
#define CLIENT_LIST_STRING "Clients Online:"
#define SESSION_LIST_STRING "Available Clients:"

#define BACKLOG 10       // How many pending connections queue will hold
#define MAXDATASIZE 1380 // Max number of bytes we can get at once 

using namespace std;

// Defines control packet types
enum msgType {
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
    NS_NACK,
    MESSAGE,
    QUERY,
    QU_ACK
};


// Message structure to be serialized when sending messages
struct message {
    unsigned int type;
    unsigned int size;
    string source;
    string data;
};


// Key is file descriptor, value is client username and password
unordered_map<int, pair<string, string>> clientList;

// Key is session name, value is set of file descriptors describing clients
// connected to the session
unordered_map<string, unordered_set<int>> sessionList;


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


// Creates a message structure from a packet (string)
struct message messageFromPacket(const char* buf)
{
    string buffer(buf);
    stringstream ss(buffer);
    struct message packet;
    ss >> packet.type >> packet.size >> packet.source;
    if(!(ss >> packet.data)) packet.data = "NoData";
    return packet;
}


// Sends a message to client in the following format:
//   message = "<type> <data_size> <source> <data>"
// Returns true if message is successfully sent
bool sendToClient(struct message *data, int sockfd)
{
    int numBytes;
    string dataStr = stringifyMessage(data);

    if(sizeof(dataStr) > MAXDATASIZE) return false;
    if((numBytes = send(sockfd, dataStr.c_str(), sizeof(dataStr), 0)) == -1)
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


// Logs a client described by a file descriptor into the server
// Returns true if successful
// TODO Check if the client is double logging in
bool loginClient(int sockfd)
{
    char buffer[MAXDATASIZE];
    int numBytes;
    struct message loginInfo;
    
    if((numBytes = recv(sockfd, buffer, MAXDATASIZE, 0)) == -1)
    {
        perror("recv");
        return false;
    }
    else
    {
        string s(buffer);
        stringstream ss(s);
        ss >> loginInfo.type >> loginInfo.size
           >> loginInfo.source >> loginInfo.data;
    }
    
    clientList.insert(make_pair(sockfd, make_pair(loginInfo.source, loginInfo.data)));
    acknowledgeLogin(sockfd);
    return true;
}


// Create a new session in the session list and initialize it with the 
// requesting client. If it already exists, send back NACk
bool createSession(int sockfd, struct message packet)
{   
    struct message ack;
    ack.size = packet.size;
    ack.source = "SERVER";
    ack.data = packet.data;
    
    // Insert returns a pair describing if the insertion was successful
    auto res = sessionList.insert(make_pair(packet.data, unordered_set<int>({sockfd})));
    if(res.second == false)
    {
        ack.type = NS_NACK;
        sendToClient(&ack, sockfd);
        return false;
    }
    else
    {
        ack.type = NS_ACK;
        sendToClient(&ack, sockfd);
        return true;
    }
    
}


int main(int argc, char** argv)
{
    fd_set master;    // Master file descriptor list
    fd_set read_fds;  // Temp file descriptor list for select()
    int fdmax;        // Maximum file descriptor number

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
                        if(loginClient(newfd) == true)
                        {
                            FD_SET(newfd, &master); // add to master set
                            if (newfd > fdmax) fdmax = newfd;

                            printf("server: new connection from %s on socket %d\n",
                                inet_ntop(remoteaddr.ss_family,
                                    get_in_addr((struct sockaddr*)&remoteaddr),
                                    remoteIP, INET6_ADDRSTRLEN),
                                    newfd);
                        }
                    }             
                }
                
                else // Handle other commands from client
                {
                    int nbytes;
                    char buf[MAXDATASIZE];

                    if ((nbytes = recv(i, buf, sizeof(buf), 0)) <= 0)
                    {
                        // Got error or connection closed by client
                        if (nbytes == 0) // Connection closed
                        {
                            printf("server: socket %d hung up\n", i);
                            clientList.erase(i);
                        }
                        else perror("recv");
                        
                        close(i);
                        FD_CLR(i, &master); // remove from master set
                    }
                    
                    else // We got some data from a client
                    {
                        struct message packet = messageFromPacket(buf);
                        
                        switch(packet.type)
                        {
//                            case JOIN:
////                                JN_ACK
////                                JN_NAK
//                            case LEAVE_SESS:
                            case NEW_SESS:
                                if(createSession(i, packet) == true)
                                {
                                    cout << "New session created for client "
                                         << packet.source << endl;
                                }
                                else
                                {
                                    cout << "Session cannot be created" << endl;
                                }
                                
//                                for(auto it = sessionList.begin(); it != sessionList.end(); it++)
//                                {
//                                    cout << it->first << " has connected clients: ";
//                                    for(auto it2 = it->second.begin(); it2 != it->second.end(); it2++)
//                                    {
//                                        cout << *it2 << ", ";
//                                    }
//                                    cout << endl;
//                                }
                                
                                break;
//                            case MESSAGE:
//                            case QUERY:
//                                QU_ACK
                            default:
                                cout << "Unknown packet type" << endl;
                                break;
                        }
                        
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
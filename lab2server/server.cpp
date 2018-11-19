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

#define SESSION_NOT_FOUND "No session found!"
#define ACK_DATA "NoData"

#define BACKLOG 10       // How many pending connections queue will hold
#define MAXDATASIZE 1380 // Max number of bytes we can get at once 

using namespace std;

// Defines control packet types
enum msgType {
    LOGIN,
    LO_ACK,
    LO_NAK,
    LO_DUP,
    EXIT,
    JOIN,
    JN_ACK,
    JN_NAK,
    LEAVE_SESS,
    LS_ACK,
    LS_NAK,
    NEW_SESS,
    NS_ACK,
    NS_NAK,
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


// Creates socket that listens for new connections and returns the file descriptor
int createListenerSocket(const char* portNum)
{
    int listener;     // listening socket descriptor
    int yes=1;        // for setsockopt() SO_REUSEADDR, below
    int rv;
    
    struct addrinfo hints, *ai, *p;
    
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
    if(!getline(ss, packet.data)) packet.data = ACK_DATA;
    return packet;
}


// Return the sessionID that the sockfd is connected to
// Returns "NoSessionFound" if session could not be found
string clientSockfdToSessionID (int sockfd)
{
    for (auto session = sessionList.begin(); session != sessionList.end(); session++)
    {
        // Check if the given client is connected to this session
        auto client = session->second.find(sockfd);
        if(client != session->second.end()) return session->first;
    }

    // Session could not be found 
    return SESSION_NOT_FOUND;
}


// Sends a message to client in the following format:
//   message = "<type> <data_size> <source> <data>"
// Returns true if message is successfully sent
bool sendToClient(struct message *data, int sockfd)
{
    int numBytes;
    string dataStr = stringifyMessage(data);

    cout << dataStr << endl;
    
    if(dataStr.length() + 1 > MAXDATASIZE) return false;
    if((numBytes = send(sockfd, dataStr.c_str(), dataStr.length() + 1, 0)) == -1)
    {
        perror("send");
        return false;
    }
    return true;
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

void acknowledgeDuplicateLogin(int sockfd)
{
    struct message loginDupAck;
    loginDupAck.type = LO_DUP;
    loginDupAck.size = 0;
    loginDupAck.source = "SERVER";
    loginDupAck.data = "";
    
    sendToClient(&loginDupAck, sockfd);
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

    for (auto it : clientList) {
        //possibility of duplicate login
        if (loginInfo.source == it.second.first) {
            //if(loginInfo.data == it.second.second)
            acknowledgeDuplicateLogin(sockfd);
            return false;
        }
    }
    
    clientList.insert(make_pair(sockfd, make_pair(loginInfo.source, loginInfo.data)));
    acknowledgeLogin(sockfd);
    return true;
}


// Adds client to the specified session
// If the session exists and they aren't already in a session, it sends back the
// session they were added to
// Otherwise, it sends back the reason they couldn't be added to the specified session
// Returns true if successful
bool joinSession (int sockfd, string sessionID)
{
    struct message ack;
    ack.source = "SERVER";
\
    // Find list of clients connected to the given session name
    auto session = sessionList.find(sessionID);
    
    // Find session the client is connected to (if any)
    string currentSessionID = clientSockfdToSessionID(sockfd);
    
    // Checking that session exists and client is not already in a session
    if (sessionID != ACK_DATA &&
        currentSessionID == SESSION_NOT_FOUND &&
        session != sessionList.end())
    {        
        // Add client to the session
        session->second.insert(sockfd);
        
        // Send response with the data as the sessionID
        ack.type = JN_ACK;
        ack.data = sessionID;
        ack.size = ack.data.length() + 1;
        
        sendToClient(&ack, sockfd);
        return true;
    }
    
    // Session does not exist or client is already in a session
    else 
    {
        ack.type = JN_NAK;
        
        if (sessionID == ACK_DATA) ack.data = "No session ID was provided!";
        else if(currentSessionID != SESSION_NOT_FOUND) ack.data = "Already in a session!";
        else ack.data = "Session not found!";

        ack.size = ack.data.length() + 1;
        
        sendToClient(&ack, sockfd);
        return false;
    }
}


// Removes client from their current session.
// If they're in a session, it sends back the session they were removed from
// Otherwise, it sends back the reason they couldn't leave the specified session
// Returns true if successful
bool leaveSession (int sockfd)
{
    struct message ack;
    ack.source = "SERVER";
\
    string currentSessionID = clientSockfdToSessionID(sockfd);
    
    // Check if client is in a session
    if (currentSessionID != SESSION_NOT_FOUND)
    {
        // Remove client from session
        auto currentSession = sessionList.find(currentSessionID);
        currentSession->second.erase(sockfd);
        if(currentSession->second.empty()) // No more clients in the session
        {
            sessionList.erase(currentSessionID);
        }
        
        ack.type = LS_ACK;
        ack.data = currentSessionID;
        ack.size = ack.data.length() + 1;

        sendToClient(&ack, sockfd);
        return true;
    }
    else
    {
        ack.type = LS_NAK;
        ack.data = "Not in a session!";
        ack.size = ack.data.length() + 1;
        
        sendToClient(&ack, sockfd);
        return false;
    }
}


// Create a new session in the session list and add the requesting client to it
// If the session doesn't exist, it creates it and adds the client to it, and
// sends back the sessionID
// Otherwise, it sends back the reason why it couldn't be created
// Returns true if successful
bool createSession(int sockfd, string sessionID)
{   
    struct message ack;
    ack.source = "SERVER";
    
    if(clientSockfdToSessionID(sockfd) != SESSION_NOT_FOUND)
    {
        ack.type = NS_NAK;
        ack.data = "Already in a session!";
        ack.size = ack.data.length() + 1;
        
        sendToClient(&ack, sockfd);
        return false;
    }
    
    // Insert returns a pair describing if the insertion was successful
    auto res = sessionList.insert(make_pair(sessionID, unordered_set<int>({sockfd})));
    if(res.second == false)
    {
        ack.type = NS_NAK;
        ack.data = "Session already exists!";
        ack.size = ack.data.length() + 1;
        
        sendToClient(&ack, sockfd);
        return false;
    }
    else if (sessionID == ACK_DATA)
    {
        ack.type = NS_NAK;
        ack.data = "No session ID was provided!";
        ack.size = ack.data.length() + 1;
        
        sendToClient(&ack, sockfd);
        return false;  
    }
    
    else
    {
        ack.type = NS_ACK;
        ack.data = sessionID;
        ack.size = ack.data.length() + 1;
        
        sendToClient(&ack, sockfd);
        return true;
    }
}


// Send an acknowledge to a client for requesting a list
void acknowledgeList(int sockfd, string buffer)
{
    struct message listAck;
    listAck.type = QU_ACK;
    listAck.size = buffer.length() + 1;
    listAck.source = "SERVER";
    listAck.data = buffer;
    
    sendToClient(&listAck, sockfd);
}


void createList(int sockfd)
{
    string buffer = "Clients Online: ";
    
    for(auto it : clientList){
        buffer += it.second.first + " ";
    }
    
    buffer += "Available Sessions: ";
    for(auto it : sessionList){
        buffer += it.first + " ";
    }
    
    acknowledgeList(sockfd, buffer);
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

                    if ((nbytes = recv(i, buf, MAXDATASIZE, 0)) <= 0)
                    {
                        // Got error or connection closed by client
                        if (nbytes == 0) // Connection closed
                        {
                            printf("server: socket %d hung up\n", i);
                            clientList.erase(i);
                            
                            string sessionID = clientSockfdToSessionID(i);
                            if(sessionID != SESSION_NOT_FOUND)
                            {
                                auto session = sessionList.find(sessionID);
                                session->second.erase(i);
                                if(session->second.empty())
                                {
                                    sessionList.erase(session);
                                }
                            }
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
                            case JOIN:
                                if(joinSession(i, packet.data))
                                {
                                    cout << "Client '" << packet.source << "' joined session '" 
                                         << packet.data  << "'" << endl;
                                }
                                else
                                {
                                    cout << "Client '" << packet.source << "' could not join session '" 
                                         << packet.data << "'" << endl;
                                }
                                break;
                                
                                
                            case LEAVE_SESS:
                                if (leaveSession(i))
                                {
                                    cout << "Client '" << packet.source << "' has left session" << endl;
                                         
                                }
                                else
                                {
                                    cout << "Client '" << packet.source << "' is not in a session" 
                                         << endl;
                                }
                                break;
                                
                            case NEW_SESS:
                                if(createSession(i, packet.data))
                                {
                                    cout << "New session '" << packet.data << "' created for client "
                                         << packet.source << endl;
                                }
                                else
                                {
                                    cout << "Session '" << packet.data << "' cannot be created" 
                                         << endl;
                                }     
                                break;
                            case MESSAGE:
                            {
                                // Get list of clients connected in the session with the sender
                                string sessionID = clientSockfdToSessionID(i);
                                unordered_set<int> session;
                                if(sessionID != SESSION_NOT_FOUND)
                                {
                                    session = sessionList.find(sessionID)->second;
                                }
                                
                                // Send message to all clients in the session (excluding the sender)
                                for(auto const & clientSockfd : session)
                                {
                                    if(clientSockfd != i) sendToClient(&packet, clientSockfd);
                                }
                                
                                cout << "Message sent to session '" << sessionID << "'" << endl;
                                break;
                            }
                            case QUERY:
                                createList(i);
                                break;
                            default:
                                cout << "Unknown packet type" << endl;
                                break;
                        }
                    }
                } // END handle data from client
            } // END got new incoming connection
        } // END looping through file descriptors
    } // END while

    return 0;
}

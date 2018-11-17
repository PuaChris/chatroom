/* 
 * File:   client.cpp
 * Author: anileeli
 *
 * Created on November 12, 2018, 8:46 PM
 */


/* TODO
 *
 * - Disconnect clients when server closes
 * - Work on logging out
 * - Work on quit whether or not the user is logged in
 * - Double logging in
 * - Parsing issues (e.g. incorrect number of arguments)
 * 
 */

#include <cstdlib>
#include <string>
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <signal.h>
#include <arpa/inet.h>

#define CMD_LOGIN      "/login"
#define CMD_LOGOUT     "/logout\n"
#define CMD_JOINSESS   "/joinsession"
#define CMD_LEAVESESS  "/leavesession\n"
#define CMD_CREATESESS "/createsession"
#define CMD_LIST       "/list\n"
#define CMD_QUIT       "/quit\n"

// Inserted into data when sending the list of clients and sessions to a client
#define CLIENT_LIST_STRING "Clients Online:"
#define SESSION_LIST_STRING "Available Clients:"

#define MAXDATASIZE 1380 // max number of bytes we can get at once

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
    MESSAGE,
    QUERY,
    QU_ACK
};


// Message structure to be serialized when sending messages
// Note: when message is stringified, the delimiter between fields is " "
struct message {
    unsigned int type;
    unsigned int size;
    string source;
    string data;
};


// Contains connection information about the client and server
struct connectionDetails {
    string clientID;
    string clientPassword;
    string serverIP;
    string serverPort;
};


// Get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa) {
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


// Sends a message to server in the following format:
//   message = "<type> <data_size> <source> <data>"
// Returns true if message is successfully sent
bool sendToServer(struct message *data, int sockfd)
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


// Sends login info to server and checks server's response
// Returns true if login is successful
bool requestLogin(struct connectionDetails login, int sockfd)
{
    char buffer[MAXDATASIZE];
    int numBytes, response;
    struct message info;
    info.type = LOGIN;
    info.size = login.clientPassword.length();
    info.source = login.clientID;
    info.data = login.clientPassword;
    
    // Sends login request to server
    if(!sendToServer(&info, sockfd)){
        return false;
    }
    
    // Server response
    if((numBytes = recv(sockfd, buffer, MAXDATASIZE, 0)) == -1)
    {
        perror("recv");
        return false;
    }
    
    // Checking packet type
    string s(buffer);
    stringstream ss(s);
    ss >> response;
    
    if(response != LO_ACK)
    {
        cout << "Login failed!" << endl;
        return false;
    }
    return true;
}


// Sends the logout request to the server
void logout(struct connectionDetails login, int sockfd)
{
    struct message info;
    info.type = EXIT;
    info.size = 0;
    info.source = login.clientID;
    info.data = "";
    
    if(sendToServer(&info, sockfd)) cout << "Logout successful!" << endl;
}


// This prints out a list of connected clients and available sessions from server 
void printClientSessionList(string buffer)
{
    // Get rid of type, data_size, and source, leaving just the list of clients and sessions
    stringstream ss(buffer);
    string temp;
    ss >> temp >> temp >> temp;

    // Printing list of clients and sessions
    string data;
    while(ss >> data)
    {
        if(data == SESSION_LIST_STRING || data == CLIENT_LIST_STRING) cout << endl;
        cout << data << endl;
    }
    cout << endl;
}


// Sends a request to return the list of active clients and available sessions
// Returns true if the list is successfully received
bool requestClientSessionList(struct connectionDetails login, int sockfd)
{
    int numBytes, response;
    char buffer[MAXDATASIZE];
    
    // Prepare message
    struct message info;
    info.type = QUERY;
    info.size = 0;
    info.source = login.clientID;
    info.data = "";
    
    if(!sendToServer(&info, sockfd)){
        cout << "List unavailable!" << endl;
        return false;
    }
    
    // Server response
    if((numBytes = recv(sockfd, buffer, MAXDATASIZE, 0)) == -1)
    {
        perror("recv");
        return false;
    }
    
    // Checking packet type
    string s(buffer);
    stringstream ss(s);
    ss >> response;
    
    if(response != QU_ACK)
    {
        cout << "Login failed!" << endl;
        return false;
    }
    
    // List received, print clients and sessions
    else
    {
        printClientSessionList(s);
        return true;
    }
}


// Creates connection with server and returns socket file descriptor that
// describes the connection
int createConnection(struct connectionDetails login) {
    
    int newSockFD, rv;
    struct addrinfo hints, *servinfo, *p;
    char s[INET6_ADDRSTRLEN];
    
    // If invalid/incomplete login information provided
    if(login.clientID.empty() == true || login.clientPassword.empty() == true ||
       login.serverIP.empty() == true || login.serverPort.empty() == true)
    {
        cout << "Invalid login info!" << endl;
    }
    
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(login.serverIP.c_str(), login.serverPort.c_str(), &hints, &servinfo)) != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return -1;
    }

    // Loop through all the results and connect to the first we can
    for (p = servinfo; p != NULL; p = p->ai_next)
    {
        if ((newSockFD = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
        {
            perror("client: socket");
            continue;
        }

        if (connect(newSockFD, p->ai_addr, p->ai_addrlen) == -1)
        {
            close(newSockFD);
            perror("client: connect");
            continue;
        }
        break;
    }

    if (p == NULL)
    {
        fprintf(stderr, "client: failed to connect\n");
        return -1;
    }

    //Retrieves IP address from the server that is currently being connected to
    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *) p->ai_addr),
            s, sizeof s);
    printf("client: connecting to %s\n", s);

    return newSockFD;
}


int main(int argc, char** argv) {

    int sockfd;
    string buffer;
    struct connectionDetails login;
    bool loggedIn = false;

    if (argc != 1)
    {
        fprintf(stderr, "usage: client\n");
        exit(1);
    }

    /********************** GET LOGIN/CONNECTION INFO *************************/

    cout << "Please enter login information in the following format:\n"
            "/login <client_id> <password> <server-IP> <server-port>\n" << endl;

    while(1)
    {
        // Create stringstream to extract login input from user
        string input, command;
        getline(cin, input);
        stringstream ss(input);

        cout << "Read: '" + input + "'" << endl;
        
        ss >> command;
        
        cout << endl;
        cout << "Command entered: " + command << endl;

        if (command == "/login" && !loggedIn)
        {
            // Get login information
            ss >> login.clientID >> login.clientPassword
               >> login.serverIP >> login.serverPort;
            
            // Create connection and get file descriptor
            sockfd = createConnection(login);
            
            if(sockfd != -1)
            { 
                if(requestLogin(login, sockfd))
                {
                    cout<< "Login successful!"<<endl;
                    loggedIn = true;
                }

            }
            
        }
        else if(!loggedIn)
        {
            cout << "Please login." << endl;
        }
        else if (command == "/joinsession")
        {
            
        }
        else if (command == "/leavesession")
        {
            
        }
        else if (command == "/createsession")
        {
            
        }
        else if (command == "/list")
        {
            
        }
        else if(command == "/logout" || command == "/quit")
        {
            logout(login, sockfd);
            close(sockfd);
            cout << "Closing connection" << endl;
            if(command == "/quit") exit(1);
        }
        else if (command.front() == '/')
        {
            cout << "Unknown command" << endl;
        }
        else
        {
            
        }
        cout << endl;
    }
    
    
    
    //waiting for new commands(joint, leave, create session etc.) from user after login
    while (1) {

        string cmd;
        getline(cin, cmd);
        stringstream command(cmd);
        command>>cmd;
        //if the user enters quit or logout, ends the program
        
        char buf[MAXDATASIZE];
        strncpy(buf, buffer.c_str(), MAXDATASIZE);
        //if we receive invalid info from server, terminates the program
        if (recv(sockfd, buf, MAXDATASIZE - 1, 0) == -1) {
            close(sockfd);
            cout << "closing connection" << endl;
            exit(1);
        }

    }
    
    close(sockfd);

    return 0;
}
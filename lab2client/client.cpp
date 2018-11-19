/* 
 * File:   client.cpp
 * Author: anileeli
 *
 * Created on November 12, 2018, 8:46 PM
 */


/* TODO
 *
 * - Disconnect clients when server closes
 * - Double logging in
 * - Parsing issues (e.g. incorrect number of arguments)
 * - Case where user inputs multiple words for a session name
 * - Case where user can create multiple sessions and is also in all of them at the same time
 * - Note: when no one is in a session, the session still remains on the server side. 
 *         Possibly have to delete that empty session
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
#define CMD_LOGOUT     "/logout"
#define CMD_JOINSESS   "/joinsession"
#define CMD_LEAVESESS  "/leavesession"
#define CMD_CREATESESS "/createsession"
#define CMD_LIST       "/list"
#define CMD_QUIT       "/quit"

// Inserted into data when sending the list of clients and sessions to a client
#define CLIENT_LIST_STRING "Clients Online:"
#define SESSION_LIST_STRING "Available Sessions:"

#define SESSION_NOT_FOUND "NoSessionFound"

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


// GLOBAL VARIABLES
int sockfd = -1;                // Socket used to communicate with server
struct connectionDetails login; // Holds login details pertaining to this client
bool loggedIn = false;          // Keep track of if this client is logged in
bool inSession = false;         // Keep track of it this client is in a session


// Get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
    {
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
bool sendToServer(struct message *data)
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


// Sends login info to server and checks server's response
// Returns true if login is successful
bool requestLogin(struct connectionDetails login)
{
    char buffer[MAXDATASIZE];
    int numBytes, response;
    struct message info;
    info.type = LOGIN;
    info.size = sizeof(login.clientPassword);
    info.source = login.clientID;
    info.data = login.clientPassword;
        
    // Sends login request to server
    if(!sendToServer(&info)){
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
    
    if(response == LO_ACK)
    {
        cout << "Login successful!" << endl;
        return true;
    }
    else
    {
        cout << "Login failed!" << endl;
        return false;
    }
}


// Requests to join a session in the server and checks server's response
// Returns true if session is joined
bool requestJoinSession(string sessionID)
{
    char buffer[MAXDATASIZE];
    int numBytes, response;
    struct message joinSession;
    joinSession.type = JOIN;
    joinSession.size = sizeof(sessionID);
    joinSession.source = login.clientID;
    joinSession.data = sessionID;
    
    // Sends login request to server
    if(!sendToServer(&joinSession)){
        return false;
    }
    
    // Server response
    if((numBytes = recv(sockfd, buffer, MAXDATASIZE, 0)) == -1)
    {
        perror("recv");
        return false;
    }
    
    // Checking packet type
    string s(buffer), temp, data;
    stringstream ss(s);
    ss >> response >> temp >> temp;
    getline(ss, data); // Get the rest of stream into data

    if(response == JN_NAK) 
    {
        cout << "Error: " << data << endl;
        return false;
    }
    else if(response == JN_ACK)
    {
        cout << "Session '" << data << "' joined!" << endl;
        return true;
    }
    else
    {
        cout << "Unknown message type received" << endl;
        return false;
    }
}


// Asks server to remove it from the current session and checks server's response
// Returns true if session is exited
bool requestLeaveSession()
{
    char buffer[MAXDATASIZE];
    int numBytes, response;
    struct message leaveSession;
    leaveSession.type = LEAVE_SESS;
    leaveSession.size = 0;
    leaveSession.source = login.clientID;
    leaveSession.data = "";
    
    // Sends login request to server
    if(!sendToServer(&leaveSession)){
        return false;
    }
    
    // Server response
    if((numBytes = recv(sockfd, buffer, MAXDATASIZE, 0)) == -1)
    {
        perror("recv");
        return false;
    }
    
    string s(buffer), temp, data;
    stringstream ss(s);
    ss >> response >> temp >> temp;
    getline(ss, data); // Get the rest of stream into data
    
    if (response == LS_NAK)
    {
        cout << "Error: " << data << endl;
        return false;
    }
    else if (response == LS_ACK)
    {
        cout << "Exited session '" << data << "'!" << endl;
        return true;
    }
    else
    {
        cout << "Unknown message type received" << endl;
        return false;
    }
}


// Requests to make a new session and checks the server's response
// Returns true if session was successfully created
bool requestNewSession(string sessionID)
{
    char buffer[MAXDATASIZE];
    int numBytes, response;
    struct message newSession;
    newSession.type = NEW_SESS;
    newSession.size = sizeof(sessionID);
    newSession.source = login.clientID;
    newSession.data = sessionID;
    
    // Sends login request to server
    if(!sendToServer(&newSession)){
        return false;
    }
    
    // Server response
    if((numBytes = recv(sockfd, buffer, MAXDATASIZE, 0)) == -1)
    {
        perror("recv");
        return false;
    }
    
    // Checking packet type
    string s(buffer), temp, data;
    stringstream ss(s);
    ss >> response >> temp >> temp;
    getline(ss, data); // Get the rest of stream into data

    if(response == NS_NAK) 
    {
        cout << "Error: " << data << endl;
        return false;
    }
    
    else if(response == NS_ACK)
    {
        cout << "Session '" << data << "' created!" << endl;
        return true;
    }
    else
    {
        cout << "Unknown message type received" << endl;
        return false;
    }
}


// Sends the logout request to the server
void logout()
{
    struct message info;
    info.type = EXIT;
    info.size = 0;
    info.source = login.clientID;
    info.data = "";
    
    if(sendToServer(&info)) cout << "Logout successful!" << endl;
}


// Prints out list of connected clients and available sessions
void printClientSessionList(string buffer)
{
    // Get rid of type, data_size, and source, leaving just the list of clients and sessions
    stringstream ss(buffer);
    string temp;
    ss >> temp >> temp >> temp;

    // Printing list of clients and sessions
    string data;
    cout << "list received :" << buffer << endl;
    cout << "size of list: " << buffer.length() << endl;
    
    while(ss >> data)
    {
        if(data == SESSION_LIST_STRING || data == CLIENT_LIST_STRING) cout << endl;
        cout << data << endl;
    }
   
    cout << endl;
}


// Sends a request to return the list of active clients and available sessions
// Returns session list if bool is true
pair<bool, string> requestClientSessionList()
{
    int numBytes, response;
    char buffer[MAXDATASIZE];
    
    // Prepare message
    struct message info;
    info.type = QUERY;
    info.size = 0;
    info.source = login.clientID;
    info.data = "";
    
    if(!sendToServer(&info)){
        cout << "List unavailable!" << endl;
        return make_pair(false, "NoList");
    }
    
    // Server response
    if((numBytes = recv(sockfd, buffer, MAXDATASIZE, 0)) == -1)
    {
        perror("recv");
        return make_pair(false, "NoList");
    }
    
    cout<<"buffer: "<<buffer<<endl;
    cout<<"numBytes: "<<numBytes<<endl;
    // Checking packet type
    string s(buffer);
    stringstream ss(s);
    ss >> response;
    
    if(response != QU_ACK)
    {
        cout << "List unavailable!" << endl;
        return make_pair(false, "NoList");
    }
    // List received, return it
    else
    {
        return make_pair(true, s);
    }
}


// Creates connection with server and returns socket file descriptor that
// describes the connection
int createConnection()
{
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


int main(int argc, char** argv)
{
    if (argc != 1)
    {
        fprintf(stderr, "usage: client\n");
        exit(1);
    }

    /********************** GET LOGIN/CONNECTION INFO *************************/

    cout << "\nPlease enter login information in the following format:\n"
            "/login <client_id> <password> <server-IP> <server-port>\n" << endl;

    while(1)
    {
        cout << ">>";
        // Create stringstream to extract login input from user
        string input, command;
        getline(cin, input);
        stringstream ss(input);
        
        ss >> command;

        if (command == CMD_LOGIN)
        {
            if(!loggedIn)
            {
                // Get login information
                ss >> login.clientID >> login.clientPassword
                   >> login.serverIP >> login.serverPort;

                // Create connection and get file descriptor
                sockfd = createConnection();

                // If connection created and login info sent successfully
                if(sockfd != -1 && requestLogin(login)) loggedIn = true;
            }
            else cout << "Already logged in!" << endl;
            
        }
        else if(command == CMD_LOGOUT)
        {
            if(inSession)
            {
                cout << "Please leave the session before logging out!" << endl;
            }
            else if(loggedIn)
            {
                logout();
                loggedIn = false;
                
                cout << "Closing connection" << endl;
                close(sockfd);
            }
            else cout << "Please login" << endl;
        }
        else if(command == CMD_QUIT)
        {
            if(inSession)
            {
                cout << "Please leave the session before quitting!" << endl;
            }
            else if(loggedIn)
            {
                logout();
                loggedIn = false;
                
                cout << "Closing connection" << endl;
                close(sockfd);
            }
            else exit(1);
        }
        else if(!loggedIn) // Cannot enter any other command before logging in
        {
            cout << "Please login" << endl;
        }
        else if (command == CMD_JOINSESS)
        {
            string sessionID;
            ss >> sessionID;
            if(requestJoinSession(sessionID))
            {
                inSession = true;
            }
        }
        else if (command == CMD_LEAVESESS)
        {
            if(requestLeaveSession())
            {
                inSession = false;
            }
        }
        else if (command == CMD_CREATESESS)
        {
            string sessionID;
            ss >> sessionID;
            if(requestNewSession(sessionID))
            {
                inSession = true;
            }
        }
        else if (command == CMD_LIST)
        {
            if(inSession)
            {
                cout << "Please leave the session before listing connected "
                        "clients and available sessions!" << endl;
            }
            else
            {
                auto res = requestClientSessionList();
                if(res.first == true) printClientSessionList(res.second);
            }
        }
        else
        {
            if(!inSession)
            {
                cout << "Unknown command" << endl;
            }
            else
            {
                // Send the message to the current session
            }
        }
        
        cout << endl;
    }
    return 0;
}
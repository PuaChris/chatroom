/* 
 * File:   client.cpp
 * Author: anileeli
 *
 * Created on November 12, 2018, 8:46 PM
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

#define PORT "3490" // the port client will be connecting to 

#define MAXDATASIZE 100 // max number of bytes we can get at once 

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

//this function sends a string(buffer) to server which holds struct message information
//in the following format: buffer = "type sizeof(data) source data"
//if sending to server is successful this function returns true otherwise returns false
bool sendToServer(struct message *data, int sockfd){
    string buffer;
    int numBytes;
    buffer = to_string(data->type) + " " + to_string(data->size) 
            + " " + data->source + " " + data->data;
    if(buffer.length() >= MAXDATASIZE)
        return false;
    char buf[MAXDATASIZE];
    strncpy(buf, buffer.c_str(), MAXDATASIZE);
    if((numBytes = send(sockfd, buf, MAXDATASIZE, 0)) == -1){
        perror("send");
        return false;
    }
    return true;
}

//this function populates struct connectionDetails login with client's information and
//calls sendToServer() which sends the client's login information to the server
//this function also checks server response for login, if login is successful it returns true else false
bool tryLogin(struct connectionDetails login, int sockfd){
    char buffer[MAXDATASIZE];
    int numBytes, response;
    struct message info;
    info.type = LOGIN;
    info.source = login.clientID;
    info.data = login.clientPassword;
    info.size = login.clientPassword.length();
    
    //sends login request to server
    if(! sendToServer(&info, sockfd)){
        return false;
    }
    
    //server response
    if((numBytes = recv(sockfd, buffer, MAXDATASIZE-1, 0)) == -1){
        perror("recv");
        return false;
    }
    buffer[numBytes] = '\0';
    response = (int)strtol(buffer, (char**)NULL, 10);
    if(response != LO_ACK){
        cout<<"Login failed!"<<endl;
        return false;
    }
    else
    return true;
}
//this function sends the logout request to the server to exit the server
void logout(struct connectionDetails login, int sockfd){
    struct message info;
    info.type = EXIT;
    info.size = 0;
    info.source = login.clientID;
    info.data = "";
    
    if(sendToServer(&info, sockfd)){
        cout<<"Logout successful!"<<endl;
    }
    
    //NOT SURE IF WE NEED TO CHECK SERVER RESPONSE HERE
    
}

//This prints out a list of connected clients and available sessions from server 
void print_list(string buffer){
    string token;
    string delimiter = " ";
    
    //prints each word from the list
    //should be modified after implementing sessions
    //right now the string expects just a list of active clients
    size_t pos = 0;
    while((pos = buffer.find(delimiter)) != std::string::npos){
        token = buffer.substr(0, buffer.find(delimiter));
        cout<<token<<", ";
        buffer.erase(0, buffer.find(delimiter) + delimiter.length());
    }
    cout<<endl;
}

//this function sends the list request to client to print the list of
//connected clients and available sessions
//if the server sends the list this function returns true oyherwise returns false
//this function expects a list of connected clients and available sessions from server in the following string format:
//QU_ACK (list of connected clients and available sessions)"
bool list(struct connectionDetails login, int sockfd){
    int numBytes, response;
    string buffer;
    string delimiter = " ";
    struct message info;
    info.type = QUERY;
    info.size = 0;
    info.source = "";
    info.data = "";
    
    if(! sendToServer(&info, sockfd)){
        cout<<"list not found!"<<endl;
        return false;
    }
    
    //check response from server
    char buf[MAXDATASIZE];
    strncpy(buf, buffer.c_str(), MAXDATASIZE);
    if((numBytes = recv(sockfd, buf, MAXDATASIZE-1, 0)) == -1){
        perror("recv");
        return false;
    }
    
    buffer[numBytes] = '\0';
    response = stoi(buffer.substr(0, buffer.find(delimiter)));
    //if the server doesn't send QU_ACK at the begining of the returned string
    if(response != QU_ACK){
        cout<<"No list found!"<<endl;
        return false;
    }
    else {
        //removing QU_ACK from the string received from server so that the string contains the list only
        buffer.erase(0, buffer.find(delimiter) + delimiter.length());
        //passing new string with removed QU_ACK to print_list to print the whole list
        print_list(buffer);
        return true;
    }
}


// Creates connection with server and returns socket file descriptor that
// describes the connection
int createConnection(struct connectionDetails login) {
    
    int newSockFD, numbytes;
    char buf[MAXDATASIZE];
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];
    //if invalid/incomplete login information provided
    if(login.clientID.empty() == true || login.clientPassword.empty() == true 
            || login.serverIP.empty() == true || login.serverPort.empty() == true){
        cout<<"Invalid login info!"<<endl;
    }
    
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(login.serverIP.c_str(), login.serverPort.c_str(), &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return -1;
    }

    // loop through all the results and connect to the first we can
    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((newSockFD = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }

        if (connect(newSockFD, p->ai_addr, p->ai_addrlen) == -1) {
            close(newSockFD);
            perror("client: connect");
            continue;
        }

        break;
    }

    if (p == NULL) {
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

    if (argc != 1) {
        fprintf(stderr, "usage: client\n");
        exit(1);
    }

    /********************** GET LOGIN/CONNECTION INFO *************************/

    cout << "Please enter login information in the following format:\n"
            "/login <client_id> <password> <server-IP> <server-port>\n" << endl;

    // Create stringstream to extract login input from user
    string loginInput, command;
    getline(cin, loginInput);
    stringstream ssLogin(loginInput);

    ssLogin >> command >> login.clientID >> login.clientPassword
            >> login.serverIP >> login.serverPort;

    sockfd = createConnection(login);
    if(sockfd != -1){
        if(tryLogin(login, sockfd))
            cout<<"Login successful!"<<endl;
    }
    //waiting for new commands(joint, leave, create session etc.) from user after login
    while (1) {

        string cmd;
        getline(cin, cmd);
        stringstream command(cmd);
        command>>cmd;
        //if the user enters quit or logout, ends the program
        if (cmd == "/logout" || cmd == "/quit") {
            logout(login, sockfd);
            close(sockfd);
            cout << "closing connection" << endl;
            exit(1);
        }
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
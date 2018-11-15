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

#define BACKLOG 10     // How many pending connections queue will hold
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


void sigchld_handler(int s) {
    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;

    while (waitpid(-1, NULL, WNOHANG) > 0);

    errno = saved_errno;
}

// Get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*) sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*) sa)->sin6_addr);
}

//this parser is only for login
void login_parser(string buffer, struct message *msg){
    
    stringstream ss(buffer);
    string temp;
    ss>>temp;
    if(std::stoi(temp) == 0){
        msg->type = LOGIN;
        temp.clear();
        ss>>temp;
        msg->size = std::stoi(temp);
        temp.clear();
        ss>>temp;
        msg->source = temp;
        temp.clear();
        ss>>temp;
        msg->data = temp;
    }
    
}

int main(int argc, char** argv) {

    int sockfd, new_fd; // listen on sock_fd, new connection on new_fd
    struct addrinfo hints, *servinfo, *p;
    socklen_t sin_size;
    struct sigaction sa;
    int yes = 1;
    char s[INET6_ADDRSTRLEN];
    int rv;
    struct sockaddr_storage their_addr; // connector's address information
    
    //Checking for any child processes going on
    pid_t childpid;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, argv[1], &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and bind to the first we can (from Beej)
    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                sizeof (int)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo); // all done with this structure

    if (p == NULL) {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

    //reapping zombie processes that appear as the fork()ed child processes exit (from Beej)
    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    printf("server: waiting for connections...\n");
    
    //waiting for requests from client to process accordingly (from beej)
    while (1) { //main access loop
        sin_size = sizeof their_addr;
        new_fd = accept(sockfd, (struct sockaddr *) &their_addr, &sin_size);
        if (new_fd == -1) {
            perror("accept");
            continue;
        }
        //Retrieves IP address from the client that is currently being connected to
        inet_ntop(their_addr.ss_family,
                get_in_addr((struct sockaddr *) &their_addr),
                s, sizeof s);
        printf("server: got connection from %s\n", s);
//        if (!fork()) {
//            close(sockfd);
//        }
        
        //If there's no child processes
        if((childpid = fork()) == 0){
            close(sockfd);
        }
        
        //handles client's valid requests
        //right now only recognizes login, logout, quit and list
        while (1) {
            int numBytes;
            string buffer;
            char buf[MAXDATASIZE];
            //if invalid data received from client
            if ((numBytes = recv(new_fd, buf, MAXDATASIZE, 0)) == -1) {
                perror("recv");
            } else {
                buf[numBytes] = '\0';
                buffer = buf;
                struct message msg;
                string clientID, password;
                //parses login command and populates struct message msg with login information
                login_parser(buffer, &msg);

                if (msg.type == LOGIN) {
                    cout << "Verifying login information!" << endl;
                    
                    //opening file which contains all clients login information
                    fstream file;
                    file.open("data.txt");
                    
                    //if file is opened
                    if (file.is_open()) {
                        //variable which changes to true in case of successful login
                        bool login = false;
                        //matching login information with the exisitng text file
                        //right now the login process works only if the user provides clientID and password 
                        //that match with the file the contains users login information
                        while (!file.eof()) {
                            file >> clientID>>password;
                            //if matched
                            if (msg.source == clientID && msg.data == password) {
                                memset(buf, 0, MAXDATASIZE);
                                strncpy(buf, std::to_string(LO_ACK).c_str(), MAXDATASIZE);
                                send(new_fd, buf, std::to_string(LO_ACK).length(), 0);
                                cout << "Successfully logged in" << endl;
                                login = true;
                                //copy zeroes into string buf
                                bzero(buf, sizeof(buf));
                                break;
                            }
                        }
                        //if user provided login information didn't match
                        if (login == false) {
                            memset(buf, 0, MAXDATASIZE);
                            strncpy(buf, std::to_string(LO_NAK).c_str(), MAXDATASIZE);
                            send(new_fd, buf, std::to_string(LO_NAK).length(), 0);
                            bzero(buf, sizeof(buf));
                            cout << "Login failed" << endl;
                        }
                        break;
                    } 
                    else{
                        cout << "Unable to verify login information" << endl;
                        break;
                    }
                }
                //parser doesn't handle quit, logout or list now, so this conditions will never be true
                else if (msg.type == EXIT) {
                    close(new_fd);
                } 
                else if (msg.type == QUERY) {

                }
            }
        }

    }
    

    return 0;
}
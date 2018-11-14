/* 
 * File:   server.cpp
 * Author: anileeli
 *
 * Created on November 12, 2018, 8:45 PM
 */

#include <cstdlib>
#include <string>
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

typedef struct str_node {
    char *str;
    struct str_node *next;
} str_node;

typedef struct client_node {
    char id[20];
    str_node *joined_sess;
    int socket;
    struct client_node *next;
} client_node;

typedef struct sess_node {
    char id[20];
    str_node *joined_clients;
    struct sess_node *next;
} sess_node;

typedef struct server_db {
    client_node *client_list;
    sess_node *sess_list;
} server_db;


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

void remove_socket(server_db *db, int socket){
    
}

void parser(string buffer, struct message *msg){
    
}

int main(int argc, char** argv) {

    int sockfd, new_fd; // listen on sock_fd, new connection on new_fd
    struct addrinfo hints, *servinfo, *p;
    socklen_t sin_size;
    struct sigaction sa;
    int yes = 1;
    char s[INET6_ADDRSTRLEN];
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, argv[1], &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and bind to the first we can
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

    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    printf("server: waiting for connections...\n");
    
    fd_set master;
    fd_set read_fds;
    int fdmax;
    
    FD_SET(sockfd, &master);
    
    server_db db;
    fdmax = sockfd;
    
    while(1) {  // main accept() loop
        
        read_fds = master;
        if (select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1) {
            perror("select");
            exit(4);
        }
        for (int i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &read_fds)) {
                if (i == sockfd) {
                    struct sockaddr_storage their_addr; // connector's address information
                    sin_size = sizeof their_addr;
                    new_fd = accept(sockfd, (struct sockaddr *) &their_addr, &sin_size);
                    if (new_fd == -1) {
                        perror("accept");
                        continue;
                    } else {

                        FD_SET(new_fd, &master);
                        if (new_fd > fdmax)
                            fdmax = new_fd;
                        inet_ntop(their_addr.ss_family,
                                get_in_addr((struct sockaddr *) &their_addr),
                                s, sizeof s);
                        printf("server: got connection from %s\n", s);
                    }
                }
                else {

                    //response based on client's request
                    int numBytes;
                    string buffer;
                    
                    char buf[MAXDATASIZE];
//                    strncpy(buf, buffer.c_str(), MAXDATASIZE); 
                    if((numBytes = recv(i, buf, MAXDATASIZE, 0)) == -1){
                        perror("recv");
                        remove_socket(&db, i);
                        close(i);
                        FD_CLR(i, &master);
                    }
                    else{
                        buf[numBytes] = '\0';
                        buffer = buf;
                        struct message msg;
                        string clientID, password;
                        parser(buffer, &msg);
                        
                        if(msg.type == LOGIN){
                            cout<<"Verifying login information!"<<endl;
                            
                            ifstream file;
                            file.open("data.txt");
                            if(file.is_open()){
                                while(! file.eof()){
                                    file>>clientID>>password;
                                    if(msg.source == clientID && msg.data == password){
                                        memset(buf, 0, MAXDATASIZE);
                                        strncpy(buf, std::to_string(LO_ACK).c_str(), MAXDATASIZE);
                                        send(i, buf, std::to_string(LO_ACK).length(), 0);
                                        cout<<"Successfully logged in"<<endl;
                                    }
                                }
                                memset(buf, 0, MAXDATASIZE);
                                strncpy(buf, std::to_string(LO_NAK).c_str(), MAXDATASIZE);
                                send(i, buf, std::to_string(LO_NAK).length(), 0);
                                cout<<"Login failed"<<endl;
                            }
                        }
                        else if (msg.type == EXIT) {
                            remove_socket(&db, i);
                            close(i);
                            FD_CLR(i, &master);
                        }
                        else if(msg.type == QUERY){
                            
                        }
                    }
                }
            }
        }
        

        if (!fork()) { // this is the child process
            close(sockfd); // child doesn't need the listener
            if (send(new_fd, "Hello, world!", 13, 0) == -1)
                perror("send");
            close(new_fd);
            exit(0);
        }
        close(new_fd);  // parent doesn't need this
    }

    return 0;
}
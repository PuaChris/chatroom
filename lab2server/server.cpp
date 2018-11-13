/* 
 * File:   server.cpp
 * Author: anileeli
 *
 * Created on November 12, 2018, 8:45 PM
 */

#include <cstdlib>

#define MAX_NAME    20
#define MAX_DATA    1000

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
    unsigned char source[MAX_NAME];
    unsigned char data[MAX_DATA];
};

using namespace std;

int main(int argc, char** argv) {


//    int sockfd, new_fd; // listen on sock_fd, new connection on new_fd
//    struct addrinfo hints, *servinfo, *p;
//    struct sockaddr_storage their_addr; // connector's address information
//    socklen_t sin_size;
//    struct sigaction sa;
//    int yes = 1;
//    char s[INET6_ADDRSTRLEN];
//    int rv;
//
//    memset(&hints, 0, sizeof hints);
//    hints.ai_family = AF_UNSPEC;
//    hints.ai_socktype = SOCK_STREAM;
//    hints.ai_flags = AI_PASSIVE; // use my IP
//
//    if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
//        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
//        return 1;
//    }
//
//    // loop through all the results and bind to the first we can
//    for (p = servinfo; p != NULL; p = p->ai_next) {
//        if ((sockfd = socket(p->ai_family, p->ai_socktype,
//                p->ai_protocol)) == -1) {
//            perror("server: socket");
//            continue;
//        }
//
//        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
//                sizeof (int)) == -1) {
//            perror("setsockopt");
//            exit(1);
//        }
//
//        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
//            close(sockfd);
//            perror("server: bind");
//            continue;
//        }
//
//        break;
//    }
//
//    freeaddrinfo(servinfo); // all done with this structure
//
//    if (p == NULL) {
//        fprintf(stderr, "server: failed to bind\n");
//        exit(1);
//    }
//
//    if (listen(sockfd, BACKLOG) == -1) {
//        perror("listen");
//        exit(1);
//    }
//
//    sa.sa_handler = sigchld_handler; // reap all dead processes
//    sigemptyset(&sa.sa_mask);
//    sa.sa_flags = SA_RESTART;
//    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
//        perror("sigaction");
//        exit(1);
//    }
//
//    printf("server: waiting for connections...\n");

    return 0;
}


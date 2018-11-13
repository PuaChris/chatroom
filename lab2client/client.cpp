/* 
 * File:   client.cpp
 * Author: anileeli
 *
 * Created on November 12, 2018, 8:46 PM
 */

#include <cstdlib>
#include <string>
#include <sstream>

#define MAX            20
#define MAX_NAME       20
#define MAX_DATA       1000
#define CMD_LOGIN      "/login"
#define CMD_LOGOUT     "/logout\n"
#define CMD_JOINSESS   "/joinsession"
#define CMD_LEAVESESS  "/leavesession\n"
#define CMD_CREATESESS "/createsession"
#define CMD_LIST       "/list\n"
#define CMD_QUIT       "/quit\n"

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

// Global variables
bool isConnected = false;
string buffer;
int sockfd, numBytes;

// Message structure to be serialized when sending messages
struct message {
    unsigned int type;
    unsigned int size;
    unsigned char source[MAX_NAME];
    unsigned char data[MAX_DATA];
};

struct connectionDetails {
    string clientID;
    string clientPassword;
    string serverIP;
    string serverPort;
};


int main(int argc, char** argv) {

    struct connectionDetails login;
    
    cout << "Please enter login information in the following format:\n"
            "/login <client_id> <password> <server-IP> <server-port>\n" << endl;
    
    // Create stringstream to extract input from user
    string loginInput, command;
    getline(cin, loginInput);
    stringstream ssLogin(loginInput);
    
    ssLogin >> command >> login.clientID >> login.clientPassword
            >> login.serverIP >> login.serverPort;
    
    return 0;
}


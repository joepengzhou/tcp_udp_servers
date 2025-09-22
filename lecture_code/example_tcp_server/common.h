#ifndef COMMON_H
#define COMMON_H

#include <iostream>
#include <string>
#include <vector>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <chrono>
#include <algorithm>
#include <sstream>
#include <iomanip>

using namespace std;

// Message types
enum MessageType {
     MSG_CHAT = 1,
     MSG_STATS = 2
};

// Message structure
struct Message {
     MessageType type;
     int client_id;
     int content_length;
     char content[1024];
     
     Message() : type(MSG_CHAT), client_id(0), content_length(0) {
       memset(content, 0, sizeof(content));
     }
};

// Client information structure
struct ClientInfo {
     int socket_fd;
     int client_id;
     pthread_t thread_id;
     string client_ip;
     int client_port;
     
     ClientInfo(int fd, int id, const string& ip, int port) 
       : socket_fd(fd), client_id(id), client_ip(ip), client_port(port) {}
};

// Server statistics
struct ServerStats {
     int client_count;
     chrono::steady_clock::time_point start_time;
     
     ServerStats() : client_count(0) {
       start_time = chrono::steady_clock::now();
     }
     
     double get_uptime_seconds() const {
       auto now = chrono::steady_clock::now();
       auto duration = chrono::duration_cast<chrono::seconds>(now - start_time);
       return duration.count();
     }
};

// Global variables for server
extern vector<ClientInfo> clients;
extern pthread_mutex_t clients_mutex;
extern pthread_mutex_t stats_mutex;
extern ServerStats server_stats;
extern int next_client_id;

// Function declarations
void* handle_client(void* arg);
void broadcast_message(const Message& msg, int exclude_client_id = -1);
void send_message(int socket_fd, const Message& msg);
Message receive_message(int socket_fd);
string get_timestamp();
void print_debug(const string& message);

#endif // COMMON_H
#pragma once

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
enum TcpMessageType {
    MSG_CHAT  = 1,
    MSG_STATS = 2
};

// Message structure
struct TcpMessage {
    TcpMessageType type;
    int client_id;
    int payload_length;
    char payload[1024];

    TcpMessage() : type(MSG_CHAT), client_id(0), payload_length(0) {
        memset(payload, 0, sizeof(payload));
    }
};

// Client information structure
struct TcpClientInfo {
    int socket_fd;
    int client_id;
    pthread_t thread_id;
    string client_ip;
    int client_port;

    TcpClientInfo(int fd, int id, const string& ip, int port)
        : socket_fd(fd), client_id(id), client_ip(ip), client_port(port) {}
};

// Server statistics
struct TcpServerStats {
    int client_count;
    chrono::steady_clock::time_point start_time;

    TcpServerStats() : client_count(0) {
        start_time = chrono::steady_clock::now();
    }

    double get_uptime_seconds() const {
        auto now = chrono::steady_clock::now();
        auto duration = chrono::duration_cast<chrono::seconds>(now - start_time);
        return duration.count();
    }
};

// Server implementation details are defined in TCPServer.cpp to mirror UDP style



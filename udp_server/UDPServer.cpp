
//UDP chat server with ACK and stats
#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <algorithm>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <chrono>
#include <queue>
#include <unordered_map>

#include "UDPCommon.h"

using namespace std;

struct ClientEndpoint {
    sockaddr_in addr;
    uint32_t clientId;
    chrono::steady_clock::time_point lastSeen;
};

struct ServerStats {
    chrono::steady_clock::time_point startTime;
    ServerStats() : startTime(chrono::steady_clock::now()) {}
    int uptimeSeconds() const {
        return (int)chrono::duration_cast<chrono::seconds>(
            chrono::steady_clock::now() - startTime).count();
    }
};

// Globals
static int g_socket_fd = -1;
static vector<ClientEndpoint> g_clients;
static pthread_mutex_t g_clients_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_stats_mutex = PTHREAD_MUTEX_INITIALIZER;
static ServerStats g_stats;
static uint32_t g_nextClientId = 1;

static pthread_mutex_t g_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_queue_cv = PTHREAD_COND_INITIALIZER;

struct Outgoing {
    vector<uint8_t> packet;
    sockaddr_in addr;
};

static queue<Outgoing> g_outgoing;

static string endpoint_key(const sockaddr_in &addr)
{
    char ip[INET_ADDRSTRLEN] = {0};
    inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof(ip));
    stringstream ss;
    ss << ip << ":" << ntohs(addr.sin_port);
    return ss.str();
}

static void print_debug(const string &msg)
{
    cerr << "[DEBUG] " << msg << endl;
}

static uint32_t find_client_id_by_addr(const sockaddr_in &addr)
{
    uint32_t id = 0;
    pthread_mutex_lock(&g_clients_mutex);
    for (const auto &c : g_clients) {
        if (c.addr.sin_addr.s_addr == addr.sin_addr.s_addr &&
            c.addr.sin_port == addr.sin_port) {
            id = c.clientId;
            break;
        }
    }
    pthread_mutex_unlock(&g_clients_mutex);
    return id;
}

static void ensure_register_client(const sockaddr_in &addr, const string &maybeHello)
{
    if (find_client_id_by_addr(addr) != 0) return;

    if (maybeHello != "hello") {
        // Only register on explicit hello as per requirement
        return;
    }

    ClientEndpoint ce{};
    ce.addr = addr;
    ce.clientId = g_nextClientId++;
    ce.lastSeen = chrono::steady_clock::now();

    pthread_mutex_lock(&g_clients_mutex);
    g_clients.push_back(ce);
    size_t count = g_clients.size();
    pthread_mutex_unlock(&g_clients_mutex);

    print_debug("Registered new client id=" + to_string(ce.clientId) +
                " from " + endpoint_key(addr) + ", total clients=" + to_string(count));
}

static void enqueue_send(const vector<uint8_t> &packet, const sockaddr_in &addr)
{
    pthread_mutex_lock(&g_queue_mutex);
    g_outgoing.push(Outgoing{packet, addr});
    pthread_cond_signal(&g_queue_cv);
    pthread_mutex_unlock(&g_queue_mutex);
}

static void broadcast_to_all_except(const vector<uint8_t> &packet, uint32_t excludeId)
{
    pthread_mutex_lock(&g_clients_mutex);
    for (const auto &c : g_clients) {
        if (c.clientId == excludeId) continue;
        enqueue_send(packet, c.addr);
    }
    pthread_mutex_unlock(&g_clients_mutex);
}

static void *sender_thread(void *)
{
    print_debug("Sender thread started");
    while (true) {
        pthread_mutex_lock(&g_queue_mutex);
        while (g_outgoing.empty()) {
            pthread_cond_wait(&g_queue_cv, &g_queue_mutex);
        }
        Outgoing out = g_outgoing.front();
        g_outgoing.pop();
        pthread_mutex_unlock(&g_queue_mutex);

        ssize_t sent = sendto(g_socket_fd, out.packet.data(), out.packet.size(), 0,
                              (const sockaddr*)&out.addr, sizeof(out.addr));
        if (sent < 0) {
            print_debug("sendto failed");
        }
    }
    return nullptr;
}

static void reply_ack(const sockaddr_in &addr, uint32_t seq, uint32_t clientId)
{
    vector<uint8_t> pkt;
    build_packet(pkt, MSG_CHAT, FLAG_ACK, seq, clientId, nullptr, 0);
    enqueue_send(pkt, addr);
}

static void send_stats(const sockaddr_in &addr)
{
    int uptime = 0;
    size_t clients = 0;
    pthread_mutex_lock(&g_stats_mutex);
    uptime = g_stats.uptimeSeconds();
    pthread_mutex_unlock(&g_stats_mutex);

    pthread_mutex_lock(&g_clients_mutex);
    clients = g_clients.size();
    pthread_mutex_unlock(&g_clients_mutex);

    stringstream ss;
    ss << "Server Statistics:\n Clients connected: " << clients
       << "\n Server uptime: " << uptime << " seconds";
    string s = ss.str();
    vector<uint8_t> pkt;
    build_packet(pkt, MSG_STATS, 0, 0, 0,
                 reinterpret_cast<const uint8_t*>(s.data()), (uint32_t)s.size());

    enqueue_send(pkt, addr);
}

static void handle_packet(const uint8_t *data, size_t len, const sockaddr_in &from)
{
    uint16_t type, flags; uint32_t seq, clientId, plLen; const uint8_t *payload;
    if (!parse_packet(data, len, type, flags, seq, clientId, payload, plLen)) {
        print_debug("Invalid packet received");
        return;
    }

    string content;
    if (plLen > 0) content.assign(reinterpret_cast<const char*>(payload), plLen);

    if (type == MSG_CHAT && (flags & FLAG_ACK) == 0) {
        // Registration on hello
        ensure_register_client(from, content);
        uint32_t senderId = find_client_id_by_addr(from);
        if (senderId == 0) {
            // not registered; ignore non-hello chat
            return;
        }

        // ACK back to sender
        reply_ack(from, seq, senderId);

        // Broadcast chat to others (exclude sender)
        string prefixed = "[" + get_timestamp() + "] Client " + to_string(senderId) + ": " + content;
        vector<uint8_t> pkt;
        build_packet(pkt, MSG_CHAT, 0, 0, senderId,
                     reinterpret_cast<const uint8_t*>(prefixed.data()), (uint32_t)prefixed.size());
        broadcast_to_all_except(pkt, senderId);
        print_debug("Broadcasted chat from client " + to_string(senderId));
    } else if (type == MSG_STATS) {
        send_stats(from);
    } else if (type == MSG_CHAT && (flags & FLAG_ACK)) {
        // Server does not expect ACKs for its broadcasts; ignore
    }
}

int main(int argc, char *argv[])
{
    int port = 5001;
    if (argc > 1) {
        int p = atoi(argv[1]);
        if (p > 0 && p <= 65535) port = p;
    }

    g_socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (g_socket_fd < 0) {
        cerr << "Failed to create UDP socket" << endl;
        return 1;
    }

    sockaddr_in srv{};
    srv.sin_family = AF_INET;
    srv.sin_port = htons(port);
    srv.sin_addr.s_addr = INADDR_ANY;
    memset(srv.sin_zero, '\0', sizeof(srv.sin_zero));

    if (bind(g_socket_fd, (sockaddr*)&srv, sizeof(srv)) < 0) {
        cerr << "Bind failed on UDP port " << port << endl;
        close(g_socket_fd);
        return 1;
    }

    cout << "UDP Server listening on port " << port << endl;

    // Start sender thread
    pthread_t senderTid;
    if (pthread_create(&senderTid, NULL, sender_thread, NULL) != 0) {
        cerr << "Failed to start sender thread" << endl;
        close(g_socket_fd);
        return 1;
    }

    // Receive loop
    vector<uint8_t> buf(2048);
    while (true) {
        sockaddr_in from{}; socklen_t fromlen = sizeof(from);
        ssize_t n = recvfrom(g_socket_fd, buf.data(), buf.size(), 0,
                             (sockaddr*)&from, &fromlen);
        if (n < 0) {
            print_debug("recvfrom failed");
            continue;
        }
        handle_packet(buf.data(), (size_t)n, from);
    }

    close(g_socket_fd);
    return 0;
}

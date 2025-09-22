
// Multithreaded UDP client with basic reliability and commands
#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <chrono>
#include <fcntl.h>

#include "UDPCommon.h"

using namespace std;

static int g_sock = -1;
static sockaddr_in g_server{};
static volatile bool g_running = true;
static pthread_mutex_t g_send_mutex = PTHREAD_MUTEX_INITIALIZER;

// Reliability state for last sent MSG_CHAT
static pthread_mutex_t g_retx_mutex = PTHREAD_MUTEX_INITIALIZER;
static uint32_t g_nextSeq = 1;
static uint32_t g_lastPendingSeq = 0;
static vector<uint8_t> g_lastPacket;
static chrono::steady_clock::time_point g_lastSentTime;
static const int RETX_TIMEOUT_MS = 800;

static void print_debug(const string &msg)
{
    cerr << "[DEBUG] " << msg << endl;
}

static void send_packet_locked(const vector<uint8_t> &pkt)
{
    ssize_t sent = sendto(g_sock, pkt.data(), pkt.size(), 0,
                          (const sockaddr*)&g_server, sizeof(g_server));
    if (sent < 0) print_debug("sendto failed");
}

static void send_packet(const vector<uint8_t> &pkt)
{
    pthread_mutex_lock(&g_send_mutex);
    send_packet_locked(pkt);
    pthread_mutex_unlock(&g_send_mutex);
}

static void *receiver_thread(void *)
{
    print_debug("Receiver thread started");
    vector<uint8_t> buf(2048);
    while (g_running) {
        sockaddr_in from{}; socklen_t fromlen = sizeof(from);
        ssize_t n = recvfrom(g_sock, buf.data(), buf.size(), 0,
                             (sockaddr*)&from, &fromlen);
        if (n < 0) {
            usleep(100000);
            continue;
        }

        uint16_t type, flags; uint32_t seq, clientId, plLen; const uint8_t *payload;
        if (!parse_packet(buf.data(), (size_t)n, type, flags, seq, clientId, payload, plLen)) {
            continue;
        }

        if (type == MSG_CHAT && (flags & FLAG_ACK)) {
            pthread_mutex_lock(&g_retx_mutex);
            if (seq == g_lastPendingSeq && g_lastPendingSeq != 0) {
                g_lastPendingSeq = 0;
            }
            pthread_mutex_unlock(&g_retx_mutex);
        } else if (type == MSG_CHAT) {
            string s;
            if (plLen > 0) s.assign(reinterpret_cast<const char*>(payload), plLen);
            cout << s << endl;
        } else if (type == MSG_STATS) {
            string s;
            if (plLen > 0) s.assign(reinterpret_cast<const char*>(payload), plLen);
            cout << s << endl;
        }
    }
    return nullptr;
}

static void *retx_thread(void *)
{
    print_debug("Retransmit thread started");
    while (g_running) {
        pthread_mutex_lock(&g_retx_mutex);
        uint32_t pending = g_lastPendingSeq;
        auto last = g_lastSentTime;
        vector<uint8_t> pkt = g_lastPacket;
        pthread_mutex_unlock(&g_retx_mutex);

        if (pending != 0) {
            auto now = chrono::steady_clock::now();
            auto ms = chrono::duration_cast<chrono::milliseconds>(now - last).count();
            if (ms >= RETX_TIMEOUT_MS && !pkt.empty()) {
                print_debug("Retransmitting seq=" + to_string(pending));
                send_packet(pkt);
                pthread_mutex_lock(&g_retx_mutex);
                g_lastSentTime = chrono::steady_clock::now();
                pthread_mutex_unlock(&g_retx_mutex);
            }
        }
        usleep(100000); // 100ms
    }
    return nullptr;
}

static void send_hello()
{
    const string hello = "hello";
    vector<uint8_t> pkt;
    build_packet(pkt, MSG_CHAT, 0, 0, 0,
                 reinterpret_cast<const uint8_t*>(hello.data()), (uint32_t)hello.size());
    send_packet(pkt);
}

static void send_say(const string &text)
{
    uint32_t seq;
    vector<uint8_t> pkt;
    pthread_mutex_lock(&g_retx_mutex);
    seq = g_nextSeq++;
    build_packet(pkt, MSG_CHAT, 0, seq, 0,
                 reinterpret_cast<const uint8_t*>(text.data()), (uint32_t)text.size());
    g_lastPacket = pkt;
    g_lastPendingSeq = seq;
    g_lastSentTime = chrono::steady_clock::now();
    pthread_mutex_unlock(&g_retx_mutex);

    send_packet(pkt);
}

static void send_stats_request()
{
    vector<uint8_t> pkt;
    build_packet(pkt, MSG_STATS, 0, 0, 0, nullptr, 0);
    send_packet(pkt);
}

int main(int argc, char *argv[])
{
    string server_ip = "127.0.0.1";
    int port = 5001;
    if (argc > 1) server_ip = argv[1];
    if (argc > 2) { int p = atoi(argv[2]); if (p > 0 && p <= 65535) port = p; }

    g_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (g_sock < 0) {
        cerr << "Failed to create UDP socket" << endl;
        return 1;
    }

    
    sockaddr_in local{}; local.sin_family = AF_INET; local.sin_port = 0; local.sin_addr.s_addr = INADDR_ANY;
    bind(g_sock, (sockaddr*)&local, sizeof(local));

    g_server.sin_family = AF_INET;
    g_server.sin_port = htons(port);
    inet_pton(AF_INET, server_ip.c_str(), &g_server.sin_addr);
    memset(g_server.sin_zero, '\0', sizeof(g_server.sin_zero));

    cout << "UDP Client connecting to " << server_ip << ":" << port << endl;
    cout << "Commands:\n  /say <text>\n  /stats\n  /quit" << endl;

    // Start threads
    pthread_t rxTid, rtTid;
    pthread_create(&rxTid, NULL, receiver_thread, NULL);
    pthread_create(&rtTid, NULL, retx_thread, NULL);

    // Send hello to register
    send_hello();

    string line;
    while (g_running && getline(cin, line)) {
        if (line == "/quit") { g_running = false; break; }
        if (line.rfind("/say ", 0) == 0) {
            string msg = line.substr(5);
            if (!msg.empty()) send_say(msg);
            continue;
        }
        if (line == "/stats") {
            send_stats_request();
            continue;
        }
        cout << "Unknown command. Use /say <text>, /stats, /quit" << endl;
    }

    // Cleanup
    g_running = false;
    pthread_join(rxTid, NULL);
    pthread_join(rtTid, NULL);
    close(g_sock);
    return 0;
}

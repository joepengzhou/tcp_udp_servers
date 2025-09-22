#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <arpa/inet.h>

using namespace std;

// Message types
enum UdpMessageType : uint16_t {
    MSG_CHAT  = 1,
    MSG_STATS = 2
};

// Flags
static const uint16_t FLAG_ACK = 0x0001; // ACK for reliability

static const size_t UDP_MAX_PAYLOAD = 1024;

struct UdpHeader {
    uint16_t type;       // UdpMessageType (network order)
    uint16_t flags;      // bit flags (network order)
    uint32_t seq;        // sequence for reliability (network order)
    uint32_t clientId;   // assigned by server (network order)
    uint32_t payloadLen; // bytes of payload following header (network order)
};

static const size_t UDP_HEADER_SIZE = sizeof(UdpHeader);

inline string get_timestamp()
{
    auto now = chrono::system_clock::now();
    auto time_t_val = chrono::system_clock::to_time_t(now);
    auto ms = chrono::duration_cast<chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    stringstream ss;
    ss << put_time(localtime(&time_t_val), "%H:%M:%S");
    ss << '.' << setfill('0') << setw(3) << ms.count();
    return ss.str();
}

inline void encode_header(UdpHeader &hdr_net, uint16_t type, uint16_t flags,
                          uint32_t seq, uint32_t clientId, uint32_t payloadLen)
{
    hdr_net.type = htons(type);
    hdr_net.flags = htons(flags);
    hdr_net.seq = htonl(seq);
    hdr_net.clientId = htonl(clientId);
    hdr_net.payloadLen = htonl(payloadLen);
}

inline void decode_header(const UdpHeader &hdr_net, uint16_t &type, uint16_t &flags,
                          uint32_t &seq, uint32_t &clientId, uint32_t &payloadLen)
{
    type = ntohs(hdr_net.type);
    flags = ntohs(hdr_net.flags);
    seq = ntohl(hdr_net.seq);
    clientId = ntohl(hdr_net.clientId);
    payloadLen = ntohl(hdr_net.payloadLen);
}

inline size_t build_packet(vector<uint8_t> &out,
                           uint16_t type, uint16_t flags,
                           uint32_t seq, uint32_t clientId,
                           const uint8_t *payload, uint32_t payloadLen)
{
    out.resize(UDP_HEADER_SIZE + payloadLen);
    UdpHeader hdr;
    encode_header(hdr, type, flags, seq, clientId, payloadLen);
    memcpy(out.data(), &hdr, UDP_HEADER_SIZE);
    if (payloadLen > 0 && payload != nullptr) {
        memcpy(out.data() + UDP_HEADER_SIZE, payload, payloadLen);
    }
    return out.size();
}

inline bool parse_packet(const uint8_t *data, size_t len,
                         uint16_t &type, uint16_t &flags,
                         uint32_t &seq, uint32_t &clientId,
                         const uint8_t* &payload, uint32_t &payloadLen)
{
    if (len < UDP_HEADER_SIZE) return false;
    UdpHeader hdr_net;
    memcpy(&hdr_net, data, UDP_HEADER_SIZE);
    uint32_t pl;
    decode_header(hdr_net, type, flags, seq, clientId, pl);
    if (UDP_HEADER_SIZE + pl > len) return false;
    payload = data + UDP_HEADER_SIZE;
    payloadLen = pl;
    return true;
}



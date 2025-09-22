#include "TCPCommon.h"

using namespace std;

// Global variables
vector<TcpClientInfo> g_tcp_clients;
pthread_mutex_t g_tcp_clients_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t g_tcp_stats_mutex = PTHREAD_MUTEX_INITIALIZER;
TcpServerStats g_tcp_server_stats;
int g_tcp_next_client_id = 1;

void print_debug(const string& message) {
         cerr << "[DEBUG] " << message << endl;
}

      string get_timestamp() {
       auto now = chrono::system_clock::now();
       auto time_t = chrono::system_clock::to_time_t(now);
       auto ms = chrono::duration_cast< chrono::milliseconds>(
         now.time_since_epoch()) % 1000;
       
        stringstream ss;
       ss << put_time( localtime(&time_t), "%H:%M:%S");
       ss << '.' << setfill('0') << setw(3) << ms.count();
       return ss.str();
}

void send_message(int socket_fd, const TcpMessage& msg) {
       // Send the message structure
       ssize_t bytes_sent = send(socket_fd, &msg, sizeof(TcpMessage), 0);
       if (bytes_sent == -1) {
        print_debug("Failed to send message to client");
       } else {
        print_debug("Sent " + to_string(bytes_sent) + " bytes to client");
       }
}

TcpMessage receive_message(int socket_fd) {
       TcpMessage msg;
       ssize_t bytes_received = recv(socket_fd, &msg, sizeof(TcpMessage), 0);
       
       if (bytes_received <= 0) {
        msg.type = MSG_CHAT; // Invalid message type to indicate error
        msg.client_id = -1;
        print_debug("Failed to receive message or client disconnected");
       } else {
        print_debug("Received " + to_string(bytes_received) + " bytes from client");
       }
       
       return msg;
}

void broadcast_message(const TcpMessage& msg, int exclude_client_id) {
       pthread_mutex_lock(&g_tcp_clients_mutex);
       
       print_debug("Broadcasting to " + to_string(g_tcp_clients.size()) + " clients, excluding " + to_string(exclude_client_id));
       
       for (const auto& client : g_tcp_clients) {
         if (client.client_id != exclude_client_id) {
          print_debug("Sending message to client " + to_string(client.client_id));
          send_message(client.socket_fd, msg);
         }
       }
       
       pthread_mutex_unlock(&g_tcp_clients_mutex);
}

void* handle_client(void* arg) {
       TcpClientInfo* client_info = static_cast<TcpClientInfo*>(arg);
       int client_socket = client_info->socket_fd;
       int client_id = client_info->client_id;
       
       print_debug("Client " + to_string(client_id) + " connected from " + 
             client_info->client_ip + ":" + to_string(client_info->client_port));
       
       while (true) {
         TcpMessage msg = receive_message(client_socket);
         
         if (msg.client_id == -1) {
           // Client disconnected
           break;
         }
         
         msg.client_id = client_id; // Ensure correct client ID
         
         switch (msg.type) {
           case MSG_CHAT: {
                // Broadcast chat message to all other clients
                string chat_msg = "[" + get_timestamp() + "] Client " + 
                                  to_string(client_id) + ": " + msg.payload;
             
             TcpMessage broadcast_msg;
             broadcast_msg.type = MSG_CHAT;
             broadcast_msg.client_id = client_id;
             strncpy(broadcast_msg.payload, chat_msg.c_str(), sizeof(broadcast_msg.payload) - 1);
             broadcast_msg.payload_length = strlen(broadcast_msg.payload);
             
             broadcast_message(broadcast_msg, client_id);
             print_debug("Broadcasted message from client " + to_string(client_id));
             break;
           }
           
           case MSG_STATS: {
             // Send server statistics to requesting client
             pthread_mutex_lock(&g_tcp_stats_mutex);
             pthread_mutex_lock(&g_tcp_clients_mutex);
             
             string stats_msg = string("Server Statistics:\n") +
                        " Clients connected: " + to_string(g_tcp_clients.size()) + "\n" +
                        " Server uptime: " + to_string((int)g_tcp_server_stats.get_uptime_seconds()) + " seconds";
             
             TcpMessage response_msg;
             response_msg.type = MSG_STATS;
             response_msg.client_id = 0; // Server response
                strncpy(response_msg.payload, stats_msg.c_str(), sizeof(response_msg.payload) - 1);
                response_msg.payload_length = strlen(response_msg.payload);
             
             send_message(client_socket, response_msg);
             
             pthread_mutex_unlock(&g_tcp_clients_mutex);
             pthread_mutex_unlock(&g_tcp_stats_mutex);
             print_debug("Sent stats to client " + to_string(client_id));
             break;
           }
           
           default:
             print_debug("Unknown message type from client " + to_string(client_id));
             break;
         }
       }
       
       // Client disconnected - remove from clients list
       pthread_mutex_lock(&g_tcp_clients_mutex);
       g_tcp_clients.erase( remove_if(g_tcp_clients.begin(), g_tcp_clients.end(),
        [client_id](const TcpClientInfo& client) { return client.client_id == client_id; }),
        g_tcp_clients.end());
       pthread_mutex_unlock(&g_tcp_clients_mutex);
       
       print_debug("Client " + to_string(client_id) + " disconnected");
       close(client_socket);
       delete client_info;
       
       return nullptr;
}

int main(int argc, char* argv[]) {
       int port = 5000; // Default port
       
       // Parse command line arguments
       if (argc > 1) {
         port = atoi(argv[1]);
         if (port <= 0 || port > 65535) {
            cerr << "Invalid port number. Using default port 5000." << endl;
           port = 5000;
         }
       }
       
       int server_fd, opt = 1;
       struct sockaddr_in server_addr;
       
       // Create socket
       server_fd = socket(AF_INET, SOCK_STREAM, 0);
       if (server_fd == -1) {
          cerr << "Socket creation failed!" << endl;
         exit(EXIT_FAILURE);
       }
       
       // Set socket options
       if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
          cerr << "Socket setsockopt error!" << endl;
         exit(EXIT_FAILURE);
       }
       
       // Configure server address
       server_addr.sin_family = AF_INET;
       server_addr.sin_addr.s_addr = INADDR_ANY;
       server_addr.sin_port = htons(port);
       memset(server_addr.sin_zero, '\0', sizeof(server_addr.sin_zero));
       
       // Bind socket
       if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
          cerr << "Bind failed!" << endl;
         exit(EXIT_FAILURE);
       }
       
       // Listen for connections
       if (listen(server_fd, 10) < 0) {
          cerr << "Listen failed!" << endl;
         exit(EXIT_FAILURE);
       }
       
        cout << "Multi-threaded TCP Server started on port " << port << endl;
        cout << "Waiting for connections..." << endl;
       
       // Main server loop
       while (true) {
         struct sockaddr_in client_addr;
         socklen_t client_addrlen = sizeof(client_addr);
         
         int client_socket = accept(server_fd, (struct sockaddr*)&client_addr, &client_addrlen);
         if (client_socket < 0) {
            cerr << "Accept failed!" << endl;
           continue;
         }
         
         // Get client IP and port
          string client_ip = inet_ntoa(client_addr.sin_addr);
         int client_port = ntohs(client_addr.sin_port);
         
         // Create client info
         TcpClientInfo* client_info = new TcpClientInfo(client_socket, g_tcp_next_client_id++, client_ip, client_port);
         
         // Add client to list
         pthread_mutex_lock(&g_tcp_clients_mutex);
         g_tcp_clients.push_back(*client_info);
         pthread_mutex_unlock(&g_tcp_clients_mutex);
         
         // Create thread for this client
         if (pthread_create(&client_info->thread_id, NULL, handle_client, client_info) != 0) {
            cerr << "Failed to create thread for client!" << endl;
           close(client_socket);
           delete client_info;
           
           // Remove from clients list
           pthread_mutex_lock(&g_tcp_clients_mutex);
           g_tcp_clients.erase( remove_if(g_tcp_clients.begin(), g_tcp_clients.end(),
            [client_info](const TcpClientInfo& client) { return client.client_id == client_info->client_id; }),
            g_tcp_clients.end());
           pthread_mutex_unlock(&g_tcp_clients_mutex);
         }
       }
       
       // Cleanup (this code will never be reached in current implementation)
       close(server_fd);
       pthread_mutex_destroy(&g_tcp_clients_mutex);
       pthread_mutex_destroy(&g_tcp_stats_mutex);
       
       return 0;
}





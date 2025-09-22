#include "common.h"

using namespace std;

// Global variables
vector<ClientInfo> clients;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t stats_mutex = PTHREAD_MUTEX_INITIALIZER;
ServerStats server_stats;
int next_client_id = 1;

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

void send_message(int socket_fd, const Message& msg) {
       // Send the message structure
       ssize_t bytes_sent = send(socket_fd, &msg, sizeof(Message), 0);
       if (bytes_sent == -1) {
         print_debug("Failed to send message to client");
       } else {
         print_debug("Sent " + to_string(bytes_sent) + " bytes to client");
       }
}

Message receive_message(int socket_fd) {
       Message msg;
       ssize_t bytes_received = recv(socket_fd, &msg, sizeof(Message), 0);
       
       if (bytes_received <= 0) {
         msg.type = MSG_CHAT; // Invalid message type to indicate error
         msg.client_id = -1;
         print_debug("Failed to receive message or client disconnected");
       } else {
         print_debug("Received " + to_string(bytes_received) + " bytes from client");
       }
       
       return msg;
}

void broadcast_message(const Message& msg, int exclude_client_id) {
       pthread_mutex_lock(&clients_mutex);
       
       print_debug("Broadcasting to " + to_string(clients.size()) + " clients, excluding " + to_string(exclude_client_id));
       
       for (const auto& client : clients) {
         if (client.client_id != exclude_client_id) {
           print_debug("Sending message to client " + to_string(client.client_id));
           send_message(client.socket_fd, msg);
         }
       }
       
       pthread_mutex_unlock(&clients_mutex);
}

void* handle_client(void* arg) {
       ClientInfo* client_info = static_cast<ClientInfo*>(arg);
       int client_socket = client_info->socket_fd;
       int client_id = client_info->client_id;
       
       print_debug("Client " + to_string(client_id) + " connected from " + 
             client_info->client_ip + ":" + to_string(client_info->client_port));
       
       while (true) {
         Message msg = receive_message(client_socket);
         
         if (msg.client_id == -1) {
           // Client disconnected
           break;
         }
         
         msg.client_id = client_id; // Ensure correct client ID
         
         switch (msg.type) {
           case MSG_CHAT: {
                // Broadcast chat message to all other clients
                string chat_msg = "[" + get_timestamp() + "] Client " + 
                                  to_string(client_id) + ": " + msg.content;
             
             Message broadcast_msg;
             broadcast_msg.type = MSG_CHAT;
             broadcast_msg.client_id = client_id;
             strncpy(broadcast_msg.content, chat_msg.c_str(), sizeof(broadcast_msg.content) - 1);
             broadcast_msg.content_length = strlen(broadcast_msg.content);
             
             broadcast_message(broadcast_msg, client_id);
             print_debug("Broadcasted message from client " + to_string(client_id));
             break;
           }
           
           case MSG_STATS: {
             // Send server statistics to requesting client
             pthread_mutex_lock(&stats_mutex);
             pthread_mutex_lock(&clients_mutex);
             
              string stats_msg = string("Server Statistics:\n") +
                         " Clients connected: " + to_string(clients.size()) + "\n" +
                         " Server uptime: " + to_string((int)server_stats.get_uptime_seconds()) + " seconds";
             
             Message response_msg;
             response_msg.type = MSG_STATS;
             response_msg.client_id = 0; // Server response
                strncpy(response_msg.content, stats_msg.c_str(), sizeof(response_msg.content) - 1);
                response_msg.content_length = strlen(response_msg.content);
             
             send_message(client_socket, response_msg);
             
             pthread_mutex_unlock(&clients_mutex);
             pthread_mutex_unlock(&stats_mutex);
             print_debug("Sent stats to client " + to_string(client_id));
             break;
           }
           
           default:
             print_debug("Unknown message type from client " + to_string(client_id));
             break;
         }
       }
       
       // Client disconnected - remove from clients list
       pthread_mutex_lock(&clients_mutex);
       clients.erase( remove_if(clients.begin(), clients.end(),
         [client_id](const ClientInfo& client) { return client.client_id == client_id; }),
         clients.end());
       pthread_mutex_unlock(&clients_mutex);
       
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
         ClientInfo* client_info = new ClientInfo(client_socket, next_client_id++, client_ip, client_port);
         
         // Add client to list
         pthread_mutex_lock(&clients_mutex);
         clients.push_back(*client_info);
         pthread_mutex_unlock(&clients_mutex);
         
         // Create thread for this client
         if (pthread_create(&client_info->thread_id, NULL, handle_client, client_info) != 0) {
            cerr << "Failed to create thread for client!" << endl;
           close(client_socket);
           delete client_info;
           
           // Remove from clients list
           pthread_mutex_lock(&clients_mutex);
           clients.erase( remove_if(clients.begin(), clients.end(),
             [client_info](const ClientInfo& client) { return client.client_id == client_info->client_id; }),
             clients.end());
           pthread_mutex_unlock(&clients_mutex);
         }
       }
       
       // Cleanup (this code will never be reached in current implementation)
       close(server_fd);
       pthread_mutex_destroy(&clients_mutex);
       pthread_mutex_destroy(&stats_mutex);
       
       return 0;
}

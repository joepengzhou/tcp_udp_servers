#include "TCPCommon.h"
#include <fcntl.h>
#include <errno.h>

using namespace std;

// Global variables for client
int client_socket = -1;
bool client_running = true;
pthread_mutex_t client_mutex = PTHREAD_MUTEX_INITIALIZER;

void print_debug(const string& message) {
    cerr << "[DEBUG] " << message << endl;
}

string get_timestamp() {
    auto now = chrono::system_clock::now();
    auto time_t = chrono::system_clock::to_time_t(now);
    auto ms = chrono::duration_cast<chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    stringstream ss;
    ss << put_time(localtime(&time_t), "%H:%M:%S");
    ss << '.' << setfill('0') << setw(3) << ms.count();
    return ss.str();
}

void send_message(const TcpMessage& msg) {
    pthread_mutex_lock(&client_mutex);
    if (client_socket != -1) {
        ssize_t bytes_sent = send(client_socket, &msg, sizeof(TcpMessage), 0);
        if (bytes_sent == -1) {
            print_debug("Failed to send message to server");
        } else {
            print_debug("Sent " + to_string(bytes_sent) + " bytes to server");
        }
    }
    pthread_mutex_unlock(&client_mutex);
}

TcpMessage receive_message() {
    TcpMessage msg;
    pthread_mutex_lock(&client_mutex);
    if (client_socket != -1) {
        // Set socket to non-blocking mode
        int flags = fcntl(client_socket, F_GETFL, 0);
        fcntl(client_socket, F_SETFL, flags | O_NONBLOCK);
        
        ssize_t bytes_received = recv(client_socket, &msg, sizeof(TcpMessage), 0);
        
        // Restore blocking mode
        fcntl(client_socket, F_SETFL, flags);
        
        if (bytes_received <= 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // No data available, return empty message
                msg.client_id = 0;
                msg.type = MSG_CHAT;
                memset(msg.payload, 0, sizeof(msg.payload));
            } else {
                msg.type = MSG_CHAT; // Invalid message type to indicate error
                msg.client_id = -1;
            }
        } else {
            print_debug("Received message: type=" + to_string(msg.type) + ", client_id=" + to_string(msg.client_id) + ", content=" + string(msg.payload));
        }
    } else {
        msg.client_id = -1;
    }
    pthread_mutex_unlock(&client_mutex);
    return msg;
}

void* receive_thread(void* /*arg*/) {
    print_debug("Receive thread started");
    
    while (client_running) {
        TcpMessage msg = receive_message();
        
        if (msg.client_id == -1) {
            // Connection lost
            cout << "\n[SYSTEM] Connection to server lost!" << endl;
            client_running = false;
            break;
        } else if (msg.client_id == 0 && strlen(msg.payload) == 0) {
            // No message received, just continue
            usleep(100000); // Sleep for 100ms to avoid busy waiting
            continue;
        }
        
        // Display received message
        cout << "\n[RECEIVED] " << msg.payload << endl;
        cout << "Enter command (/say <text> or /stats): ";
        cout.flush();
    }
    
    print_debug("Receive thread ended");
    return nullptr;
}

void* input_thread(void* /*arg*/) {
    print_debug("Input thread started");
    
    string input;
    cout << "TCP Chat Client Connected!" << endl;
    cout << "Commands:" << endl;
    cout << "  /say <text>  - Send chat message" << endl;
    cout << "  /stats       - Request server statistics" << endl;
    cout << "  /quit        - Disconnect from server" << endl;
    cout << "Enter command (/say <text> or /stats): ";
    cout.flush();
    
    while (client_running) {
        getline(cin, input);
        
        if (!client_running) break;
        
        if (input.empty()) {
            cout << "Enter command (/say <text> or /stats): ";
            cout.flush();
            continue;
        }
        
        TcpMessage msg;
        
        if (input.substr(0, 5) == "/say ") {
            // Send chat message
            string message_text = input.substr(5);
            if (message_text.empty()) {
                cout << "Please provide a message after /say" << endl;
                cout << "Enter command (/say <text> or /stats): ";
                cout.flush();
                continue;
            }
            
            msg.type = MSG_CHAT;
            msg.client_id = 0; // Will be set by server
            strncpy(msg.payload, message_text.c_str(), sizeof(msg.payload) - 1);
            msg.payload_length = strlen(msg.payload);
            
            send_message(msg);
            cout << "Enter command (/say <text> or /stats): ";
            cout.flush();
            
        } else if (input == "/stats") {
            // Request server statistics
            msg.type = MSG_STATS;
            msg.client_id = 0;
            msg.payload_length = 0;
            memset(msg.payload, 0, sizeof(msg.payload));
            
            send_message(msg);
            cout << "Enter command (/say <text> or /stats): ";
            cout.flush();
            
        } else if (input == "/quit") {
            // Disconnect from server
            cout << "Disconnecting from server..." << endl;
            client_running = false;
            break;
            
        } else {
            cout << "Unknown command. Use /say <text> or /stats" << endl;
            cout << "Enter command (/say <text> or /stats): ";
            cout.flush();
        }
    }
    
    print_debug("Input thread ended");
    return nullptr;
}

int main(int argc, char* argv[]) {
    string server_ip = "127.0.0.1";
    int port = 5000;
    
    // Parse command line arguments
    if (argc > 1) {
        server_ip = argv[1];
    }
    if (argc > 2) {
        port = atoi(argv[2]);
        if (port <= 0 || port > 65535) {
            cerr << "Invalid port number. Using default port 5000." << endl;
            port = 5000;
        }
    }
    
    struct sockaddr_in server_addr;
    
    // Create socket
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket < 0) {
        cerr << "Socket creation failed!" << endl;
        exit(EXIT_FAILURE);
    }
    
    // Configure server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, server_ip.c_str(), &server_addr.sin_addr) <= 0) {
        cerr << "Invalid server IP address!" << endl;
        exit(EXIT_FAILURE);
    }
    
    // Connect to server
    if (connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        cerr << "Connection failed!" << endl;
        exit(EXIT_FAILURE);
    }
    
    cout << "Connected to server " << server_ip << ":" << port << endl;
    
    // Create threads
    pthread_t receive_tid, input_tid;
    
    if (pthread_create(&receive_tid, NULL, receive_thread, NULL) != 0) {
        cerr << "Failed to create receive thread!" << endl;
        close(client_socket);
        exit(EXIT_FAILURE);
    }
    
    if (pthread_create(&input_tid, NULL, input_thread, NULL) != 0) {
        cerr << "Failed to create input thread!" << endl;
        client_running = false;
        close(client_socket);
        exit(EXIT_FAILURE);
    }
    
    // Wait for threads to complete
    pthread_join(receive_tid, NULL);
    pthread_join(input_tid, NULL);
    
    // Cleanup
    close(client_socket);
    pthread_mutex_destroy(&client_mutex);
    
    cout << "Client disconnected." << endl;
    return 0;
}





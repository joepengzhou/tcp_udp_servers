
#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string>
#include <arpa/inet.h>
#include <cstring>

using namespace std;

int main()
{
    int server_fd, new_socket, nBytes, port=12345, opt=1;
    char buffer[1024]={0};
    struct sockaddr_in serverAddr;
    int addrlen = sizeof(serverAddr);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1)
    {
        cout << "Socket creation failed!!" << endl;
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
    {
        cout << "Socket setsockopt error!" << endl;
        exit(EXIT_FAILURE);
    }

    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    memset(serverAddr.sin_zero, '\0', sizeof(serverAddr.sin_zero));

    if (bind(server_fd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
    {
        cout << "Bind failed!!" << endl;
        exit(EXIT_FAILURE);
    }

    cout << "Listen..." << endl;

    if (listen(server_fd, 3) < 0)
    {
        cout << "Listen failed!!" << endl;
        exit(EXIT_FAILURE);
    }

    if ((new_socket = accept(server_fd, (struct sockaddr *)&serverAddr, (socklen_t*)&addrlen)) < 0)
    {
        cout << "Accept failed!" << endl;
        exit(EXIT_FAILURE);
    }

    do {
        memset(buffer, 0, sizeof(buffer));
        nBytes = read(new_socket, buffer, 1024);
        cout << "I received: " << buffer << endl;

        if (send(new_socket, buffer, nBytes, 0) == -1)
        {
            cout << "Send failed!" << endl;
            close(new_socket);
            exit(EXIT_FAILURE);
        }

        if (strcmp(buffer, "Quit!") == 0)
        {
            close(new_socket);
        }

    } while(strcmp(buffer, "Quit!") != 0);

    cout << "Exit..." << endl;
    exit(EXIT_SUCCESS);
}


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
    int TCPsocket = 0;
    int port = 12345;
    struct sockaddr_in serverAddr;
    char buffer[1024] = {0};

    if ((TCPsocket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        cout << "Socket creation failed!!" << endl;
        exit(EXIT_FAILURE);
    }

    cout << "Please enter TCP server IP address: ";
    cin.getline(buffer, 1024, '\n');
    serverAddr.sin_addr.s_addr = inet_addr(buffer);
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    memset(serverAddr.sin_zero, 0, sizeof(serverAddr.sin_zero));

    if (connect(TCPsocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
    {
        cout << "Connection failed!!" << endl;
        exit(EXIT_FAILURE);
    }

    do {
        memset(buffer, 0, sizeof(buffer));
        cout << "Message to server: ";
        cin.getline(buffer, 1024, '\n');

        if (send(TCPsocket, buffer, strlen(buffer), 0) == -1)
        {
            cout << "Send failed!" << endl;
            exit(EXIT_FAILURE);
        }

        memset(buffer, 0, sizeof(buffer));
        read(TCPsocket, buffer, 1024);
        cout << "Server echoed: " << buffer << endl;

    } while(strcmp(buffer, "Quit!") != 0);

    close(TCPsocket);
    exit(EXIT_SUCCESS);
}

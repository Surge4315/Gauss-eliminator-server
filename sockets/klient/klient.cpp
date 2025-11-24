#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string>
#include <termios.h>
#include <thread>
using namespace std;

void receiveData(int clientSocket)
{
    char buffer[4096];
    int bytesReceived;
    while ((bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0)) > 0) //recv blocks so checks infinitely
    {
        buffer[bytesReceived] = '\0';
        cout << buffer << endl;
        fflush(stdout);
    }
}

char getch()
{
    // evil ass function to capture single key press
    char buf = 0;
    termios old = {0};
    if (tcgetattr(STDIN_FILENO, &old) < 0) // saves old terminal config, if lower than 0 you get error
        perror("tcgetattr()");
    termios newt = old;
    newt.c_lflag &= ~(ICANON | ECHO);                // turn off canonical mode & echo by inverting boolean bits using tilde
    if (tcsetattr(STDIN_FILENO, TCSANOW, &newt) < 0) // creates new terminal config
        perror("tcsetattr()");
    if (read(STDIN_FILENO, &buf, 1) < 0) // read one byte
        perror("read()");
    tcsetattr(STDIN_FILENO, TCSANOW, &old); // restore settings
    return buf;
}

int main()
{
    // creating socket
    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);

    // specifying address
    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(5000);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    // sending connection request
    connect(clientSocket, (struct sockaddr *)&serverAddress,
            sizeof(serverAddress));


    char option;

    thread receiver(receiveData, clientSocket);
    receiver.detach(); // run independently

    cout << "1. 10\n2. 100\n3. 1000\n4. 10 000\n5. 10 000 with processes\nq. quit\n";
    // sending data
    while (1)
    {
        option = getch();
        switch (option)
        {
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
            send(clientSocket, &option, 1, 0);
            break;
        case 'q':
        case 'Q':
            send(clientSocket, &option, 1, 0);
            close(clientSocket); // closing the socket
            return 0;
        default:
            continue;
        }
    }
    

    return 0;
}
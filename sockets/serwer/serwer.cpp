#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include "macierz.h"
#include "gauss.h"
#include "gauss_process.h"

using namespace std;

int main()
{
    // creating socket
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);

    // specifying the address
    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(5000);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    //setting up so can be used immediately after program restart
    int opt = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    // binding socket.
    bind(serverSocket, (struct sockaddr *)&serverAddress,
         sizeof(serverAddress));

    // listening to the assigned socket
    listen(serverSocket, 5);

    // accepting connection request
    int clientSocket = accept(serverSocket, nullptr, nullptr);

    // recieving data
    while (1)
    {
        char option;
        recv(clientSocket, &option, 1, 0);
        vector<vector<double>> augmented;
        switch (option)
        {
        case '1':
        {
            cout << "Rozmiar 10" << endl;
            send(clientSocket, "Rozmiar 10", strlen("Rozmiar 10"), 0);
            usleep(1000); // so everything gets sent properly
            augmented = createAugmentedMatrix(generateDiagonallyDominantMatrix(10), generateVectorB(10));
            gaussianElimination(augmented, clientSocket);
            gaussianElimination_p(augmented, clientSocket);
            cout << endl;
            break;
        }
        case '2':
        {
            cout << "Rozmiar 100" << endl;
            send(clientSocket, "Rozmiar 100", strlen("Rozmiar 100"), 0);
            augmented = createAugmentedMatrix(generateDiagonallyDominantMatrix(100), generateVectorB(100));
            gaussianElimination(augmented, clientSocket);
            gaussianElimination_p(augmented, clientSocket);
            cout << endl;
            break;
        }
        case '3':
        {
            cout << "Rozmiar 1000" << endl;
            send(clientSocket, "Rozmiar 1000", strlen("Rozmiar 1000"), 0);
            augmented = createAugmentedMatrix(generateDiagonallyDominantMatrix(1000), generateVectorB(1000));
            gaussianElimination(augmented, clientSocket);
            gaussianElimination_p(augmented, clientSocket);
            cout << endl;
            break;
        }
        case '4':
        {

            cout << "Rozmiar 10 000\nz 40 minut to zajmie" << endl;
            send(clientSocket, "Rozmiar 10 000\nz 40 minut to zajmie", strlen("Rozmiar 10 000\nz 40 minut to zajmie"), 0);
            augmented = createAugmentedMatrix(generateDiagonallyDominantMatrix(10000), generateVectorB(10000));
            gaussianElimination(augmented, clientSocket);
            break;
        }
        case '5':
        {
            cout << "Rozmiar 10 000\nz 40 minut to zajmie" << endl;
            send(clientSocket, "Rozmiar 10 000\nz 40 minut to zajmie", strlen("Rozmiar 10 000\nz 40 minut to zajmie"), 0);
            augmented = createAugmentedMatrix(generateDiagonallyDominantMatrix(10000), generateVectorB(10000));
            gaussianElimination_p(augmented, clientSocket);
            cout << endl;
            break;
        }
        case 'q':
        case 'Q':
            cout << "Quitting..." << endl;
            close(clientSocket); // closing the client connection
            close(serverSocket); // closing the server socket
            return 0;
        default:
            continue;
        }
    }
    return 0;
}
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>

struct client{
    int FD;
    char username[8];
};

struct client clients[100];
int numofClients = 0;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

void broadcast(char* message, struct client sender){
    pthread_mutex_lock(&clients_mutex);
    char msg[1030];
    snprintf(msg, sizeof(msg), "%s: %s", sender.username, message);
    printf("%s",msg);
     for (int i = 0; i < numofClients; i++) {
        if (clients[i].FD != sender.FD) {
            send(clients[i].FD, msg, strlen(msg), 0);
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

void* welcome_client(void* FD){
    struct client Client;
    Client.FD = *(int*)FD;
    free(FD);
    char buffer[1024];
    char* message = "Enter a username: ";

    send(Client.FD, message, strlen(message), 0);
    memset(buffer, 0, sizeof(buffer));
    ssize_t bytes = recv(Client.FD, buffer, sizeof(buffer)-1, 0);
    buffer[bytes-1] = '\0';
    strcpy(Client.username, buffer);
    
    pthread_mutex_lock(&clients_mutex);
    clients[numofClients++] = Client;
    pthread_mutex_unlock(&clients_mutex);

    while(1){
        memset(buffer, 0, sizeof(buffer));
        ssize_t bytes = recv(Client.FD, buffer, sizeof(buffer)-1, 0);
        buffer[bytes] = '\0';

        if(bytes <= 0){
            printf("Client disconnected, fd = %d\n", Client.FD);
            break;
        }
        
        printf("Client %d: %s\n", Client.FD, buffer);
        broadcast(buffer, Client);
    }

    pthread_mutex_lock(&clients_mutex);
    struct client* temp = clients;
    for (int i = 0; i < numofClients; i++) {
        if (clients[i].FD == Client.FD) {
            clients[i] = clients[numofClients - 1];
            numofClients--;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);

    close(Client.FD);

}

int main(int argc, char const* argv[])
{
    int serverFD;
    struct sockaddr_in address;
    int opt = 1;
    socklen_t addrlen = sizeof(address);

    if ((serverFD = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(serverFD, SOL_SOCKET,
                   SO_REUSEADDR, &opt,
                   sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(3000);

    if (bind(serverFD, (struct sockaddr*)&address,
             sizeof(address))
        < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    if (listen(serverFD, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    
    while(1){
    int clientFD;

    if ((clientFD
         = accept(serverFD, (struct sockaddr*)&address,
                  &addrlen))
        < 0) {
        perror("accept");
        exit(EXIT_FAILURE);
    }

    printf("New client connected, fd = %d\n", clientFD);

    int* clientPtr = malloc(sizeof(int));
    *clientPtr = clientFD;

    pthread_t threadID;

    pthread_create(&threadID, NULL, welcome_client, clientPtr);

    pthread_detach(threadID);
    }
  
    close(serverFD);
    return 0;
}
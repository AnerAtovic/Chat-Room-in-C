#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>

typedef struct{
    int FD;
    char username[17];
} Client;

Client clients[100];
int numofClients = 0;
pthread_mutex_t clientsMutex = PTHREAD_MUTEX_INITIALIZER;

void broadcast(char* message, Client sender, bool option){
    pthread_mutex_lock(&clientsMutex);

    char msg[1030];
    snprintf(msg, sizeof(msg), "%s%s %s", sender.username, option ? "" : ":", message);

    for (int i = 0; i < numofClients; i++){
        if (clients[i].FD != sender.FD) {
            send(clients[i].FD, msg, strlen(msg), 0);
        }
    }

    pthread_mutex_unlock(&clientsMutex);
}

void addClient(Client client){
    pthread_mutex_lock(&clientsMutex);
    clients[numofClients++] = client;
    pthread_mutex_unlock(&clientsMutex);
}

void setUsername(Client* client){
    char buffer[17];
    char* message = "Welcome! Enter a username (max 16 characters): ";
    send(client->FD, message, strlen(message), 0);
    memset(buffer, 0, sizeof(buffer));
    ssize_t bytes = recv(client->FD, buffer, sizeof(buffer)-1, 0);
    if (bytes <= 0){
        strcpy(client->username, "guest");
        return;
    }
    buffer[bytes] = '\0';
    buffer[strcspn(buffer, "\r\n")] = '\0';
    strncpy(client->username, buffer, 16);
    client->username[16] = '\0';
    broadcast("connected!", *client, 1);
}

void disconnectClient(Client* client){
    pthread_mutex_lock(&clientsMutex);
    for (int i = 0; i < numofClients; i++) {
        if (clients[i].FD == client->FD) {
            clients[i] = clients[numofClients - 1];
            numofClients--;
            break;
        }
    }
    pthread_mutex_unlock(&clientsMutex);
    broadcast("disconnected!", *client, 1);
    close(client->FD);
    free(client);
}

void* chat(void* arg){
    Client* client = (Client*)arg;
    char buffer[1024];

    setUsername(client);
    addClient(*client);

    while(1){
        memset(buffer, 0, sizeof(buffer));
        ssize_t bytes = recv(client->FD, buffer, sizeof(buffer) - 1, 0);
        if (bytes <= 0) break;
        buffer[bytes] = '\0';

        if(bytes <= 0){
            printf("Client disconnected, fd = %d\n", client->FD);
            break;
        }
        
        printf("Client %d: %s\n", client->FD, buffer);
        broadcast(buffer, *client, 0);
    }

    disconnectClient(client);
}

int main(int argc, char const* argv[]){
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
    Client client;

    if ((client.FD
         = accept(serverFD, (struct sockaddr*)&address,
                  &addrlen))
        < 0) {
        perror("accept");
        exit(EXIT_FAILURE);
    }

    printf("New client connected, fd = %d\n", client.FD);

    Client* clientPtr = malloc(sizeof(Client));
    *clientPtr = client;

    pthread_t threadID;
    pthread_create(&threadID, NULL, chat, clientPtr);
    pthread_detach(threadID);
    }
  
    close(serverFD);
    return 0;
}
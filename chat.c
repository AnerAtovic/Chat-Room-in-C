#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>

//add /cmds, different users, potentially a log in system

typedef struct{
    int FD;
    char username[17];
    char roomName[17];
} Client;

Client clients[100];
int numOfClients = 0;
pthread_mutex_t clientsMutex = PTHREAD_MUTEX_INITIALIZER;

// basic info about the room, finds users with the same roomName value
void printRoomInfo(Client client){
    pthread_mutex_lock(&clientsMutex);
    printf("entered active fun");
    char listOfUsers[1800];
    int activeUsers = 0;
    memset(listOfUsers, 0, sizeof(listOfUsers));
    for(int i = 0; i < numOfClients; i++){
        if(strncmp(clients[i].roomName, client.roomName, strlen(client.roomName)) == 0){
            printf("%s", clients[i].username);
            snprintf(listOfUsers + strlen(listOfUsers), sizeof(listOfUsers), "%s\n", clients[i].username);
            activeUsers++;
        }
    }
    char *message;
    snprintf(message, 1835, "Room name: %s\nActive users(%d):\n%s", client.roomName, activeUsers, listOfUsers);
    send(client.FD, message, strlen(message), 0);
    

    pthread_mutex_unlock(&clientsMutex);
}

// messages are sent to everyone with the same roomName value, option = 1 removes : from message (until i find a better way to do this)
void broadcast(char* message, Client sender, bool option, char* roomName){
    pthread_mutex_lock(&clientsMutex);

    char msg[1030];
    snprintf(msg, sizeof(msg), "%s%s %s", sender.username, option ? "" : ":", message);

    for (int i = 0; i < numOfClients; i++){
        if (clients[i].FD != sender.FD && strncmp(clients[i].roomName, roomName, 16) == 0) {
            send(clients[i].FD, msg, strlen(msg), 0);
        }
    }

    pthread_mutex_unlock(&clientsMutex);
}

// adds client to the clients array
void addClient(Client client){
    pthread_mutex_lock(&clientsMutex);
    clients[numOfClients++] = client;
    pthread_mutex_unlock(&clientsMutex);
}

// sets username and "places" client into a room
void setUsername(Client* client){
    char buffer[17];
    
    // get username
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

    // get room name
    message = "Enter room name to continue(max 16 characters): ";
    send(client->FD, message, strlen(message), 0);

    memset(buffer, 0, sizeof(buffer));
    bytes = recv(client->FD, buffer, sizeof(buffer)-1, 0);
    while(bytes == sizeof(buffer)){
        send(client->FD, message, strlen(message), 0);
        bytes = recv(client->FD, buffer, sizeof(buffer), 0);
    }

    buffer[bytes] = '\0';
    buffer[strcspn(buffer, "\r\n")] = '\0';
    strncpy(client->roomName, buffer, 16);
    broadcast("connected to your room!\n", *client, 1, client->roomName);
}


// closes client's connection and frees up space in the clients array
void disconnectClient(Client* client){
    pthread_mutex_lock(&clientsMutex);
    for (int i = 0; i < numOfClients; i++) {
        if (clients[i].FD == client->FD) {
            clients[i] = clients[numOfClients - 1];
            numOfClients--;
            break;
        }
    }
    pthread_mutex_unlock(&clientsMutex);
    broadcast("disconnected!\n", *client, 1, client->roomName);
    close(client->FD);
    free(client);
}


// all chatting happens here
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
        
        printf("Client in room %s %d: %s\n", client->roomName, client->FD, buffer);

        if(strncmp(buffer, "/room", bytes-1) == 0 && bytes-1 == 5)
            printRoomInfo(*client);
        else
            broadcast(buffer, *client, 0, client->roomName);

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
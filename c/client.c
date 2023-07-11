#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define SOCKET_PATH "/tmp/lol"

int main() {
    // Create the socket
    int client_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (client_fd == -1) {
        perror("Failed to create socket");
        exit(EXIT_FAILURE);
    }

    // Connect to the server
    struct sockaddr_un server_address;
    server_address.sun_family = AF_UNIX;
    strcpy(server_address.sun_path, SOCKET_PATH);

    if (connect(client_fd, (struct sockaddr*)&server_address, sizeof(server_address)) == -1) {
        perror("Failed to connect to server");
        exit(EXIT_FAILURE);
    }

    printf("Connected to the server\n");

    // Send data to the server
    const char* message = "Hello, server!";
    if (send(client_fd, message, strlen(message), 0) == -1) {
        perror("Failed to send data");
        exit(EXIT_FAILURE);
    }

    printf("Data sent to the server\n");

    // Close the socket
    close(client_fd);

    return 0;
}

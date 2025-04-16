#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 12345
#define BUFFER_SIZE 1024

int main() {
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];

    // Create a TCP socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Enable reusing port quickly after restarting
    int optval = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    // Bind to PORT on all interfaces
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Listen for 1 connection
    if (listen(server_fd, 1) < 0) {
        perror("listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    printf("Echo server listening on port %d...\n", PORT);

    // Accept a single connection
    client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
    if (client_fd < 0) {
        perror("accept");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    printf("Client connected!\n");

    // Echo loop
    while (1) {
        ssize_t bytes_read = recv(client_fd, buffer, BUFFER_SIZE, 0);
        if (bytes_read < 0) {
            perror("recv");
            break;
        }
        if (bytes_read == 0) {
            // Client closed connection
            printf("Client disconnected.\n");
            break;
        }
        // Echo the data back
        ssize_t bytes_sent = send(client_fd, buffer, bytes_read, 0);
        if (bytes_sent < 0) {
            perror("send");
            break;
        }
    }

    close(client_fd);
    close(server_fd);
    return 0;
}

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>

#define SERVER_IP   "15.15.15.1"  // "127.0.0.1" Change to the server VM IP
#define PORT        12345
#define BUFFER_SIZE 1024
#define NUM_TESTS   50  // Number of RTT measurements

// Helper to get time in nanosecond resolution
static double timespec_to_seconds(struct timespec ts) {
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

int main() {
    int sock_fd;
    struct sockaddr_in server_addr;
    char send_buf[BUFFER_SIZE], recv_buf[BUFFER_SIZE];

    // Create a TCP socket
    if ((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Setup server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port   = htons(PORT);
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        perror("inet_pton");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }

    printf("Connecting to server at %s:%d...\n", SERVER_IP, PORT);
    // Connect to the server
    while (connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        sleep(1); // Retry every second
        printf("Retrying connection...\n");
    }
    printf("Connected to server at %s:%d\n", SERVER_IP, PORT);

    // We'll do multiple RTT measurements
    double min_rtt = 999999.0, max_rtt = 0.0, sum_rtt = 0.0;
    int successful_tests = 0;

    for (int i = 0; i < NUM_TESTS; i++) {
        // Prepare a message containing iteration info
        snprintf(send_buf, sizeof(send_buf), "Ping %d", i);

        // Get the timestamp just before sending
        struct timespec start_ts, end_ts;
        clock_gettime(CLOCK_MONOTONIC, &start_ts);

        // Send the message
        ssize_t sent = send(sock_fd, send_buf, strlen(send_buf), 0);
        if (sent < 0) {
            perror("send");
            break;
        }

        // Read the echo
        ssize_t received = recv(sock_fd, recv_buf, sizeof(recv_buf) - 1, 0);
        if (received < 0) {
            perror("recv");
            break;
        } else if (received == 0) {
            printf("Server closed connection.\n");
            break;
        }

        clock_gettime(CLOCK_MONOTONIC, &end_ts);

        // Calculate RTT
        double start_sec = timespec_to_seconds(start_ts);
        double end_sec   = timespec_to_seconds(end_ts);
        double rtt = (end_sec - start_sec) * 1000.0; // in milliseconds

        // Null-terminate the received buffer
        recv_buf[received] = '\0';

        printf("[%d] Sent: \"%s\", Received: \"%s\", RTT = %.3f ms\n",
               i, send_buf, recv_buf, rtt);

        // Update stats
        if (rtt < min_rtt) min_rtt = rtt;
        if (rtt > max_rtt) max_rtt = rtt;
        sum_rtt += rtt;
        successful_tests++;
        
        // Optional small sleep to spread out tests
        usleep(200000); // 200 ms
    }

    // Print summary
    if (successful_tests > 0) {
        double avg_rtt = sum_rtt / successful_tests;
        printf("\nRTT Summary (ms):\n");
        printf("  Tests: %d\n", successful_tests);
        printf("  Min:   %.3f ms\n", min_rtt);
        printf("  Max:   %.3f ms\n", max_rtt);
        printf("  Avg:   %.3f ms\n", avg_rtt);
    }

    close(sock_fd);
    return 0;
}

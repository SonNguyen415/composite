#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include <errno.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <net/route.h>
#include <fcntl.h>

#define PORT 12345
#define BUFFER_SIZE 1024
#define GUEST_IP   "15.15.15.1"
#define GATEWAY    "15.15.15.2"


/* ---------- helpers to print panic-style errors ---------- */
static void die(const char *msg) { perror(msg); exit(1); }

/* ---------- minimal net-setup using classic ioctls -------- */
static void iface_up(const char *name)
{
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    struct ifreq ifr = {0};
    strncpy(ifr.ifr_name, name, IFNAMSIZ);

    /* set address */
    struct sockaddr_in *a = (struct sockaddr_in *)&ifr.ifr_addr;
    a->sin_family = AF_INET;
    inet_pton(AF_INET, GUEST_IP, &a->sin_addr);   /* <- guest IP   */
    if (ioctl(s, SIOCSIFADDR, &ifr) < 0) die("SIOCSIFADDR");

    /* set netmask */
    inet_pton(AF_INET, "255.255.255.0", &a->sin_addr);
    if (ioctl(s, SIOCSIFNETMASK, &ifr) < 0) die("SIOCSIFNETMASK");

    /* bring interface up */
    if (ioctl(s, SIOCGIFFLAGS, &ifr) < 0) die("SIOCGIFFLAGS");
    ifr.ifr_flags |= IFF_UP | IFF_RUNNING;
    if (ioctl(s, SIOCSIFFLAGS, &ifr) < 0) die("SIOCSIFFLAGS");
    close(s);
}

static void add_default_route(const char *gw)
{
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    struct rtentry rt = {0};
    struct sockaddr_in *addr;

    /* destination 0.0.0.0/0 */
    addr = (struct sockaddr_in *)&rt.rt_dst;
    addr->sin_family = AF_INET;
    addr->sin_addr.s_addr = INADDR_ANY;

    addr = (struct sockaddr_in *)&rt.rt_genmask;
    addr->sin_family = AF_INET;
    addr->sin_addr.s_addr = INADDR_ANY;          /* netmask 0 */

    /* gateway */
    addr = (struct sockaddr_in *)&rt.rt_gateway;
    addr->sin_family = AF_INET;
    inet_pton(AF_INET, gw, &addr->sin_addr);      /* e.g. 15.15.15.1 */

    rt.rt_flags = RTF_UP | RTF_GATEWAY;
    if (ioctl(s, SIOCADDRT, &rt) < 0) die("SIOCADDRT");
    close(s);
}

int main() {
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];

    /* Set up networking */
    iface_up("eth0");
    add_default_route(GATEWAY);   /* host TAP address */

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

    while(1);
    return 0;
}

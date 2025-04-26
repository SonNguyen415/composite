#ifndef SHM_CK_RING_H
#define SHM_CK_RING_H

#include <ck_ring.h>

// Define the size of the ring buffer and packet structure
#define SHM_CK_RING_SIZE 1024
#define SHM_CK_BUF_SIZE 2048

// Define the ring buffer size in bytes
#define SHM_CK_RING_BUFFER_SIZE round_up_to_page(sizeof(struct shared_ring_t))

// Define the packet structure (for example purposes)
struct packet_t {
    u16_t len;
    char data[SHM_CK_BUF_SIZE];
};

// Shared memory structure holding the ring buffer
struct shared_ring_t {
    ck_ring_t ring;
    struct packet_t buffer[SHM_CK_RING_SIZE];
};

CK_RING_PROTOTYPE(shm_ring, packet_t);
// Wrapper functions for enqueueing and dequeueing
static inline int enqueue_packet(struct shared_ring_t *shared, struct packet_t *pkt) {
    return CK_RING_ENQUEUE_SPMC(shm_ring, &shared->ring, shared->buffer, pkt);
}

static inline int dequeue_packet(struct shared_ring_t *shared, struct packet_t *pkt) {

    if (CK_RING_DEQUEUE_SPMC(shm_ring, &shared->ring, shared->buffer, pkt)) {
        return 0;
    }
    return -1; // No packet available
}

// Initialize ring buffer in pre-allocated shared memory
static inline int setup_ring_buffer(struct shared_ring_t *shared, size_t size) {
    // Initialize the CK ring structure in the shared memory
    if (size < sizeof(struct shared_ring_t) || shared == NULL) {
        return -1;
    }
    ck_ring_init(&shared->ring, size);
    return 0;
}

#endif /* SHM_CK_RING_H */

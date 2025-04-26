#include <cos_types.h>
#include "vbridge.h"
#include "simple_hash.h"

/* indexed by thread id */
struct session sessions[VBRIDGE_MAX_SESSION];
int g_num_sessions = 0;

int
vbridge_shmem_bind_port(u32_t ip_addr, u16_t port, cbuf_t shmid)
{
	// Allocate shared memory for the inter-VM session
	if (g_num_sessions >= VBRIDGE_MAX_SESSION) {
		printc("Max sessions reached\n");
		return -1;
	}
	int idx = g_num_sessions++;
	sessions[idx].shemem_info.shmid = shmid;
	simple_hash_add(ip_addr, port, &sessions[idx]);
	printc("Session bound: IP %u, Port %u, Shmid %d\n", ip_addr, port, shmid);

	return 0;
}

int
vbridge_get_shmem_id(u32_t ip_addr, u16_t port)
{
	// Get the shared memory ID for the given IP and port
	struct session *session = simple_hash_find(ip_addr, port);
	if (session) {
		printc("Session found: IP %u, Port %u, Shmid %d\n", ip_addr, port, session->shemem_info.shmid);
		return session->shemem_info.shmid;
	}
	return -1;
}


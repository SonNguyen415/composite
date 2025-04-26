#ifndef VBRIDGE_SHMEM_H
#define VBRIDGE_SHMEM_H

#include <cos_types.h>
#include <cos_component.h>
#include <cos_stubs.h>
#include <shm_bm.h>

int vbridge_shmem_bind_port(u32_t ip_addr, u16_t port, cbuf_t shmid);
int vbridge_get_shmem_id(u32_t ip_addr, u16_t port);

#endif // VBRIDGE_SHMEM_H

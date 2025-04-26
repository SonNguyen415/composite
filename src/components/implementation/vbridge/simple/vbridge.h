#ifndef VBRIDGE_H
#define VBRIDGE_H

#include <cos_types.h>
#include <cos_component.h>
#include <simple_hash.h>

#define VBRIDGE_MAX_SESSION 2000

struct shemem_info {
	cbuf_t   shmid;
};

struct session {
	struct shemem_info shemem_info;
};

#endif /* VBRIDGE_H */

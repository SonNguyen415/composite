#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <cos_debug.h>
#include <llprint.h>
#include <cos_component.h>
#include <cos_kernel_api.h>
#include <static_slab.h>
#include <sched.h>
#include <contigmem.h>
#include <shm_bm.h>
#include <vmrt.h>
#include <instr_emul.h>
#include <acrn_common.h>

#include <vlapic.h>
#include <vioapic.h>
#include <sync_lock.h>
#include <cos_time.h>
#include <sync_sem.h>
#include <shm_ck_ring.h>
#include <vbridge.h>
#include <arpa/inet.h>
#include <devices/vpci/virtio_net_io.h>

#define RX_TX_THD_PRIORITY 31

thdid_t rx_tid = 0;
thdid_t tx_tid = 0;

void *g_rx_mem = NULL;
void *g_tx_shmemd = NULL;

INCBIN(vmlinux, "../linux_vm_initframs/vmlinux_echo_server.img")
INCBIN(bios, "guest/guest.img")

/* Currently only have one VM component globally managed by this VMM */
static struct vmrt_vm_comp *g_vm;
static struct vmrt_vm_comp *g_vm1;

struct vmrt_vm_comp *vm_list[2] = {0};

#define VM_MAX_COMPS (2)
#define GUEST_MEM_SZ (310*1024*1024)

SS_STATIC_SLAB(vm_comp, struct vmrt_vm_comp, VM_MAX_COMPS);
SS_STATIC_SLAB(vm_lapic, struct acrn_vlapic, VM_MAX_COMPS * VMRT_VM_MAX_VCPU);
SS_STATIC_SLAB(vcpu_inst_ctxt, struct instr_emul_ctxt, VM_MAX_COMPS * VMRT_VM_MAX_VCPU);
SS_STATIC_SLAB(vm_io_apic, struct acrn_vioapics, VM_MAX_COMPS);
SS_STATIC_SLAB(vcpu_mmio_req, struct acrn_mmio_request, VM_MAX_COMPS * VMRT_VM_MAX_VCPU);

struct sync_lock vm_boot_lock;

void 
pause_handler(struct vmrt_vm_vcpu *vcpu)
{
	sched_thd_yield();
	GOTO_NEXT_INST(vcpu->shared_region);
}

void 
hlt_handler(struct vmrt_vm_vcpu *vcpu)
{
	sched_thd_yield();
	GOTO_NEXT_INST(vcpu->shared_region);
}

void
mmio_init(struct vmrt_vm_vcpu *vcpu)
{
	vcpu->mmio_request = ss_vcpu_mmio_req_alloc();
	assert(vcpu->mmio_request);

	ss_vcpu_mmio_req_activate(vcpu->mmio_request);
}

void
ioapic_init(struct vmrt_vm_comp *vm)
{
	vm->ioapic = ss_vm_io_apic_alloc();
	assert(vm->ioapic);

	vioapic_init(vm);
	ss_vm_io_apic_activate(vm->ioapic);
}

void
iinst_ctxt_init(struct vmrt_vm_vcpu *vcpu)
{
	vcpu->inst_ctxt = ss_vcpu_inst_ctxt_alloc();
	assert(vcpu->inst_ctxt);

	ss_vcpu_inst_ctxt_activate(vcpu->inst_ctxt);
}

void
lapic_init(struct vmrt_vm_vcpu *vcpu)
{
	struct acrn_vlapic *vlapic = ss_vm_lapic_alloc();
	assert(vlapic);

	/* A vcpu can only have one vlapic */
	assert(vcpu->vlapic == NULL);

	vcpu->vlapic = vlapic;
	vlapic->apic_page = vcpu->lapic_page;
	vlapic->vcpu = vcpu;

	/* Status: enable APIC and vector be 0xFF */
	vlapic_reset(vlapic);

	ss_vm_lapic_activate(vlapic);
	return;
}

struct vmrt_vm_comp *
vm_comp_create(void)
{
	u64_t guest_mem_sz = GUEST_MEM_SZ;
	u64_t num_vcpu = 1;
	void *start;
	void *end;
	cbuf_t shm_id;
	void  *mem, *vm_mem;
	size_t sz;

	struct vmrt_vm_comp *vm = ss_vm_comp_alloc();
	assert(vm);
	
	vmrt_vm_create(vm, "vmlinux-5.15", num_vcpu, guest_mem_sz);

	/* Allocate memory for the VM */
	shm_id	= memmgr_shared_page_allocn_aligned(guest_mem_sz / PAGE_SIZE_4K, PAGE_SIZE_4K, (vaddr_t *)&mem);
	/* Make the memory accessible to VM */
	int ret = memmgr_shared_page_map_aligned_in_vm(shm_id, PAGE_SIZE_4K, (vaddr_t *)&vm_mem, vm->comp_id);
	vmrt_vm_mem_init(vm, mem);
	printc("created VM with %u cpus, memory size: %luMB, at host vaddr: %p\n", vm->num_vcpu, vm->guest_mem_sz/1024/1024, vm->guest_addr);

	ss_vm_comp_activate(vm);

	start = &incbin_bios_start;
	end = &incbin_bios_end;
	sz = end - start + 1;

	printc("BIOS image start: %p, end: %p, size: %lu(%luKB)\n", start, end, sz, sz/1024);
	vmrt_vm_data_copy_to(vm, start, sz, PAGE_SIZE_4K);
	
	start = &incbin_vmlinux_start;
	end = &incbin_vmlinux_end;
	sz = end - start + 1;

	printc("Guest Linux image start: %p, end: %p, size: %lu(%luMB)\n", start, end, sz, sz/1024/1024);
	#define GUEST_IMAGE_ADDR 0x100000
	vmrt_vm_data_copy_to(vm, start, sz, GUEST_IMAGE_ADDR);

	ioapic_init(vm);

	printc("Guest(%s) image has been loaded into the VM component\n", vm->name);

	return vm;
}


/* Summary for rx_task:
 * Receives packets from the NIC and sends them to the VMM
 * Sets up the shared memory for the NIC to receive (dpdk)
 * 
 * Processes incoming packets in a loop fetches from the NIC to shared memory and then to the virtio net device
 */

/* TODO ESMA: Why Port is 0 and 1 and IP setted up seperately?
 * Port 0 is for RX and Port 1 is for TX
 */
static void
rx_task(void)
{	
	struct packet_t rx_pkt;
	while (1) {
		while(dequeue_packet(g_rx_mem, &rx_pkt)) {
			//printc("#VM(%u): rx_task: received packet of length %u\n", g_vm->vm_ip, rx_pkt.len);
			virtio_net_rcv_one_pkt(rx_pkt.data, rx_pkt.len);
		}
	}
}

/* Summary for tx_task:
 * Sets up the shared memory using ip and port of the receiver
 * Receives packets from the VMM and copies them into shared memory
 */
static void
tx_task(void)
{
	while (1)
	{
		// Make the shared memory a ring buffer
		struct packet_t tx_pkt;
		virtio_net_send_one_pkt(tx_pkt.data, &tx_pkt.len);

		if (tx_pkt.len > 0) {
			printc("#VM(%u): tx_task: sent packet of length %u\n", g_vm->vm_ip, tx_pkt.len);
			while(enqueue_packet(g_tx_shmemd, &tx_pkt)) {
				printc("tx_task: enqueue failed, retrying...\n");
			}
		}
	}
}

void
cos_init(void)
{
	struct vmrt_vm_vcpu *vcpu;
	g_vm = vm_comp_create();
	printc("created vm done:%d, %p\n", g_vm->comp_id, g_vm);
	g_vm->vm_mac_id = 0;
	g_vm->vm_ip = inet_addr("15.15.15.1");
	printc("created vm done:%d], %p, IP:%u\n", g_vm->comp_id, g_vm, g_vm->vm_ip);
	vm_list[0] = g_vm;
}

void
cos_parallel_init(coreid_t cid, int init_core, int ncores)
{
	struct vmrt_vm_vcpu *vcpu;

	if (cid == 0) {
		vmrt_vm_vcpu_init(g_vm, 0);
		vcpu = vmrt_get_vcpu(g_vm, 0);

		lapic_init(vcpu);
		iinst_ctxt_init(vcpu);
		mmio_init(vcpu);

		/* Init tx and rx threads */
		rx_tid = sched_thd_create((void *)rx_task, NULL);
		cbuf_t shm_id = contigmem_shared_alloc_aligned(SHM_CK_RING_BUFFER_SIZE/PAGE_SIZE, SHM_BM_ALIGN, (vaddr_t *)&g_rx_mem);
		setup_ring_buffer(g_rx_mem, SHM_CK_RING_BUFFER_SIZE);
		printc("#VM(%u): shm_id for rx_task is %d\n", g_vm->vm_ip, shm_id);
		vbridge_shmem_bind_port(g_vm->vm_ip, 0, shm_id);
		tx_tid = sched_thd_create((void *)tx_task, NULL);		
		printc("cos_parallel init vm vcpu done\n");
	}

	return;
}

void
parallel_main(coreid_t cid)
{
	struct vmrt_vm_vcpu *vcpu;
	cbuf_t shm_id;
	int target_ip = inet_addr("15.15.15.2");
	shm_id = vbridge_get_shmem_id(target_ip, 0);
	//printc("#VM(%u): shm_id for tx_task is %d\n", g_vm->vm_ip, shm_id);
	unsigned long npages = memmgr_shared_page_map_aligned(shm_id, SHM_BM_ALIGN, (vaddr_t *)&g_tx_shmemd);
	assert(g_tx_shmemd);

	/* DPDK rx and tx will only run on core 0 */
	if(cid == 0) {
		sched_thd_block_timeout(0, time_now() + time_usec2cyc(20000000));
		vcpu = vmrt_get_vcpu(g_vm, 0);
		vmrt_vm_vcpu_start(vcpu);

		sched_thd_param_set(rx_tid, sched_param_pack(SCHEDP_PRIO, RX_TX_THD_PRIORITY));
		sched_thd_param_set(tx_tid, sched_param_pack(SCHEDP_PRIO, RX_TX_THD_PRIORITY));
	} 

	while (1) {
		sched_thd_block(0);
		/* Should not be here, or there is a bug in the scheduler! */
		assert(0);
	}
}

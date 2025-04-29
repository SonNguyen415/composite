# Advanced OS Summary

## Design and Summary
We used Xinyu's vmm in vmm and a copy, vmm_client. Each VMM runs a linux image that Esma compiles into `src/components/implementation/simple_vmm/linux_vm_initframs`. Their source code can also be viewed there. The vm consists of a client connecting to the server via network socket, then sending messages to the server, which echoes it back. The client calculates this rtt, providing the resulting minimum, maximum, and average. 

The shared memory regions are bound to each `vmm` by the `vbridge` component, which [insert stuff here]. Each shmem consists of a ring buffer provided by `ck`. 

The VMM's IP addresses are hard coded. Each VMM consists of 2 threads, `tx` and `rx`. `tx` repeatedly copies data from virtio into a packet, then enqueue it in a ring buffer. `rx` dequeues from the ring buffer and then copy it back to the virtio.

## Instructions
- We ran our tests via `vmm_multi_test.toml` 
- Component `vmm` uses `simple_vmm.vmm`, it runs the server `echo_server.c` with an IP of `15.15.15.1` at port `12345`
- Component `vmm1` uses `simple_vmm.vmm_client`, it runs the client `rtt_client.c` with an IP of `15.15.15.2`


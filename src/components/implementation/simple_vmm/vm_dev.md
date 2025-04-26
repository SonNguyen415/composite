# VM Development Guide

---

## How to Build the System

### 1. Build Guest Linux VM

### Linux VM Branch

This is the stock Linux 5.15 used as the VM within Composite hypervisor:
[https://github.com/betahxy/vmx-linux-5.15.107](https://github.com/betahxy/vmx-linux-5.15.107)

The Linux kernel source code has been modified slightly for a simplified guest bootloader. The booter resides in `arch/x86/vmxbooter/` and supports multiboot2-compatible boot loaders like Qemu.

Install necessary packages to build linux kernel:

```
sudo apt install libncurses-dev gawk flex bison openssl libssl-dev dkms libelf-dev libudev-dev libpci-dev libiberty-dev autoconf llvm
```

#### Prepare the Initramfs Directory

1. Create the `initramfs` directory under a directory of your own wish and add an `init.c` file.

   ```c
   #include <stdio.h>
   int main() {
       printf("Hello Initramfs in special bootloader!\n");
       sleep(10);
       return 0;
   }
   ```

2. Compile the `init.c` file:

   ```bash
   gcc -o init -static -s init.c
   ```

3. Create the necessary virtual console devices: under the initframs directory

   ```bash
   mkdir dev
   sudo mknod dev/console c 5 1
   sudo mknod dev/null c 1 3
   sudo mknod dev/random c 1 8
   sudo mknod dev/urandom c 1 9
   ```

#### Build the Kernel

1. Use the provided `.config` file for configuration (under the kernel directory). This .config file is a predefined kernel configuration tailored for running Linux as a guest inside the Composite hypervisor. 

2. Enter the configuration menu and adjust paths:

   ```bash
   make menuconfig
   ```

   - Set `CONFIG_INITRAMFS_SOURCE` to the path of the `initramfs` directory. 
     // menu item is here -> General setup -->Initramfs source file(s)

3. Build the kernel:

   ```bash
   make
   ```

   - If multicore builds crash (make -j), use a single-threaded build.

4. Generate the boot image:

   ```bash
   cd arch/x86/vmxbooter/
   make
   ```

5. Test with Qemu:

   ```bash
   make run
   ```

   - If it crashes due to insufficient `/dev/tun` permissions, either:
     ```bash
     sudo make run
     ```
     or
     ```bash
     sudo chmod 666 /dev/net/tun
     ```
   
   - To exit Qemu, press `Ctrl + A` followed by `X`.
  

6. You compiled the kernel and generated image successfully. The generated image is located in `arch/x86/vmxbooter/vmlinux.img`. This image is a compatible boot image that can be loaded by the Composite.

---

### 2. Build the Composite Hypervisor

### Composite Hypervisor Branch

Use this branch to develop hypervisor features:
[https://github.com/betahxy/composite/tree/cos\_vmx](https://github.com/betahxy/composite/tree/cos_vmx)

#### Hypervisor Code Components

1. `src/platform/x86_64/vmx`: Kernel support for VMX.
2. `src/components/lib/vmrt`: Library for VM manipulation.
3. `src/components/implementation/simple_vmm`: Simple hypervisor implementation.

#### Prepare Guest Image

1. Compile the guest Linux kernel (`vmlinux`) and place it in:

   ```
   src/components/implementation/simple_vmm/vmm/guest/vmlinux.img
   ```

#### Build the Composite

1. Initialize and build:

   ```bash
   ./cos init x86_64
   ./cos build
   ./cos compose composition_scripts/simple_vmm.toml vm
   ```

   > The guest bootloader is here: `src/components/implementation/simple_vmm/vmm/guest/guest_realmode.S`. It will then be compiled to this binary file: `src/components/implementation/simple_vmm/vmm/guest/guest.img`

2. Run the system:

   ```bash
   ./cos run vm
   ```

---

## Debugging

### Debugging Guest Linux in Composite

1. Use `printk` to output messages from the kernel:

   ```c
   printk(KERN_INFO "%s at %u in (%s)\n", __FILE__, __LINE__, __func__);
   pr_info("%s at %u in (%s)\n", __FILE__, __LINE__, __func__);
   ```

2. Trigger a VM exit with `vmcall`:

   ```c
   asm volatile("vmcall");
   ```

### Debugging in Qemu

1. Use Linux kernel print functions for outputs.
2. Utilize GDB with Qemu for deeper debugging.

---

This revised guide organizes instructions for clarity and simplicity, ensuring a smooth development experience.


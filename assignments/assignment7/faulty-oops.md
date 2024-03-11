
# Kernel Oops Analysis
This document provides an analysis of the page fault (kernel oops) occurance created in our Linux QEMU instance.

## Fault Replication
The setup is as follows:assignment 
- The git repo `https://github.com/cu-ecen-aeld/assignment-5-rojasx` holds the buildroot setup
    - This repo references `https://github.com/cu-ecen-aeld/assignment-7-rojasx` which contains the ldd package
    - This repo also references `https://github.com/cu-ecen-aeld/assignments-3-and-later-rojasx` which contains the socket script.
- In the assignment-5 repo, the following command is called to build the QEMU machine: `./build.sh`
- Next, `./runqemu.sh` is called to open the QEMU instance.
- Finally, a simple command that writes to /dev/faulty is called, creating the page fault: `"hello_world" > /dev/faulty`

## Investigation
When writing to `/dev/faulty`, the following error is seen in the terminal: 
```
Unable to handle kernel NULL pointer dereference at virtual address 0000000000000000
Mem abort info:
  ESR = 0x96000045
  EC = 0x25: DABT (current EL), IL = 32 bits
  SET = 0, FnV = 0
  EA = 0, S1PTW = 0
  FSC = 0x05: level 1 translation fault
Data abort info:
  ISV = 0, ISS = 0x00000045
  CM = 0, WnR = 1
user pgtable: 4k pages, 39-bit VAs, pgdp=00000000420aa000
[0000000000000000] pgd=0000000000000000, p4d=0000000000000000, pud=0000000000000000
Internal error: Oops: 96000045 [#1] SMP
Modules linked in: hello(O) faulty(O) scull(O)
CPU: 0 PID: 159 Comm: sh Tainted: G           O      5.15.18 #1
Hardware name: linux,dummy-virt (DT)
pstate: 80000005 (Nzcv daif -PAN -UAO -TCO -DIT -SSBS BTYPE=--)
pc : faulty_write+0x14/0x20 [faulty]
lr : vfs_write+0xa8/0x2b0
sp : ffffffc008d23d80
x29: ffffffc008d23d80 x28: ffffff80020d2640 x27: 0000000000000000
x26: 0000000000000000 x25: 0000000000000000 x24: 0000000000000000
x23: 0000000040001000 x22: 000000000000000c x21: 000000558e0b2a70
x20: 000000558e0b2a70 x19: ffffff8002053100 x18: 0000000000000000
x17: 0000000000000000 x16: 0000000000000000 x15: 0000000000000000
x14: 0000000000000000 x13: 0000000000000000 x12: 0000000000000000
x11: 0000000000000000 x10: 0000000000000000 x9 : 0000000000000000
x8 : 0000000000000000 x7 : 0000000000000000 x6 : 0000000000000000
x5 : 0000000000000001 x4 : ffffffc0006f7000 x3 : ffffffc008d23df0
x2 : 000000000000000c x1 : 0000000000000000 x0 : 0000000000000000
Call trace:
 faulty_write+0x14/0x20 [faulty]
 ksys_write+0x68/0x100
 __arm64_sys_write+0x20/0x30
 invoke_syscall+0x54/0x130
 el0_svc_common.constprop.0+0x44/0xf0
 do_el0_svc+0x40/0xa0
 el0_svc+0x20/0x60
 el0t_64_sync_handler+0xe8/0xf0
 el0t_64_sync+0x1a0/0x1a4
Code: d2800001 d2800000 d503233f d50323bf (b900003f) 
---[ end trace 7c9354e0667ecf49 ]---
```

Thankfully, this output clearly states the root cause of the issue: `Unable to handle kernel NULL pointer dereference at virtual address 0000000000000000` i.e. an attemp at a NULL pointer dereference was made.

## Fault Origin
The fault occurs at `pc : faulty_write+0x14/0x20 [faulty]`, which originates from one of the scripts that we added from our LDD package inclusion from the `base_external` director. In `base_external/package/ldd/ldd.mk`, we specify the inclusion of misc-modules from our assignment-7 repo. One of these modules is called faulty.c, which includes the function `faulty_write` where an integer pointer is set to 0 and dereferenced:
```
ssize_t faulty_write (struct file *filp, const char __user *buf, size_t count,
		loff_t *pos)
{
	/* make a simple fault by dereferencing a NULL pointer */
	*(int *)0 = 0;
	return 0;
}
```

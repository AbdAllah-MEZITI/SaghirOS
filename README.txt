SaghirOS is a simple x86 OS.
it's based on the "SimpleOS" tutorial. http://sos.enix.org/fr

The os is "multiboot2" complient,
it's simulated under qemu using GRUB2 to boot.

Ubuntu :14.04.4 LTS
QEMU emulator version 2.0.0 (Debian 2.0.0+dfsg-2ubuntu1.22)
GRUB 2.02~beta2-9ubuntu1.7


BUILD
-----
to build:
make

to simulate under qemu:
make runiso

clean the project:
make clean


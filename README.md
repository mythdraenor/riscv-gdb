riscv-gdb
=========

RISC-V GDB port

Build Instructions
------------------
$ mkdir build

$ cd build

$ ../configure --program-prefix=riscv- --enable-riscv --enable-targets=riscv-unknown-elf

$ make

$ sudo make install


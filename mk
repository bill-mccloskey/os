#!/usr/bin/env python

import os
import sys

kernel_files = [
    'address_space.cc',
    'assertions.cc',
    'elf.cc',
    'framebuffer.cc',
    'frame_allocator.cc',
    'interrupt_handlers.s',
    'interrupts.cc',
    'io.cc',
    'kmain.cc',
    'loader.s',
    'loader64.s',
    'multiboot_header.s',
    'multiboot.cc',
    'page_tables.cc',
    'protection.cc',
    'serial.cc',
    'builtins/string.cc',
    'syscall.s',
    'thread.cc',
]

kernel_cflags = (
    '-nostdlib -nostdinc -fno-builtin -fno-stack-protector -mno-red-zone -ffreestanding -mcmodel=large ' +
    '-mno-mmx -mno-sse -mno-sse2 -nostartfiles -nodefaultlibs -fno-rtti ' +
    '-fomit-frame-pointer -fno-exceptions -fno-asynchronous-unwind-tables -fno-unwind-tables ' +
    '-Wall -Wextra -Werror -Wno-unused-parameter -isystem /usr/lib/gcc/x86_64-linux-gnu/5/include ' +
    '-isystem src/builtins'
)

user_cflags = kernel_cflags

kernel_asflags = '-f elf64'

kernel_ldflags = '-n -T link.ld'

def run(cmd, d):
    d = d.copy()
    d.update(globals())
    cmd = cmd.format(**d)
    print cmd
    code = os.system(cmd)
    if code:
        sys.exit(code)

def build_kernel_cc(inp, out):
    run('g++ {kernel_cflags} -c {inp} -o {out}', locals())

def build_kernel_asm(inp, out):
    run('nasm {kernel_asflags} {inp} -o {out}', locals())

def build_kernel():
    obj_files = []
    for f in kernel_files:
        if f.endswith('.cc'):
            objfile = 'obj/' + f.replace('.cc', '.o')
            build_kernel_cc('src/' + f, objfile)
        elif f.endswith('.s'):
            objfile = 'obj/' + f.replace('.s', '.o')
            build_kernel_asm('src/' + f, objfile)
        obj_files.append(objfile)

    obj_files = ' '.join(obj_files)
    run('ld {kernel_ldflags} {obj_files} -o obj/kernel.elf', locals())

def build_program():
    run('g++ {user_cflags} src/program/program.cc -o obj/program.elf', locals())

def build_iso():
    run('mkdir -p obj/iso/boot/grub', {})
    run('mkdir -p obj/iso/modules', {})
    run('cp obj/kernel.elf obj/iso/boot', {})
    run('cp obj/program.elf obj/iso/modules', {})
    run('cp grub.cfg obj/iso/boot/grub', {})
    run('grub-mkrescue /usr/lib/grub/i386-pc -o obj/os.iso obj/iso', {})
    run('rm -r obj/iso', {})

def build():
    os.system('mkdir -p obj')
    os.system('mkdir -p obj/builtins')
    os.system('mkdir -p obj/tests')

    build_kernel()
    build_program()
    build_iso()

def clean():
    os.system('rm -rf obj')

def run_bochs():
    os.system('bochs -f bochsrc.txt -q')

def run_qemu():
    os.system('qemu-system-x86_64 -cdrom build/os.iso -serial mon:stdio ' +
	      '-device isa-debug-exit,iobase=0xf4,iosize=0x01')

if sys.argv[1] == 'clean':
    clean()
elif sys.argv[1] == 'build':
    build()
elif sys.argv[1] == 'bochs':
    build()
    run_bochs()
elif sys.argv[1] == 'qemu':
    build()
    run_qemu()

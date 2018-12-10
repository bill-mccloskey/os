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
    'loader.cc',
    'loader32.s',
    'loader64.s',
    'multiboot_header.s',
    'multiboot.cc',
    'output_stream.cc',
    'page_tables.cc',
    'protection.cc',
    'serial.cc',
    'builtins/string.cc',
    'system_calls.cc',
    'thread.cc',
]

test_files = [
    'allocator_test.cc',
    'page_tables_test.cc',
    'linked_list_test.cc',
]

extra_test_files = {
    'allocator_test.cc': 'src/frame_allocator.cc',
    'page_tables_test.cc': 'src/frame_allocator.cc src/page_tables.cc'
}

kernel_compiler = 'x86_64-elf-g++'
#kernel_compiler = 'clang++ --target=x86_64-pc-none-elf'

kernel_cflags = (
    '-std=c++14 -march=x86-64 ' +
    '-ffreestanding  -fno-builtin -nostdlib -nostdinc++ ' +
    '-fno-stack-protector -mno-red-zone -mcmodel=large ' +
    '-mno-mmx -mno-sse -mno-sse2 -fno-rtti ' +
    '-fomit-frame-pointer -fno-exceptions -fno-asynchronous-unwind-tables -fno-unwind-tables ' +
    '-Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-const-variable ' +
    '-isystem src/builtins'
)

user_compiler = 'x86_64-elf-g++'
#user_compiler = 'clang++ --target=x86_64-pc-none-elf'
user_cflags = kernel_cflags

test_compiler = 'g++'
#test_compiler = 'clang++ --target=x86_64-pc-none-elf'
test_cflags = kernel_cflags

asflags = '-f elf64'

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
    run('{kernel_compiler} {kernel_cflags} -c {inp} -o {out}', locals())

def build_kernel_asm(inp, out):
    run('nasm {asflags} {inp} -o {out}', locals())

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

def build_terminal_driver():
    run('nasm {asflags} src/syscall.s -o obj/syscall.o', locals())
    run('{user_compiler} {user_cflags} obj/syscall.o -Isrc src/framebuffer.cc src/io.cc src/output_stream.cc src/drivers/terminal/terminal.cc -o obj/terminal.elf',
        locals())

def build_test_program():
    run('nasm {asflags} src/syscall.s -o obj/syscall.o', locals())
    run('{user_compiler} {user_cflags} obj/syscall.o -Isrc src/test_program/test_program.cc -o obj/test_program.elf',
        locals())

def build_iso():
    run('mkdir -p obj/iso/boot/grub', {})
    run('mkdir -p obj/iso/modules', {})
    run('cp obj/kernel.elf obj/iso/boot', {})
    run('cp obj/terminal.elf obj/iso/modules', {})
    run('cp obj/test_program.elf obj/iso/modules', {})
    run('cp grub.cfg obj/iso/boot/grub', {})
    run('grub-mkrescue /usr/lib/grub/i386-pc -o obj/os.iso obj/iso', {})
    run('rm -r obj/iso', {})

def build():
    os.system('mkdir -p obj')
    os.system('mkdir -p obj/builtins')

    build_kernel()
    build_test_program()
    build_terminal_driver()
    build_iso()

def build_tests():
    os.system('mkdir -p obj')
    os.system('mkdir -p obj/tests')

    gtest = 'googletest/googletest'
    run('{test_compiler} -std=c++14 -isystem {gtest}/include -I{gtest} -pthread -c {gtest}/src/gtest-all.cc -o obj/gtest-all.o', locals())
    run('ar -rv obj/libgtest.a obj/gtest-all.o', locals())

    for f in test_files:
        objfile = 'obj/tests/' + f.replace('.cc', '')
        extra = extra_test_files.get(f, '')
        run('{test_compiler} -g -DTESTING -std=c++14 -o {objfile} -Isrc -I{gtest}/include src/tests/{f} {extra} -pthread obj/libgtest.a',
            locals())

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
elif sys.argv[1] == 'tests':
    build_tests()
elif sys.argv[1] == 'bochs':
    build()
    run_bochs()
elif sys.argv[1] == 'qemu':
    build()
    run_qemu()

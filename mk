#!/usr/bin/env python

import os
import os.path
import sys

kernel_files = [
    'base/io.cc',
    'base/output_stream.cc',
    'builtins/string.cc',
    'kernel/address_space.cc',
    'kernel/assertions.cc',
    'kernel/elf.cc',
    'kernel/frame_allocator.cc',
    'kernel/interrupt_handlers.s',
    'kernel/interrupts.cc',
    'kernel/kmain.cc',
    'kernel/loader.cc',
    'kernel/loader32.s',
    'kernel/loader64.s',
    'kernel/multiboot_header.s',
    'kernel/multiboot.cc',
    'kernel/page_tables.cc',
    'kernel/protection.cc',
    'kernel/serial.cc',
    'kernel/system_calls.cc',
    'kernel/thread.cc',
]

console_files = [
    'base/io.cc',
    'base/output_stream.cc',
    'base/driver_assertions.cc',
    'usr/syscall.s',
    'drivers/console/framebuffer.cc',
    'drivers/console/console.cc',
    'drivers/console/vtparse.c',
    'drivers/console/vtparse_table.c',
    'drivers/console/emulator.cc',
]

keyboard_files = [
    'base/io.cc',
    'base/output_stream.cc',
    'usr/syscall.s',
    'drivers/keyboard/keyboard.cc',
]

test_files = [
    'kernel/allocator_test.cc',
    'kernel/page_tables_test.cc',
    'base/linked_list_test.cc',
    'drivers/console/emulator_test.cc',
]

test_data = {
    'drivers/console/emulator_test.cc': 'drivers/console/emulator_test_data',
}

extra_test_files = {
    'kernel/allocator_test.cc': ['kernel/frame_allocator.cc'],
    'kernel/page_tables_test.cc': ['kernel/frame_allocator.cc', 'kernel/page_tables.cc'],
}

extra_test_obj_files = {
    'drivers/console/emulator_test.cc': [
        'drivers/console/vtparse.o',
        'drivers/console/vtparse_table.o',
        'drivers/console/emulator.o',
    ],
}

#compiler = 'x86_64-elf-g++'
#c_compiler = 'x86_64-elf-gcc'

compiler = 'clang++ --target=x86_64-pc-none-elf'
c_compiler = 'clang --target=x86_64-pc-none-elf'

cflags = (
    '-march=x86-64 -g ' +
    '-ffreestanding  -fno-builtin -nostdlib -nostdinc++ ' +
    '-fno-stack-protector -mno-red-zone -mcmodel=large ' +
    '-mno-mmx -mno-sse -mno-sse2 -fno-rtti ' +
    '-fomit-frame-pointer -fno-exceptions -fno-asynchronous-unwind-tables -fno-unwind-tables ' +
    '-Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-const-variable -Wno-missing-field-initializers ' +
    '-isystem src/builtins ' +
    '-Isrc/base '
)

ccflags = (
    '-std=c++14 ' + cflags
)

#test_compiler = 'g++'
#test_c_compiler = 'gcc'

test_compiler = 'clang++'
test_c_compiler = 'clang'

asflags = '-f elf64'
ldflags = '-n -T link.ld'

def run(cmd, d):
    d = d.copy()
    d.update(globals())
    cmd = cmd.format(**d)
    print cmd
    code = os.system(cmd)
    if code:
        sys.exit(code)

def build_cc(inp, out):
    run('{compiler} {ccflags} -c {inp} -o {out}', locals())

def build_c(inp, out):
    run('{c_compiler} {cflags} -c {inp} -o {out}', locals())

def build_asm(inp, out):
    run('nasm {asflags} {inp} -o {out}', locals())

def build_files(files):
    obj_files = []
    for f in files:
        if f.endswith('.cc'):
            objfile = 'obj/' + f.replace('.cc', '.o')
            build_cc('src/' + f, objfile)
        elif f.endswith('.c'):
            objfile = 'obj/' + f.replace('.c', '.o')
            build_c('src/' + f, objfile)
        elif f.endswith('.s'):
            objfile = 'obj/' + f.replace('.s', '.o')
            build_asm('src/' + f, objfile)
        obj_files.append(objfile)

    return obj_files

def build_kernel():
    obj_files = build_files(kernel_files)
    obj_files = ' '.join(obj_files)
    run('ld {ldflags} {obj_files} -o obj/kernel.elf', locals())

def build_console_driver():
    obj_files = build_files(console_files)
    obj_files = ' '.join(obj_files)
    run('{compiler} {ccflags} {obj_files} -o obj/console.elf', locals())

def build_keyboard_driver():
    obj_files = build_files(keyboard_files)
    obj_files = ' '.join(obj_files)
    run('{compiler} {ccflags} {obj_files} -o obj/keyboard.elf', locals())

def build_iso():
    run('mkdir -p obj/iso/boot/grub', {})
    run('mkdir -p obj/iso/modules', {})
    run('cp obj/kernel.elf obj/iso/boot', {})
    run('cp obj/console.elf obj/iso/modules', {})
    run('cp obj/keyboard.elf obj/iso/modules', {})
    #run('cp obj/test_program.elf obj/iso/modules', {})
    run('cp grub.cfg obj/iso/boot/grub', {})
    run('grub-mkrescue /usr/lib/grub/i386-pc -o obj/os.iso obj/iso', {})
    run('rm -r obj/iso', {})

def build():
    os.system('mkdir -p obj')
    os.system('mkdir -p obj/kernel')
    os.system('mkdir -p obj/base')
    os.system('mkdir -p obj/usr')
    os.system('mkdir -p obj/drivers/console')
    os.system('mkdir -p obj/drivers/keyboard')
    os.system('mkdir -p obj/builtins')

    build_kernel()
    #build_test_program()
    build_console_driver()
    build_keyboard_driver()
    build_iso()

def build_tests():
    os.system('mkdir -p obj')
    os.system('mkdir -p obj/tests')

    gtest = 'googletest/googletest'
    run('{test_compiler} -std=c++14 -isystem {gtest}/include -I{gtest} -pthread -c {gtest}/src/gtest-all.cc -o obj/gtest-all.o', locals())
    run('ar -rv obj/libgtest.a obj/gtest-all.o', locals())

    for f in test_files:
        objfile = 'obj/' + f.replace('.cc', '')
        extra = extra_test_files.get(f, []) + ['base/gtest_assertions.cc']
        extra = ' '.join([ 'src/' + ex for ex in extra ])

        extra_obj = extra_test_obj_files.get(f, [])
        extra_obj = ' '.join([ 'obj/' + ex for ex in extra_obj ])

        include = '-Isrc -Isrc/base -I{gtest}/include '.format(**locals())
        run('{test_compiler} -g -DTESTING -std=c++14 -o {objfile} {include} src/{f} {extra} {extra_obj} -pthread obj/libgtest.a',
            locals())

        if f in test_data:
            os.system('mkdir -p obj/%s' % os.path.dirname(test_data[f]))
            os.system('ln -sf %s obj/%s' % (os.path.relpath('src/' + test_data[f], test_data[f]),
                                            os.path.dirname(test_data[f])))

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

#!/usr/bin/env python3

# TODO:
# - Emulator test data

from build.rules import kernel, lib, test_lib, test, task, commands
from build.commands import go, add_target_command

test_lib(
    target='gtest.lib',
    src_path='googletest/googletest',
    hdr_path='googletest/googletest',
    public_hdr_path='googletest/googletest/include',
    cmds={
        'src/gtest-all.cc':
        ('{cc_compiler} {cc_flags} -isystem include -I. -pthread ' +
         '-c src/gtest-all.cc -o {outfile}')
    },
    hdrs=[
        'src/gtest.cc',
        'src/gtest-death-test.cc',
        'src/gtest-filepath.cc',
        'src/gtest-port.cc',
        'src/gtest-printers.cc',
        'src/gtest-test-part.cc',
        'src/gtest-typed-test.cc',
        'src/gtest-internal-inl.h',
    ],
    public_hdrs=[
        'gtest/gtest-death-test.h',
        'gtest/gtest.h',
        'gtest/gtest-message.h',
        'gtest/gtest-param-test.h',
        'gtest/gtest_pred_impl.h',
        'gtest/gtest-printers.h',
        'gtest/gtest_prod.h',
        'gtest/gtest-spi.h',
        'gtest/gtest-test-part.h',
        'gtest/gtest-typed-test.h',

        'gtest/internal/gtest-death-test-internal.h',
        'gtest/internal/gtest-filepath.h',
        'gtest/internal/gtest-internal.h',
        'gtest/internal/gtest-param-util-generated.h',
        'gtest/internal/gtest-param-util.h',
        'gtest/internal/gtest-port-arch.h',
        'gtest/internal/gtest-port.h',
        'gtest/internal/gtest-string.h',
        'gtest/internal/gtest-type-util.h',

        'gtest/internal/custom/gtest.h',
        'gtest/internal/custom/gtest-port.h',
        'gtest/internal/custom/gtest-printers.h',
    ],
)

lib(
    target='console_fonts.lib',
    public_hdrs=[
        'drivers/console/console_fonts.h',
    ],
    srcs=[
        'drivers/console/console_fonts.cc',
    ],
    hdrs=[
        'drivers/console/Lat15-Fixed16.psf',
        'drivers/console/Lat15-VGA16.psf',
    ],
    file_transform={
        '.psf': 'xxd -i {infile} > {outfile}',
    }
)

lib(
    target='base.lib',
    srcs=[
        'base/assertions.cc',
        'base/io.cc',
        'base/output_stream.cc',
        'base/placement_new.cc',
        'base/string.cc',
    ],
    public_hdrs=[
        'base/assertions.h',
        'base/io.h',
        'base/kernel_module.h',
        'base/lazy_global.h',
        'base/linked_list.h',
        'base/output_stream.h',
        'base/placement_new.h',
        'base/refcount.h',
        'base/types.h',
    ],
    hdrs=[
        'kernel/serial.h',
        'usr/system.h',
    ],
    mode_flags={
        'kernel': ['-DKERNEL_BUILD'],
        'task': ['-DTASK_BUILD'],
        'test': ['-DTEST_BUILD'],
    },
)

lib(
    target='task.lib',
    srcs=[
        'usr/syscall.s',
    ],
    public_hdrs=[
        'usr/system.h',
    ],
    deps=[
        'base.lib',
    ],
)

lib(
    target='keyboard.lib',
    srcs=[],
    public_hdrs=[
        'usr/keyboard.h',
    ],
)

lib(
    target='emulator.lib',
    srcs=[
        'drivers/console/vtparse.c',
        'drivers/console/vtparse_table.c',
        'drivers/console/emulator.cc',
    ],
    public_hdrs=[
        'drivers/console/emulator.h',
        'drivers/console/vtparse.h',
        'drivers/console/vtparse_table.h',
    ],
    deps=[
        'base.lib',
    ],
)

test(
    target='emulator_test',
    srcs=['drivers/console/emulator_test.cc'],
    deps=[
        'emulator.lib',
        'gtest.lib',
    ],
)

task(
    target='console.elf',
    srcs=[
        'drivers/console/abstract_frame_buffer.cc',
        'drivers/console/raster_frame_buffer.cc',
        'drivers/console/text_frame_buffer.cc',
        'drivers/console/console.cc',
    ],
    hdrs=[
        'drivers/console/abstract_frame_buffer.h',
        'drivers/console/raster_frame_buffer.h',
        'drivers/console/text_frame_buffer.h',
    ],
    deps=[
        'base.lib',
        'task.lib',
        'console_fonts.lib',
        'emulator.lib',
    ],
)

task(
    target='keyboard.elf',
    srcs=[
        'drivers/keyboard/keyboard.cc',
    ],
    deps=[
        'base.lib',
        'task.lib',
        'keyboard.lib',
    ],
)

task(
    target='test_program.elf',
    srcs=[
        'test_program/test_program.cc',
    ],
    deps=[
        'base.lib',
        'task.lib',
        'keyboard.lib',
    ],
)

lib(
    target='inode.lib',
    srcs=[
        'fs/inode.cc',
    ],
    public_hdrs=[
        'fs/inode.h',
    ],
    deps=[
        'base.lib',
    ],
)

test(
    target='inode_test',
    srcs=['fs/inode_test.cc'],
    deps=[
        'inode.lib',
        'gtest.lib',
    ],
)

lib(
    target='kmem.lib',
    srcs=[
        'kernel/frame_allocator.cc',
        'kernel/page_tables.cc',
    ],
    public_hdrs=[
        'kernel/allocator.h',
        'kernel/frame_allocator.h',
        'kernel/page_tables.h',
        'kernel/page_translation.h',
    ],
    deps=[
        'base.lib',
    ],
)

test(
    target='allocator_test',
    srcs=['kernel/allocator_test.cc'],
    deps=[
        'kmem.lib',
        'gtest.lib',
    ],
)

test(
    target='page_tables_test',
    srcs=['kernel/page_tables_test.cc'],
    deps=[
        'kmem.lib',
        'gtest.lib',
    ],
)

test(
    target='linked_list_test',
    srcs=['base/linked_list_test.cc'],
    deps=[
        'base.lib',
        'gtest.lib',
    ],
)

kernel(
    target='kernel.elf',
    srcs=[
        'kernel/address_space.cc',
        'kernel/elf.cc',
        'kernel/interrupt_handlers.s',
        'kernel/interrupts.cc',
        'kernel/kmain.cc',
        'kernel/loader.cc',
        'kernel/loader32.s',
        'kernel/loader64.s',
        'kernel/multiboot_header.s',
        'kernel/multiboot.cc',
        'kernel/protection.cc',
        'kernel/serial.cc',
        'kernel/system_calls.cc',
        'kernel/thread.cc',
    ], hdrs=[
        'kernel/address_space.h',
        'kernel/elf.h',
        'kernel/interrupts.h',
        'kernel/loader.h',
        'kernel/multiboot.h',
        'kernel/protection.h',
        'kernel/serial.h',
        'kernel/thread.h',
    ], deps=[
        'base.lib',
        'kmem.lib',
    ]
)

commands(
    target='os.iso',
    cmds=[
        'mkdir -p iso/boot/grub',
        'mkdir -p iso/modules',
        'cp kernel.elf iso/boot',
        'cp console.elf iso/modules',
        'cp keyboard.elf iso/modules',
        'cp test_program.elf iso/modules',
        'cp grub.cfg iso/boot/grub',
        'grub-mkrescue /usr/lib/grub/i386-pc -o os.iso iso',
    ],
    data=[
        'grub.cfg',
    ],
    deps=[
        'kernel.elf',
        'console.elf',
        'keyboard.elf',
        'test_program.elf',
    ],
)

add_target_command(
    name='bochs',
    target='os.iso',
    command='bochs -f bochsrc.txt -q',
)

add_target_command(
    name='qemu',
    target='os.iso',
    command=
    'qemu-system-x86_64 -cdrom obj/os.iso -serial mon:stdio '
    '-device isa-debug-exit,iobase=0xf4,iosize=0x01',
)

if __name__ == '__main__':
    go()

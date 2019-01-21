# FIXME: Make this a parameter
debug_option = ['-g', '-O0']

def asm_compiler():
    return 'nasm'

def asm_flags():
    return ['-f', 'elf64']

def cc_compiler():
    return 'clang++'

def cc_flags():
    return [
        '-std=c++14',
        '-Wall',
        '-Wextra',
        '-Werror',
        '-Wno-unused-parameter',
        '-Wno-unused-const-variable',
        '-Wno-missing-field-initializers',
        '-Wno-unused-function',
    ] + debug_option

def c_compiler():
    return 'clang'

def c_flags():
    return debug_option

def ar():
    return 'ar'

def ar_flags():
    return ['-rv']

def linker():
    return 'ld'

def ld_flags():
    return []

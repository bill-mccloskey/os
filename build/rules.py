import time
import os.path
import inspect
import subprocess

from . import graph
from . import hash
from . import options
from . import sandbox

class UnexpectedSuffixError(Exception):
    def __init__(self, src, location):
        self.src = src
        self.location = location

    def __str__(self):
        return '{}: Unexpected source suffix {}'.format(self.location, self.src)

class MissingRequiredAttribute(Exception):
    def __init__(self, attr, location):
        self.attr = attr
        self.location = location

    def __str__(self):
        return '{}: Missing required attribute {}'.format(self.location, self.attr)

def file_date(filename):
    if not os.path.exists(filename):
        return 0
    return os.path.getmtime(filename)

ANY_MODE = {'default': []}

class RuleArgs(object):
    def __init__(self, dictionary, location):
        self.dictionary = dictionary
        self.location = location

    def copy_to(self, obj, required, defaults):
        for req in required:
            if req not in self.dictionary:
                raise MissingRequiredAttribute(req, self.location)

        for (k, v) in self.dictionary.items():
            setattr(obj, k, v)

        for (k, v) in defaults.items():
            if k not in self.dictionary:
                setattr(obj, k, v)

class CommandsTarget(graph.Target):
    def __init__(self, name, location, args):
        args.copy_to(self, [], {
            'cmds': [],
            'data': [],
            'deps': [],
        })
        super().__init__(name, self.deps, location)

    def target_filename(self):
        return 'obj/{}'.format(self.name)

    def copy_interfaces_to_sandbox(self, sb):
        pass

    def copy_targets_to_sandbox(self, mode, sb):
        sb.copy_in(self.target_filename(), self.name)
        return [self.name]

    def build_self(self):
        sb = sandbox.Sandbox()

        for dep in self.deps:
            dep.copy_targets_to_sandbox('default', sb)

        for data in self.data:
            sb.copy_in(data, data)

        for cmd in self.cmds:
            sb.run_shell(cmd)

        sb.copy_out(self.name, self.target_filename())
        sb.destroy()

    def target_timestamp(self):
        return file_date(self.target_filename())

    def interface_timestamp(self):
        return 0

    def is_up_to_date(self):
        target_ts = self.target_timestamp()
        for dep in self.deps:
            if dep.target_timestamp() > target_ts:
                return False

        return True

class CppTarget(graph.Target):
    def __init__(self, name, location, args):
        args.copy_to(self, [], {
            'srcs': [],
            'hdrs': [],
            'public_hdrs': [],
            'deps': [],
            'mode_flags': None,
            'file_transform': {},
            'src_path': 'src',
            'hdr_path': 'src',
            'public_hdr_path': 'src',
            'cmds': {},
        })
        super().__init__(name, self.deps, location)

        if not self.mode_flags:
            self.mode_flags = ANY_MODE

    def target_filename(self, mode):
        if self.mode_flags == ANY_MODE:
            return 'obj/{}'.format(self.name)
        else:
            return 'obj/{}-{}'.format(mode, self.name)

    def source_filename(self, src):
        return '{}/{}'.format(self.src_path, src)

    def header_filename(self, hdr):
        return '{}/{}'.format(self.hdr_path, hdr)

    def public_header_filename(self, hdr):
        return '{}/{}'.format(self.public_hdr_path, hdr)

    def interface_timestamp(self):
        if self.public_hdrs:
            t = min([ file_date(self.public_header_filename(hdr)) for hdr in self.public_hdrs ])
        else:
            t = time.clock()

        if self.deps:
            return min(t, min([ dep.interface_timestamp() for dep in self.deps ]))
        else:
            return t

    def target_timestamp_in_mode(self, mode):
        return file_date(self.target_filename(mode))

    def target_timestamp(self):
        ts = 2**64
        for mode in self.mode_flags:
            ts = min(ts, self.target_timestamp_in_mode(mode))
        return ts

    def is_up_to_date(self):
        target_ts = self.target_timestamp()

        files = ([ self.source_filename(f) for f in self.srcs ] +
                 [ self.header_filename(f) for f in self.hdrs ] +
                 [ self.public_header_filename(f) for f in self.public_hdrs])

        for filename in files:
            if file_date(filename) > target_ts:
                return False

        print('interface ts =', self.interface_timestamp())
        print('target ts =', target_ts)
        return target_ts >= self.interface_timestamp()

    def asm_flags(self):
        return options.asm_flags()

    def cc_flags(self, mode):
        return ['-I.'] + options.cc_flags() + self.mode_flags.get(mode, [])

    def c_flags(self):
        return ['-I.'] + options.c_flags()

    def build_self(self):
        for mode in self.mode_flags:
            self.build_self_in_mode(mode)

    def maybe_transform(self, sb, filename, new_suffix):
        (base, suffix) = os.path.splitext(filename)
        if suffix in self.file_transform:
            sb.run_shell(self.file_transform[suffix].format(infile=filename, outfile=(base + new_suffix)))

    def build_self_in_mode(self, mode):
        sb = sandbox.Sandbox()

        for dep in self.deps:
            dep.copy_interfaces_to_sandbox(sb)

        for hdr in self.public_hdrs:
            sb.copy_in(self.public_header_filename(hdr), hdr)

        for hdr in self.hdrs:
            sb.copy_in(self.header_filename(hdr), hdr)
            self.maybe_transform(sb, hdr, '.h')

        outfiles = []
        for src in self.srcs:
            sb.copy_in(self.source_filename(src), src)
            self.maybe_transform(sb, src, '.cc')
            outfile = hash.hash_strings(self.hashcode() + [src, mode])

            self.build_source(mode, sb, src, outfile)

            outfiles.append(outfile)
            sb.delete(src)

        for (src, cmd) in self.cmds.items():
            sb.copy_in(self.source_filename(src), src)
            self.maybe_transform(sb, src, '.cc')
            outfile = hash.hash_strings(self.hashcode() + [src, mode])

            sb.run_shell(cmd.format(
                cc_compiler=options.cc_compiler(),
                cc_flags=' '.join(self.cc_flags(mode)),
                outfile=outfile,
            ))

            outfiles.append(outfile)
            sb.delete(src)

        self.build_final(sb, outfiles, self.name)
        sb.copy_out(self.name, self.target_filename(mode))

        sb.destroy()

    def copy_targets_to_sandbox(self, mode, sb):
        sb.copy_in(self.target_filename(mode), self.name)
        return [self.name]

    def copy_interfaces_to_sandbox(self, sb):
        for dep in self.deps:
            dep.copy_interfaces_to_sandbox(sb)

        for hdr in self.public_hdrs:
            sb.copy_in(self.public_header_filename(hdr), hdr)

    def build_source(self, mode, sb, src, outfile):
        (src_base, suffix) = os.path.splitext(src)
        if suffix == '.s':
            sb.run(options.asm_compiler(), self.asm_flags() + [src, '-o', outfile])
        elif suffix == '.cc':
            sb.run(options.cc_compiler(), self.cc_flags(mode) + ['-c', src, '-o', outfile])
        elif suffix == '.c':
            sb.run(options.c_compiler(), self.c_flags() + ['-c', src, '-o', outfile])
        else:
            raise UnexpectedSuffixError(src, self.location)

class FreestandingCppTarget(CppTarget):
    def __init__(self, name, location, args):
        super().__init__(name, location, args)

    def kernel_c_flags(self):
        extra = []
        if 'clang' in options.c_compiler():
            extra += ['--target=x86_64-pc-none-elf']

        return extra + [
            '-march=x86-64',
            '-ffreestanding',
            '-fno-builtin',
            '-fno-stack-protector',
            '-mno-red-zone',
            '-mcmodel=large',
            '-mno-mmx',
            '-mno-sse',
            '-mno-sse2',
            '-fomit-frame-pointer',
            '-fno-rtti',
            '-fno-exceptions',
            '-fno-asynchronous-unwind-tables',
            '-fno-unwind-tables',
        ]

    def kernel_ld_flags(self):
        return ['-nostdlib']

    def cc_flags(self, mode):
        return super().cc_flags(mode) + self.kernel_c_flags()

    def c_flags(self):
        return options.c_flags() + self.kernel_c_flags()

    def ld_flags(self):
        return options.ld_flags() + self.kernel_ld_flags()

class TaskTarget(FreestandingCppTarget):
    def __init__(self, name, location, args):
        super().__init__(name, location, args)

    def build_final(self, sb, objfiles, targetfile):
        dep_targets = []
        for dep in self.transitive_deps():
            dep_targets += dep.copy_targets_to_sandbox('task', sb)

        sb.run(options.cc_compiler(), self.ld_flags() + objfiles + dep_targets + ['-o', targetfile])

class KernelTarget(FreestandingCppTarget):
    def __init__(self, name, location, args):
        super().__init__(name, location, args)

    def build_final(self, sb, objfiles, targetfile):
        dep_targets = []
        for dep in self.transitive_deps():
            dep_targets += dep.copy_targets_to_sandbox('kernel', sb)

        # '-n' turns off alignment of sections. Why do I pass this?
        # FIXME: Make this depend on link.ld
        # Should also have a dependency on the build files themselves!
        sb.copy_in('link.ld', 'link.ld')
        sb.run(options.linker(), options.ld_flags() + ['-n', '-T', 'link.ld'] + objfiles + dep_targets + ['-o', targetfile])

class LibTarget(FreestandingCppTarget):
    def __init__(self, name, location, args):
        super().__init__(name, location, args)

    def build_final(self, sb, objfiles, targetfile):
        sb.run(options.ar(), options.ar_flags() + [targetfile] + objfiles)

class TestLibTarget(CppTarget):
    def __init__(self, name, location, args):
        super().__init__(name, location, args)

    def build_final(self, sb, objfiles, targetfile):
        sb.run(options.ar(), options.ar_flags() + [targetfile] + objfiles)

class TestTarget(CppTarget):
    def __init__(self, name, location, args):
        super().__init__(name, location, args)

    def cc_flags(self, mode):
        return super().cc_flags(mode) + ['-pthread']

    def build_final(self, sb, objfiles, targetfile):
        dep_targets = []
        for dep in self.transitive_deps():
            dep_targets += dep.copy_targets_to_sandbox('test', sb)

        sb.run(options.cc_compiler(), self.cc_flags('test') + objfiles + dep_targets + ['-o', targetfile])

def kernel(target, **args):
    caller = inspect.getframeinfo(inspect.stack()[1][0])
    location = '{}:{}'.format(caller.filename, caller.lineno)
    KernelTarget(target, location, RuleArgs(args, location))

def task(target, **args):
    caller = inspect.getframeinfo(inspect.stack()[1][0])
    location = '{}:{}'.format(caller.filename, caller.lineno)
    TaskTarget(target, location, RuleArgs(args, location))

def lib(target, **args):
    caller = inspect.getframeinfo(inspect.stack()[1][0])
    location = '{}:{}'.format(caller.filename, caller.lineno)
    LibTarget(target, location, RuleArgs(args, location))

def test_lib(target, **args):
    caller = inspect.getframeinfo(inspect.stack()[1][0])
    location = '{}:{}'.format(caller.filename, caller.lineno)
    TestLibTarget(target, location, RuleArgs(args, location))

def test(target, **args):
    caller = inspect.getframeinfo(inspect.stack()[1][0])
    location = '{}:{}'.format(caller.filename, caller.lineno)
    TestTarget(target, location, RuleArgs(args, location))

def commands(target, **args):
    caller = inspect.getframeinfo(inspect.stack()[1][0])
    location = '{}:{}'.format(caller.filename, caller.lineno)
    CommandsTarget(target, location, RuleArgs(args, location))

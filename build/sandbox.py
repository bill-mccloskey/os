import tempfile
import os.path
import shutil
import subprocess

class CommandError(Exception):
    def __init__(self, code):
        self.code = code

    def __str__(self):
        return 'Command failed: {}'.format(self.code)


class Sandbox(object):
    def __init__(self):
        self.path = tempfile.mkdtemp(prefix='build_sandbox')

    def create_path(self, path):
        try:
            os.makedirs(os.path.join(self.path, path))
        except FileExistsError:
            pass

    def copy_in(self, srcfile, dstfile):
        print('copyin', srcfile, dstfile)
        (d, f) = os.path.split(dstfile)
        try:
            os.makedirs(os.path.join(self.path, d))
        except FileExistsError:
            pass
        shutil.copy(srcfile, os.path.join(self.path, dstfile))

    def copy_out(self, srcfile, dstfile):
        print('copyout', dstfile)
        (d, f) = os.path.split(dstfile)
        try:
            os.makedirs(d)
        except FileExistsError:
            pass
        shutil.copy(os.path.join(self.path, srcfile), dstfile)

    def delete(self, f):
        os.unlink(os.path.join(self.path, f))

    def run(self, cmd, flags):
        print('Running', [cmd] + flags)
        code = subprocess.call([cmd] + flags, cwd=self.path)
        if code:
            raise CommandError(code)

    def run_shell(self, cmd):
        print('Running "{}"'.format(cmd))
        code = subprocess.call(cmd, cwd=self.path, shell=1)
        if code:
            raise CommandError(code)

    def destroy(self):
        shutil.rmtree(self.path)

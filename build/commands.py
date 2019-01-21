import sys
import os

from . import graph

target_commands = {}

def add_target_command(name, target, command):
    target_commands[name] = (target, command)

def go():
    if sys.argv[1] == 'build':
        target = sys.argv[2]
        target = graph.registry.find(target)
        target.resolve()
        target.build()
    elif sys.argv[1] == 'test':
        target = sys.argv[2]
        target = graph.registry.find(target)
        target.resolve()
        target.build()
        os.system('./obj/{}'.format(sys.argv[2]))
    elif sys.argv[1] in target_commands:
        (target, command) = target_commands[sys.argv[1]]
        target = graph.registry.find(target)
        target.resolve()
        target.build()
        os.system(command)

import hashlib

class DependencyNotFoundError(Exception):
    def __init__(self, name, location):
        self.name = name
        self.location = location

    def __str__(self):
        return '{}: Dependency {} not found'.format(self.location, self.name)

class TargetRegistry(object):
    def __init__(self):
        self.targets = {}

    def add(self, target):
        self.targets[target.name] = target

    def find(self, name):
        return self.targets.get(name)

registry = TargetRegistry()

class Target(object):
    def __init__(self, name, deps, location):
        self.name = name
        self.deps = deps
        self.location = location
        self.resolved = False
        registry.add(self)

    def build_self(self):
        pass

    def copy_targets_to_sandbox(self, mode, sb):
        return []

    def copy_interfaces_to_sandbox(self, sb):
        pass

    def resolve(self):
        if self.resolved:
            return
        self.resolved = True

        new_deps = []
        for dep in self.deps:
            dep_target = registry.find(dep)
            if not dep_target:
                raise DependencyNotFoundError(dep, self.location)
            new_deps.append(dep_target)
            dep_target.resolve()

        self.deps = new_deps

    def interface_timestamp(self):
        return 0

    def target_timestamp(self):
        return 0

    def is_up_to_date(self):
        return False

    def build(self):
        for dep in self.deps:
            dep.build()

        if not self.is_up_to_date():
            self.build_self()

    def transitive_deps(self):
        r = []
        for dep in self.deps:
            r += [dep] + dep.transitive_deps()
        return r

    def self_hashcode(self):
        return [self.name, self.location]

    def hashcode(self):
        h = ['(']
        for dep in self.deps:
            h += dep.hashcode()
        h += [')']

        h += self.self_hashcode()
        return h

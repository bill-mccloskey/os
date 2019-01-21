import hashlib

def hash_strings(strings):
    m = hashlib.sha256()
    for s in strings:
        m.update(bytes(s, 'utf-8'))
    return m.hexdigest()

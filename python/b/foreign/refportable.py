'''
Portable (and slow) implementations of incref and decref.
'''
import collections

# TODO(strager): Locking.
_objects = collections.defaultdict(int)

def incref(object):
    _objects[object] += 1

def decref(object):
    old_ref_count = _objects[object]
    assert old_ref_count > 0
    ref_count = old_ref_count - 1
    _objects[object] = ref_count
    if ref_count == 0:
        del _objects[object]

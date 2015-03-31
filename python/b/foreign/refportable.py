'''
Portable (and slow) implementations of incref and decref.
'''
import collections
import gc
import os.path
import sys
import traceback

# Manually set this to True to record a traceback every
# incref and decref call.
track_tracebacks = False

# TODO(strager): Locking.
_objects = collections.defaultdict(int)
_tracebacks = collections.defaultdict(list)

def _capture_traceback():
    return traceback.extract_stack()

def incref(object):
    old_ref_count = _objects[object]
    ref_count = old_ref_count + 1
    _objects[object] = ref_count
    if track_tracebacks:
        _tracebacks[object].append(_capture_traceback())

def decref(object):
    old_ref_count = _objects[object]
    assert old_ref_count > 0
    ref_count = old_ref_count - 1
    _objects[object] = ref_count
    if ref_count == 0:
        del _tracebacks[object]
        del _objects[object]
    else:
        if track_tracebacks:
            _tracebacks[object].append(_capture_traceback())

def _last_index_of(pred, xs):
    index = None
    for (i, x) in enumerate(xs):
        if pred(x):
            index = i
    return index

def _print_traceback(tb, out):
    records = list(tb)
    # Skip the last two entires (_capture_traceback and
    # incref/decref).
    records.pop()
    records.pop()
    # Drop unittest framework stuff.
    def is_unittest_record(frame):
        (path, line, method, code) = frame
        return (
            '{_}unittest{_}'.format(_=os.path.sep) in path
            and method == 'run'
            and 'testMethod()' in code
        )
    unittest_index = _last_index_of(
        is_unittest_record,
        records,
    )
    if unittest_index is not None:
        records = records[unittest_index + 1:]
        out.write('  ...\n')
    for item in traceback.format_list(records):
        out.write(item)

def dump_leaked_objects(out=sys.stderr):
    print('gc.collect() = {}'.format(gc.collect()))
    print('gc.collect() = {}'.format(gc.collect()))
    print('gc.collect() = {}'.format(gc.collect()))
    out.write('{} objects leaked:\n'.format(len(_objects)))
    for (object, refcount) in _objects.items():
        out.write(' - {} (+{})\n'.format(
            repr(object),
            refcount,
        ))
        tracebacks = _tracebacks.get(object, None)
        if tracebacks is not None:
            for tb in tracebacks:
                _print_traceback(tb=tb, out=out)
                out.write('\n')
            out.write('\n')

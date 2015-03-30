import os

should_dump_leaked_objects \
    = bool(os.getenv('B_PYTHON_DUMP_LEAKED_OBJECTS', False))

try:
    if should_dump_leaked_objects:
        # refcpython does not support dump_leaked_objects.
        raise ImportError()
    import b.foreign.refcpython as ref
    from b.foreign.refcpython import incref
    from b.foreign.refcpython import decref
except ImportError:
    import b.foreign.refportable as ref

incref = ref.incref
decref = ref.decref

if should_dump_leaked_objects:
    import atexit
    atexit.register(ref.dump_leaked_objects)
    ref.track_tracebacks = True

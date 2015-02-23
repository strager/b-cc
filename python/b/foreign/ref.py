try:
    from b.foreign.refcpython import incref
    from b.foreign.refcpython import decref
except ImportError:
    from b.foreign.refportable import incref
    from b.foreign.refportable import decref

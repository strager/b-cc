import ctypes

def format_pointer(pointer):
    address = ctypes.cast(pointer, ctypes.c_void_p).value
    if address is None:
        return 'NULL'
    else:
        return '{:#018x}'.format(address)

def weak_py_object(object):
    return ctypes.cast(
        ctypes.c_void_p(id(object)),
        ctypes.py_object,
    )

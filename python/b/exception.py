import b.internal as b

import ctypes

class BException(Exception):
    def __init__(self, ex_object):
        super(BException, self).__init__(ex_object.message)
        self.ex_object = ex_object

domestic_exception_uuid = '0160B1BB-558E-4CCF-8BAD-B5B32B24D9D3'
domestic_exceptions = []
foreign_exceptions = []

def domestic_exception_deallocate_(ex):
    domestic_exceptions.remove(ex.contents.data)
    foreign_exceptions.remove(ex)
domestic_exception_deallocate = b.BDeallocate(domestic_exception_deallocate_)

def domestic_exception(ex):
    domestic_py_ptr = ctypes.pointer(ctypes.py_object(ex))
    domestic_ptr = ctypes.cast(domestic_py_ptr, ctypes.c_void_p)
    domestic_exceptions.append(domestic_ptr)

    foreign_ptr = b.BExceptionStructure(
        uuid=domestic_exception_uuid,
        message=str(ex),
        data=domestic_ptr,
        deallocate=domestic_exception_deallocate)
    foreign_exceptions.append(foreign_ptr)
    return foreign_ptr

def maybe_raise(ex_ptr):
    if ex_ptr:
        ex = BException(ex_ptr.contents)
        raise ex

def raise_foreign(function, *args):
    ex = ctypes.POINTER(b.BExceptionStructure)()
    new_args = list(args) + [ctypes.byref(ex)]
    result = function(*new_args)
    maybe_raise(ex)
    return result

def raise_domestic(function, *args):
    new_args = args[:-1]
    ex_ptr = args[-1]

    try:
        return function(*new_args)
    except Exception as ex:
        ex_ptr.contents.contents = domestic_exception(ex)
        return None

def wrap_domestic(function):
    return lambda *args: raise_domestic(function, *args)

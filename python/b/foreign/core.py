import ctypes
import ctypes.util
import os
import os.path
import types

def find_libb_path():
    env_var = 'B_DLL_PATH'
    path = os.getenv('B_DLL_PATH', None)
    if path is not None:
        if not os.path.exists(path):
            raise Exception(
                '{} ({}) does not point to a file'
                    .format(env_var, path),
            )
        return path
    path = ctypes.util.find_library('b')
    if path is not None:
        assert os.path.exists(path)
        return path
    raise Exception(
        'Could not find b library. Set {}.'.format(env_var),
    )

libb = ctypes.cdll.LoadLibrary(find_libb_path())

def b_export_func(name, args, restype=ctypes.c_bool):
    '''
    args is a heterogenous list of
    (('arg_name', arg_type) | arg_type).
    '''
    def is_tuple(value):
        return isinstance(value, tuple)
    def arg_name(arg):
        return arg[0] if is_tuple(arg) else None
    def arg_type(arg):
        return arg[1] if is_tuple(arg) else arg

    prototype = b_func(map(arg_type, args), restype=restype)
    function = prototype(
        (name, libb),
        tuple([(0, arg_name(arg)) for arg in args]),
    )
    # Set the func_name attribute for improved debugging.
    if not hasattr(function, 'func_name'):
        setattr(function, 'func_name', name)
    return function

def b_func(argtypes, restype=ctypes.c_bool):
    return ctypes.CFUNCTYPE(restype, *argtypes)

import b.foreign.all as foreign
import ctypes
import errno

def call_with_eh(func, *kargs, **kwargs):
    eh = None
    if 'eh' in kwargs:
        eh = kwargs['eh']
        del kwargs['eh']
    if eh is None:
        eh = ctypes.POINTER(foreign.ErrorHandler)()  # TODO

    if kwargs:
        kwargs['eh'] = eh
    else:
        kargs += (eh,)

    if not func(*kargs, **kwargs):
        raise Exception('Call to {} failed'.format(
            getattr(func, 'func_name', 'function')))

def wrap_eh(func, eh):
    try:
        func()
        return True
    except Exception as e:
        import traceback
        traceback.print_exc()  # TODO(strager): Remove.
        # TODO(strager): Use e.
        e = e
        # TODO(strager): Define raise_error.
        result = foreign.raise_error(eh, foreign.Error(
            errno_value=errno.EFAULT,
        ))
        # TODO(strager): Use result.
        result = result
        return False

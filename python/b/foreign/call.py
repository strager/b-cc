import b.foreign.all as foreign
import ctypes

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

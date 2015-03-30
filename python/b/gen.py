'''
By convention, a gen_ prefix to a function name means the
function is a generator.  To consume the value of a
generator, use yield:

    value = yield gen_myfunction(...)

To call the function without using its return value, use
yield:

    yield gen_myfunction(...)

If a function uses yield gen_, that function must be written
as a generator and must have a gen_ prefix:

    def gen_make_program():
        yield gen_compile_sources()
        yield gen_link_objects()
        yield gen_copy_resources()
'''

class _YieldReturnValue(object):
    def __init__(self, value):
        self.__value = value

    @property
    def value(self):
        return self.__value

    def __repr__(self):
        return '{}({})'.format(
            self.__class__.__name__, repr(self.__value),
        )

# TODO(strager): Make this thread-safe.
class Future(object):
    def __init__(self):
        self.__callbacks = []
        self.__callback_caller = None

    def add_callbacks(
            self,
            resolve_callback,
            cancel_callback,
            throw_callback,
    ):
        if resolve_callback is None \
                or cancel_callback is None \
                or throw_callback is None:
            raise ValueError('Callback cannot be None')
        callback_tuple = (
            resolve_callback,
            cancel_callback,
            throw_callback,
        )
        if self.__callback_caller is not None:
            self.__callback_caller(callback_tuple)
        else:
            self.__callbacks.append(callback_tuple)

    def resolve(self, value=None):
        if self.__callback_caller is not None:
            raise Exception('Already called')
        self.__callback_caller \
            = lambda callbacks: callbacks[0](value)
        self.__call_callbacks()

    def cancel(self):
        if self.__callback_caller is not None:
            raise Exception('Already called')
        self.__callback_caller \
            = lambda callbacks: callbacks[1]()
        self.__call_callbacks()

    def throw(self, error):
        if self.__callback_caller is not None:
            raise Exception('Already called')
        self.__callback_caller \
            = lambda callbacks: callbacks[2](error)
        self.__call_callbacks()

    def __call_callbacks(self):
        # Pull out the callbacks list in case we are called
        # recursively.
        callbacks = self.__callbacks
        self.__callbacks = None
        for callback_tuple in callbacks:
            self.__callback_caller(callback_tuple)

def gen_return(value=None):
    return _YieldReturnValue(value)

def gen_yield(generator):
    '''
    yield gen_yield(gen_foo()) ==
        yield gen_return(yield gen_foo())
    '''
    value = yield generator
    yield gen_return(value)

def gen_many(*kargs):
    values = []
    # TODO(strager): Parallelize.
    for gen_value in kargs:
        value = yield gen_value
        values.append(value)
    yield gen_return(tuple(values))

class _Looper(object):
    def __init__(self, generator, done_callback):
        self.__generator_stack = []
        self.__current_generator = generator
        self.__done_callback = done_callback

        self.__loop(value=None)

    def __loop(self, value):
        while True:
            try:
                result = self.__current_generator.send(
                    value,
                )
            except StopIteration:
                # Implicit gen_return(None) after every gen
                # function.
                result = _YieldReturnValue(None)
            if isinstance(result, _YieldReturnValue):
                value = result.value
                if self.__generator_stack:
                    self.__current_generator \
                        = self.__generator_stack.pop()
                    # Keep looping.
                else:
                    if self.__done_callback:
                        self.__done_callback(value)
                    return
            elif isinstance(result, Future):
                def resolve_callback(value):
                    self.__loop(value)
                def cancel_callback():
                    self.__current_generator.close()
                def throw_callback(error):
                    self.__current_generator.throw(error)
                result.add_callbacks(
                    resolve_callback,
                    cancel_callback,
                    throw_callback,
                )
                return
            else:
                self.__generator_stack.append(
                    self.__current_generator,
                )
                self.__current_generator = result
                value = None
                # Keep looping.

def loop(generator, done_callback=None):
    _Looper(generator, done_callback)

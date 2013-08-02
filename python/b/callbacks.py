class Callbacks(object):
    def __init__(self, ctor):
        self._list = []
        self._ctor = ctor

    def new(self, callback):
        c_callback = self._ctor(callback)
        self._list.append(c_callback)
        return c_callback

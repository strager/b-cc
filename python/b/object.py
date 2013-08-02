class Object(object):
    def __init__(self, ptr, vtable):
        self._ptr = ptr
        self._vtable = vtable

    def replicate(self):
        new_ptr = self._vtable.contents.replicate(self._ptr)
        return Object(ptr=new_ptr, vtable=self._vtable)

    def deallocate(self):
        self._vtable.contents.deallocate(self._ptr)

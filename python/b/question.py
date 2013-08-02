from b.object import Object
import b.internal as b

import ctypes

class Question(Object):
    def __init__(self, ptr, vtable):
        super(Question, self).__init__(
            ptr=ptr,
            vtable=ctypes.cast(vtable,
                ctypes.POINTER(b.BQuestionVTableStructure)))

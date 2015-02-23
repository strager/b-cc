from b.foreign.call import call_with_eh
from b.native import NativeObject
import b.foreign.all as foreign
import ctypes

class ByteSource(NativeObject):
    __token = object()

    # TODO(strager): Allow Python-created ByteSource.
    def __init__(self, token, pointer):
        if token is not ByteSource.__token:
            raise Exception('Use from_pointer')
        self.__pointer = pointer

    @classmethod
    def from_pointer(cls, pointer):
        return cls(ByteSource.__token, pointer)

    def to_pointer(self):
        return self.__pointer

    def read_1(self):
        u8 = ctypes.c_uint8()
        call_with_eh(
            foreign.deserialize_1,
            self.__pointer,
            ctypes.byref(u8),
        )
        return u8

    def read_2_be(self):
        u16 = ctypes.c_uint16()
        call_with_eh(
            foreign.deserialize_2_be,
            self.__pointer,
            ctypes.byref(u16),
        )
        return u16

    def read_4_be(self):
        u32 = ctypes.c_uint32()
        call_with_eh(
            foreign.deserialize_4_be,
            self.__pointer,
            ctypes.byref(u32),
        )
        return u32

    def read_8_be(self):
        u64 = ctypes.c_uint64()
        call_with_eh(
            foreign.deserialize_8_be,
            self.__pointer,
            ctypes.byref(u64),
        )
        return u64

    def read_data_and_size_8_be(self):
        data = ctypes.POINTER(ctypes.c_uint8)()
        data_size = ctypes.c_size_t()
        call_with_eh(
            foreign.deserialize_data_and_size_8_be,
            self.__pointer,
            ctypes.byref(data),
            ctypes.byref(data_size),
        )
        try:
            return ctypes.string_at(data, data_size.value)
        finally:
            call_with_eh(foreign.deallocate, data)

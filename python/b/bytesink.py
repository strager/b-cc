from b.foreign.call import call_with_eh
from b.native import NativeObject
import b.foreign.all as foreign
import ctypes

def make_byte_buffer(bytes):
    return ctypes.cast(
        (ctypes.c_uint8 * len(bytes)) \
            .from_buffer_copy(bytes),
        ctypes.POINTER(ctypes.c_uint8),
    )

class ByteSink(NativeObject):
    __token = object()

    # TODO(strager): Allow Python-created ByteSinks.
    def __init__(self, token, pointer):
        if token is not ByteSink.__token:
            raise Exception('Use from_pointer')
        self.__pointer = pointer

    @classmethod
    def from_pointer(cls, pointer):
        return cls(ByteSink.__token, pointer)

    def to_pointer(self):
        return self.__pointer

    def write_1(self, u8):
        call_with_eh(
            foreign.serialize_1,
            self.__pointer,
            u8,
        )

    def write_2_be(self, u16):
        call_with_eh(
            foreign.serialize_2_be,
            self.__pointer,
            u16,
        )

    def write_4_be(self, u32):
        call_with_eh(
            foreign.serialize_4_be,
            self.__pointer,
            u32,
        )

    def write_8_be(self, u64):
        call_with_eh(
            foreign.serialize_8_be,
            self.__pointer,
            u64,
        )

    def write_bytes(self, bytes):
        call_with_eh(
            foreign.serialize_8_be,
            self.__pointer,
            make_byte_buffer(bytes),
            len(bytes),
        )

    def write_data_and_size_8_be(self, bytes):
        call_with_eh(
            foreign.serialize_data_and_size_8_be,
            self.__pointer,
            make_byte_buffer(bytes),
            len(bytes),
        )

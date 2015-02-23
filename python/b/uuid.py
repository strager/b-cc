from b.long import long
import b.foreign.all as foreign
import ctypes

class UUID(object):
    def __init__(self, a, b, c, d, e):
        self.__a = long(a)
        self.__b = long(b)
        self.__c = long(c)
        self.__d = long(d)
        self.__e = long(e)

    @classmethod
    def from_foreign(cls, uuid):
        return cls.from_list(uuid.data)

    def to_foreign(self):
        # TODO(strager): Use from_buffer_copy.
        return foreign.UUID(
            data=(ctypes.c_uint8 * 16)(*self.to_list()),
        )

    @classmethod
    def from_list(cls, bytes):
        return cls(
            0
            | (bytes[ 0] & 0xFF) << 24
            | (bytes[ 1] & 0xFF) << 16
            | (bytes[ 2] & 0xFF) <<  8
            | (bytes[ 3] & 0xFF) <<  0,

            0
            | (bytes[ 4] & 0xFF) <<  8
            | (bytes[ 5] & 0xFF) <<  0,

            0
            | (bytes[ 6] & 0xFF) <<  8
            | (bytes[ 7] & 0xFF) <<  0,

            0
            | (bytes[ 8] & 0xFF) <<  8
            | (bytes[ 9] & 0xFF) <<  0,

            0
            | (bytes[10] & 0xFF) << 40
            | (bytes[11] & 0xFF) << 32
            | (bytes[12] & 0xFF) << 24
            | (bytes[13] & 0xFF) << 16
            | (bytes[14] & 0xFF) <<  8
            | (bytes[15] & 0xFF) <<  0,
        )

    def to_list(self):
        # FIXME(strager): This is as wrong as the C
        # implementation.
        return [
            (self.__a >> 24) & 0xFF,
            (self.__a >> 16) & 0xFF,
            (self.__a >>  8) & 0xFF,
            (self.__a >>  0) & 0xFF,

            (self.__b >>  8) & 0xFF,
            (self.__b >>  0) & 0xFF,

            (self.__c >>  8) & 0xFF,
            (self.__c >>  0) & 0xFF,

            (self.__d >>  8) & 0xFF,
            (self.__d >>  0) & 0xFF,

            (self.__e >> 40) & 0xFF,
            (self.__e >> 32) & 0xFF,
            (self.__e >> 24) & 0xFF,
            (self.__e >> 16) & 0xFF,
            (self.__e >>  8) & 0xFF,
            (self.__e >>  0) & 0xFF,
        ]

    def __repr__(self):
        return (
            'UUID({:#010x}, {:#02x}, {:#02x}, {:#02x}, '
            '{:#014x})'
        ).format(
            self.__a,
            self.__b,
            self.__c,
            self.__d,
            self.__e,
        )

    def __eq__(self, other):
        if type(self) is not type(other):
            return NotImplemented
        if type(self) is not UUID:
            return NotImplemented
        return (
            self.__a,
            self.__b,
            self.__c,
            self.__d,
            self.__e,
        ) == (
            other.__a,
            other.__b,
            other.__c,
            other.__d,
            other.__e,
        )

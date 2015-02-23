from b.foreign.call import call_with_eh
import b.foreign.all as foreign
import ctypes

class Database(object):
    SQLITE_OPEN_CREATE = 0x00000004
    SQLITE_OPEN_READONLY = 0x00000001
    SQLITE_OPEN_READWRITE = 0x00000002

    __token = object()

    def __init__(self, token, pointer):
        if token is not self.__token:
            raise Exception('Call load_sqlite')
        self.__pointer = pointer

    @classmethod
    def load_sqlite(cls, path, flags, vfs=None):
        if vfs is not None:
            raise NotImplementedError('vfs')
        pointer = ctypes.POINTER(foreign.Database)()
        call_with_eh(
            foreign.database_load_sqlite,
            sqlite_path=path.encode('utf8'),
            sqlite_flags=flags,
            sqlite_vfs=vfs,
            out=ctypes.byref(pointer),
        )
        return cls(cls.__token, pointer)

    def to_pointer(self):
        return self.__pointer

    def recheck_all(self, question_classes):
        vtable_set_pointer \
            = ctypes.POINTER(foreign.QuestionVTableSet)()
        call_with_eh(
            foreign.question_vtable_set_allocate,
            ctypes.byref(vtable_set_pointer),
        )
        try:
            for question_class in question_classes:
                call_with_eh(
                    foreign.question_vtable_set_add,
                    vtable_set_pointer,
                    question_class.vtable_pointer(),
                )
            call_with_eh(
                foreign.database_recheck_all,
                database=self.__pointer,
                question_vtable_set=vtable_set_pointer,
            )
        finally:
            call_with_eh(
                foreign.question_vtable_set_deallocate,
                vtable_set_pointer,
            )

    def __del__(self):
        raise NotImplementedError()

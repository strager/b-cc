from b.database import Database
import b.exception as exception
import b.internal as b

import ctypes

class DatabaseInMemory(Database):
    def __init__(self, ptr=None):
        if ptr is None:
            ptr = b.lib.b_database_in_memory_allocate()
        super(DatabaseInMemory, self).__init__(
            ptr=ptr,
            vtable=b.lib.b_database_in_memory_vtable())

    def serialize_file(self, file_path):
        err_code = b.lib.b_serialize_to_file_path(
            file_path,
            self._ptr,
            b.lib.b_database_in_memory_serialize)
        if err_code:
            # TODO Something more Pythonic.
            raise OSException("Error code " + err_code)

    @staticmethod
    def deserialize_file(file_path):
        question_vtables = [b.lib.b_file_question_vtable()]

        question_vtables_ptr = b.lib.b_question_vtable_list_allocate()
        for vtable in question_vtables:
            b.lib.b_question_vtable_list_add(
                question_vtables_ptr,
                vtable)

        database_ptr = ctypes.c_void_p()
        err_code = b.lib.b_deserialize_from_file_path1(
            file_path,
            ctypes.byref(database_ptr),
            b.lib.b_database_in_memory_deserialize,
            question_vtables_ptr)
        if err_code:
            # TODO Something more Pythonic.
            raise OSException("Error code " + err_code)

        return DatabaseInMemory(ptr=database_ptr)

    def recheck_all(self):
        exception.raise_foreign(
            b.lib.b_database_in_memory_recheck_all,
            self._ptr)

from b.foreign.Error import ErrorHandler
from b.foreign.QuestionVTableSet import QuestionVTableSet
from b.foreign.core import b_export_func
from ctypes import POINTER
import ctypes

class Database(ctypes.Structure):
    # Opaque.
    pass

database_load_sqlite = b_export_func(
    'b_database_load_sqlite',
    args=[
        ('sqlite_path', ctypes.c_char_p),
        ('sqlite_flags', ctypes.c_int),
        ('sqlite_vfs', ctypes.c_char_p),
        ('out', POINTER(POINTER(Database))),
        ('eh', POINTER(ErrorHandler)),
    ],
)

database_close = b_export_func('b_database_close', args=[
    ('database', POINTER(Database)),
    ('eh', POINTER(ErrorHandler)),
])

database_recheck_all = b_export_func(
    'b_database_recheck_all',
    args=[
        ('database', POINTER(Database)),
        ('question_vtable_set', POINTER(QuestionVTableSet)),
        ('eh', POINTER(ErrorHandler)),
    ],
)

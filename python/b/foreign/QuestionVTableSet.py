import ctypes

from b.foreign.Error import ErrorHandler
from b.foreign.QuestionAnswer import QuestionVTable
from b.foreign.UUID import UUID
from b.foreign.core import b_export_func
from ctypes import POINTER

class QuestionVTableSet(ctypes.Structure):
    # Opaque.
    pass

question_vtable_set_allocate = b_export_func(
    'b_question_vtable_set_allocate',
    args=[
        ('out', POINTER(POINTER(QuestionVTableSet))),
        ('eh', POINTER(ErrorHandler)),
    ],
)

question_vtable_set_deallocate = b_export_func(
    'b_question_vtable_set_deallocate',
    args=[
        ('question_vtable_set', POINTER(QuestionVTableSet)),
        ('eh', POINTER(ErrorHandler)),
    ],
)

question_vtable_set_add = b_export_func(
    'b_question_vtable_set_add',
    args=[
        ('question_vtable_set', POINTER(QuestionVTableSet)),
        ('question_vtable', POINTER(QuestionVTable)),
        ('eh', POINTER(ErrorHandler)),
    ],
)

question_vtable_set_look_up = b_export_func(
    'b_question_vtable_set_look_up',
    args=[
        ('question_vtable_set', POINTER(QuestionVTableSet)),
        ('uuid', UUID),
        ('out', POINTER(POINTER(QuestionVTable))),
        ('eh', POINTER(ErrorHandler)),
    ],
)

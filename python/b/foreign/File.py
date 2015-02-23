from b.foreign.Error import ErrorHandler
from b.foreign.FilePath import FilePath
from b.foreign.QuestionAnswer import Answer
from b.foreign.QuestionAnswer import QuestionVTable
from b.foreign.core import b_export_func
from ctypes import POINTER

file_contents_question_vtable = b_export_func(
    'b_file_contents_question_vtable',
    args=[],
    restype=POINTER(QuestionVTable),
)

directory_listing_question_vtable = b_export_func(
    'b_directory_listing_question_vtable',
    args=[],
    restype=POINTER(QuestionVTable),
)

directory_listing_answer_strings = b_export_func(
    'b_directory_listing_answer_strings',
    args=[
        POINTER(Answer),
        POINTER(POINTER(FilePath)),
        POINTER(ErrorHandler),
    ],
)

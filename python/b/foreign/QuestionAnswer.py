from b.foreign.Deserialize import ByteSource
from b.foreign.Error import ErrorHandler
from b.foreign.Serialize import ByteSink
from b.foreign.UUID import UUID
from b.foreign.core import b_export_func
from b.foreign.core import b_func
from ctypes import POINTER
from ctypes import c_bool
from ctypes import c_size_t
from ctypes import c_uint8
from ctypes import c_void_p
import ctypes

class Question(ctypes.Structure): pass
class QuestionVTable(ctypes.Structure): pass
class Answer(ctypes.Structure): pass
class AnswerVTable(ctypes.Structure): pass

AnswerCallback = b_func(argtypes=[
    POINTER(Answer),
    c_void_p,
    POINTER(ErrorHandler),
])

Question._fields_ = []

QuestionVTableAnswer = b_func(argtypes=[
    POINTER(Question),
    POINTER(POINTER(Answer)),
    POINTER(ErrorHandler),
])
QuestionVTableEqual = b_func(argtypes=[
    POINTER(Question),
    POINTER(Question),
    POINTER(c_bool),
    POINTER(ErrorHandler),
])
QuestionVTableReplicate = b_func(argtypes=[
    POINTER(Question),
    POINTER(POINTER(Question)),
    POINTER(ErrorHandler),
])
QuestionVTableDeallocate = b_func(argtypes=[
    POINTER(Question),
    POINTER(ErrorHandler),
])
QuestionVTableSerialize = b_func(argtypes=[
    POINTER(Question),
    POINTER(ByteSink),
    POINTER(ErrorHandler),
])
QuestionVTableDeserialize = b_func(argtypes=[
    POINTER(ByteSource),
    POINTER(POINTER(Question)),
    POINTER(ErrorHandler),
])

QuestionVTable._fields_ = [
    ('uuid', UUID),
    ('answer_vtable', POINTER(AnswerVTable)),
    # TODO(strager): Rename to query_answer.
    ('answer', QuestionVTableAnswer),
    ('equal', QuestionVTableEqual),
    ('replicate', QuestionVTableReplicate),
    ('deallocate', QuestionVTableDeallocate),
    ('serialize', QuestionVTableSerialize),
    ('deserialize', QuestionVTableDeserialize),
]

Answer._fields_ = []

AnswerVTableEqual = b_func(argtypes=[
    POINTER(Answer),
    POINTER(Answer),
    POINTER(c_bool),
    POINTER(ErrorHandler),
])
AnswerVTableReplicate = b_func(argtypes=[
    POINTER(Answer),
    POINTER(POINTER(Answer)),
    POINTER(ErrorHandler),
])
AnswerVTableDeallocate = b_func(argtypes=[
    POINTER(Answer),
    POINTER(ErrorHandler),
])
AnswerVTableSerialize = b_func(argtypes=[
    POINTER(Answer),
    POINTER(ByteSink),
    POINTER(ErrorHandler),
])
AnswerVTableDeserialize = b_func(argtypes=[
    POINTER(ByteSource),
    POINTER(POINTER(Answer)),
    POINTER(ErrorHandler),
])

AnswerVTable._fields_ = [
    ('equal', AnswerVTableEqual),
    ('replicate', AnswerVTableReplicate),
    ('deallocate', AnswerVTableDeallocate),
    ('serialize', AnswerVTableSerialize),
    ('deserialize', AnswerVTableDeserialize),
]

question_serialize_to_memory = b_export_func(
    'b_question_serialize_to_memory',
    args=[
        POINTER(Question),
        POINTER(QuestionVTable),
        POINTER(POINTER(c_uint8)),
        POINTER(c_size_t),
        POINTER(ErrorHandler),
    ],
)

answer_serialize_to_memory = b_export_func(
    'b_answer_serialize_to_memory',
    args=[
        POINTER(Answer),
        POINTER(AnswerVTable),
        POINTER(POINTER(c_uint8)),
        POINTER(c_size_t),
        POINTER(ErrorHandler),
    ],
)

question_deserialize_from_memory = b_export_func(
    'b_question_deserialize_from_memory',
    args=[
        POINTER(QuestionVTable),
        POINTER(c_uint8),
        c_size_t,
        POINTER(POINTER(Question)),
        POINTER(ErrorHandler),
    ],
)

answer_deserialize_from_memory = b_export_func(
    'b_answer_deserialize_from_memory',
    args=[
        POINTER(AnswerVTable),
        POINTER(c_uint8),
        c_size_t,
        POINTER(POINTER(Answer)),
        POINTER(ErrorHandler),
    ],
)

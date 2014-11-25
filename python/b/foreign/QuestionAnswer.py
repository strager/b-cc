import ctypes

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

QuestionVTable._fields_ = [
    ('uuid', UUID),
    ('answer_vtable', POINTER(AnswerVTable)),
    ('answer', b_func(argtypes=[
        POINTER(Question),
        POINTER(POINTER(Answer)),
        POINTER(ErrorHandler),
    ])),
    ('equal', b_func(argtypes=[
        POINTER(Question),
        POINTER(Question),
        POINTER(c_bool),
        POINTER(ErrorHandler),
    ])),
    ('replicate', b_func(argtypes=[
        POINTER(Question),
        POINTER(POINTER(Question)),
        POINTER(ErrorHandler),
    ])),
    ('deallocate', b_func(argtypes=[
        POINTER(Question),
        POINTER(ErrorHandler),
    ])),
    ('serialize', b_func(argtypes=[
        POINTER(Question),
        POINTER(ByteSink),
        POINTER(ErrorHandler),
    ])),
    ('deserialize', b_func(argtypes=[
        POINTER(ByteSource),
        POINTER(POINTER(Question)),
        POINTER(ErrorHandler),
    ])),
]

Answer._fields_ = []

AnswerVTable._fields_ = [
    ('equal', b_func(argtypes=[
        POINTER(Answer),
        POINTER(Answer),
        POINTER(c_bool),
        POINTER(ErrorHandler),
    ])),
    ('replicate', b_func(argtypes=[
        POINTER(Answer),
        POINTER(POINTER(Answer)),
        POINTER(ErrorHandler),
    ])),
    ('deallocate', b_func(argtypes=[
        POINTER(Answer),
        POINTER(ErrorHandler),
    ])),
    ('serialize', b_func(argtypes=[
        POINTER(Answer),
        POINTER(ByteSink),
        POINTER(ErrorHandler),
    ])),
    ('deserialize', b_func(argtypes=[
        POINTER(ByteSource),
        POINTER(POINTER(Answer)),
        POINTER(ErrorHandler),
    ])),
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

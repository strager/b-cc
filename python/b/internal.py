from ctypes import *
import os

_sysname = os.uname()[0]
_shared_ext = '.dylib' if _sysname == 'Darwin' else '.so'

_cur_dir = os.path.dirname(os.path.realpath(__file__))
_lib_dir = os.path.join(_cur_dir, '..', '..', 'lib')

BEqual = CFUNCTYPE(
    c_bool,
    c_void_p,
    c_void_p)
BReplicate = CFUNCTYPE(
    c_void_p,
    c_void_p)
BDeallocate = CFUNCTYPE(
    None,
    c_void_p)

class BExceptionStructure(Structure):
    pass

BExceptionStructure._fields_ = [
    ("uuid", c_char_p),
    ("message", c_char_p),
    ("data", c_void_p),
    ("deallocate", BDeallocate)]

class BAnswerVTableStructure(Structure):
    _fields_ = [
        ("equal", BEqual),
        ("replicate", BReplicate),
        ("deallocate", BDeallocate)]

BRuleAnswer = CFUNCTYPE(
    c_void_p,
    c_void_p,
    POINTER(POINTER(BExceptionStructure)))

class BQuestionVTableStructure(Structure):
    _fields_ = [
        ("uuid", c_char_p),
        ("answer_vtable", POINTER(BAnswerVTableStructure)),
        ("answer", BRuleAnswer),
        ("equal", BEqual),
        ("replicate", BReplicate),
        ("deallocate", BDeallocate)]

class BRuleVTableStructure(Structure):
    pass

BRuleAdd = CFUNCTYPE(
    None,
    c_void_p,
    c_void_p)
BRuleQuery = CFUNCTYPE(
    None,
    c_void_p,
    c_void_p,
    POINTER(BQuestionVTableStructure),
    c_void_p,
    c_void_p,
    POINTER(POINTER(BExceptionStructure)))

BRuleVTableStructure._fields_ = [
    ("uuid", c_char_p),
    ("add", BRuleAdd),
    ("query", BRuleQuery),
    ("deallocate", BDeallocate)]

BDatabaseAddDependency = CFUNCTYPE(
    None,
    c_void_p,
    c_void_p,
    POINTER(BQuestionVTableStructure),
    c_void_p,
    POINTER(BQuestionVTableStructure),
    POINTER(POINTER(BExceptionStructure)))
BDatabaseGetAnswer = CFUNCTYPE(
    c_void_p,
    c_void_p,
    c_void_p,
    POINTER(BQuestionVTableStructure),
    POINTER(POINTER(BExceptionStructure)))
BDatabaseSetAnswer = CFUNCTYPE(
    None,
    c_void_p,
    c_void_p,
    POINTER(BQuestionVTableStructure),
    c_void_p,
    POINTER(POINTER(BExceptionStructure)))

class BDatabaseVTableStructure(Structure):
    _fields_ = [
        ("deallocate", BDeallocate),
        ("add_dependency", BDatabaseAddDependency),
        ("get_answer", BDatabaseGetAnswer),
        ("set_answer", BDatabaseSetAnswer)]

class BBuildContextInfoStructure(Structure):
    _fields_ = [
        ("database", c_void_p),
        ("database_vtable", POINTER(BDatabaseVTableStructure)),
        ("rule", c_void_p),
        ("rule_vtable", POINTER(BRuleVTableStructure))]

FileRuleCallback = CFUNCTYPE(
    None,
    c_void_p,
    c_char_p,
    POINTER(POINTER(BExceptionStructure)))

lib = CDLL(os.path.join(_lib_dir, 'libb' + _shared_ext))

lib.b_file_question_allocate.argtypes = [
    c_char_p]
lib.b_file_question_allocate.restype = c_void_p
lib.b_file_question_deallocate.argtypes = [
    c_void_p]
lib.b_file_question_deallocate.restype = None
lib.b_file_question_vtable.argtypes = []
lib.b_file_question_vtable.restype = c_void_p

lib.b_database_in_memory_allocate.argtypes = []
lib.b_database_in_memory_allocate.restype = c_void_p
lib.b_database_in_memory_deallocate.argtypes = [
    c_void_p]
lib.b_database_in_memory_deallocate.restype = None
lib.b_database_in_memory_vtable.argtypes = []
lib.b_database_in_memory_vtable.restype = c_void_p

lib.b_build_context_allocate.argtypes = [
    POINTER(BBuildContextInfoStructure)]
lib.b_build_context_allocate.restype = c_void_p
lib.b_build_context_info.argtypes = [
    c_void_p]
lib.b_build_context_info.restype = POINTER(BBuildContextInfoStructure)
lib.b_build_context_deallocate.argtypes = [
    c_void_p]
lib.b_build_context_deallocate.restype = None
lib.b_build_context_need.argtypes = [
    c_void_p,
    POINTER(c_void_p),
    POINTER(POINTER(BQuestionVTableStructure)),
    c_size_t,
    POINTER(POINTER(BExceptionStructure))]
lib.b_build_context_need.restype = None
lib.b_build_context_need_one.argtypes = [
    c_void_p,
    c_void_p,
    BQuestionVTableStructure,
    POINTER(POINTER(BExceptionStructure))]
lib.b_build_context_need_one.restype = None

lib.b_file_rule_add.argtypes = [
    c_void_p,
    c_char_p,
    FileRuleCallback]
lib.b_file_rule_add.restype = None
lib.b_file_rule_allocate.argtypes = []
lib.b_file_rule_allocate.restype = c_void_p
lib.b_file_rule_deallocate.argtypes = [
    c_void_p]
lib.b_file_rule_deallocate.restype = None
lib.b_file_rule_vtable.argtypes = []
lib.b_file_rule_vtable.restype = POINTER(BRuleVTableStructure)

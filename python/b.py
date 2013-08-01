from ctypes import *
import os

_sysname = os.uname()[0]
_shared_ext = '.dylib' if _sysname == 'Darwin' else '.so'

_cur_dir = os.path.dirname(os.path.realpath(__file__))
_lib_dir = os.path.join(_cur_dir, '..', 'lib')

class BException(Structure):
    pass

BExceptionDeallocate = CFUNCTYPE(None, POINTER(BException))

BException._fields_ = [
    ("uuid", c_char_p),
    ("message", c_char_p),
    ("data", c_void_p),
    ("deallocate", BExceptionDeallocate)]

FileRuleCallback = CFUNCTYPE(
    None,
    c_void_p,
    c_char_p,
    POINTER(POINTER(BException)))

lib = CDLL(os.path.join(_lib_dir, 'libb' + _shared_ext))

lib.b_file_rule_add.argtypes = [c_void_p, c_char_p, FileRuleCallback]
lib.b_file_rule_add.restype = None
lib.b_file_rule_allocate.argtypes = []
lib.b_file_rule_allocate.restype = c_void_p
lib.b_file_rule_deallocate.argtypes = [c_void_p]
lib.b_file_rule_deallocate.restype = None
lib.b_file_rule_vtable.argtypes = []
lib.b_file_rule_vtable.restype = c_void_p

lib.b_file_question_allocate.argtypes = [c_char_p]
lib.b_file_question_allocate.restype = c_void_p
lib.b_file_question_deallocate.argtypes = [c_void_p]
lib.b_file_question_deallocate.restype = None
lib.b_file_question_vtable.argtypes = []
lib.b_file_question_vtable.restype = c_void_p

lib.b_database_in_memory_allocate.argtypes = []
lib.b_database_in_memory_allocate.restype = c_void_p
lib.b_database_in_memory_deallocate.argtypes = [c_void_p]
lib.b_database_in_memory_deallocate.restype = None
lib.b_database_in_memory_vtable.argtypes = []
lib.b_database_in_memory_vtable.restype = c_void_p

lib.b_build_context_allocate.argtypes = [c_void_p, c_void_p, c_void_p, c_void_p]
lib.b_build_context_allocate.restype = c_void_p
lib.b_build_context_deallocate.argtypes = [c_void_p]
lib.b_build_context_deallocate.restype = None
lib.b_build_context_need.argtypes = [c_void_p, POINTER(c_void_p), POINTER(c_void_p), c_size_t, POINTER(POINTER(BException))]
lib.b_build_context_need.restype = None
lib.b_build_context_need_one.argtypes = [c_void_p, c_void_p, c_void_p, POINTER(POINTER(BException))]
lib.b_build_context_need_one.restype = None

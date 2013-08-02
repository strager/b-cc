import b.exception as exception
import b.internal as b

import ctypes

_build_contexts = {}

class BuildContext(object):
    def __init__(self, database, rule):
        self._database = database
        self._rule = rule
        self._ptr = b.lib.b_build_context_allocate(
            b.BBuildContextInfoStructure(
                database=database._ptr,
                database_vtable=database._vtable,
                rule=rule._ptr,
                rule_vtable=rule._vtable))

        info = b.lib.b_build_context_info(self._ptr).contents
        _build_contexts[ctypes.addressof(info)] = self

    @staticmethod
    def from_ptr(ctx_ptr):
        info = b.lib.b_build_context_info(ctx_ptr).contents
        return _build_contexts[ctypes.addressof(info)]

    def need_one(self, question):
        self.need([question])

    def need(self, questions):
        new_questions = [q.replicate() for q in questions]

        try:
            # FIXME Partition functionally.
            qs = []
            q_vtables = []
            for new_question in new_questions:
                qs.append(new_question._ptr)
                q_vtables.append(new_question._vtable)

            count = len(new_questions)
            qs_array = (ctypes.c_void_p * count)(*qs)
            q_vtables_array = (ctypes.POINTER(b.BQuestionVTableStructure) * count)(*q_vtables)

            exception.raise_foreign(
                b.lib.b_build_context_need,
                self._ptr,
                qs_array,
                q_vtables_array,
                count)

        finally:
            for new_question in new_questions:
                new_question.deallocate()

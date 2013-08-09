import b.exception as exception
import b.internal as b

import ctypes

class BuildContext(object):
    def __init__(self, **kwargs):
        """Parameters: database and rule, or ptr"""
        # WTB Overloading...
        if 'ptr' in kwargs:
            self._ptr = kwargs['ptr']
        else:
            self._ptr = BuildContext._create(**kwargs)

    @staticmethod
    def _create(database, rule):
        return b.lib.b_build_context_allocate(
            database._ptr,
            database._vtable,
            rule._ptr,
            rule._vtable)

    @staticmethod
    def from_ptr(ctx_ptr):
        return BuildContext(ptr=ctx_ptr)

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

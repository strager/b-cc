import b.internal as b
from b.question import Question

class FileQuestion(Question):
    def __init__(self, filename):
        super(FileQuestion, self).__init__(
            ptr=b.lib.b_file_question_allocate(filename),
            vtable=b.lib.b_file_question_vtable())

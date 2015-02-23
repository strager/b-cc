#from b.file import FileQuestion
from b.nativefile import FileQuestion
from b.gen import gen_many
from b.gen import gen_return
from tempfile import NamedTemporaryFile
import b.main
import logging
import os

logger = logging.getLogger(__name__)

def _gen_gcc_dependencies(ac, cc, flags):
    # TODO(strager): Something more comprehensive for
    # compiler dependencies.
    #questions = [FileQuestion.from_path(cc)]
    questions = []

    skip = 0
    for flag in flags:
        if skip > 0:
            skip -= 1
            continue
        if flag.startswith('-'):
            if flag == '-MF':
                skip = 1
            elif flag == '-o':
                skip = 1
            elif flag.startswith('-l'):
                if flag == '-l':
                    skip = 1
            elif flag.startswith('-I'):
                # FIXME(strager)
                if flag == '-I':
                    skip = 1
            elif flag in ['-MD', '-c']:
                pass  # Ignore.
            else:
                logger.warn('Unknown flag %s', flag)
        else:
            questions.append(FileQuestion.from_path(flag))

    yield ac.gen_need(questions)

def gen_gcc(ac, cc, flags, output_path):
    with NamedTemporaryFile() as dep_file:
        flags += ['-MD', '-MF', dep_file.name]
        flags += ['-o', output_path]
        yield gen_many(
            _gen_gcc_dependencies(ac, cc, flags),
            ac.process_controller.gen_check_exec_basic(
                [cc] + flags),
        )
        #add_dependencies_from_dep_file(ac, dep_file.name)

class CTargetBase(object):
    LANGUAGE_C = 'c'
    LANGUAGE_CXX = 'cxx'

    def _gen_c_compile_flags(
        self,
        ac,
        object_path,
        source_path,
    ):
        yield gen_return([])

    def _gen_cxx_compile_flags(
        self,
        ac,
        object_path,
        source_path,
    ):
        yield gen_return([])

    def _gen_c_compiler(self, ac):
        yield gen_return('clang')

    def _gen_cxx_compiler(self, ac):
        yield gen_return('clang++')

    def __gen_compiler(self, ac, language):
        if language == CTargetBase.LANGUAGE_C:
            return self._gen_c_compiler(ac)
        elif language == CTargetBase.LANGUAGE_CXX:
            return self._gen_cxx_compiler(ac)
        else:
            raise ValueError('Unknown language')

    def __gen_compile_flags(
            self,
            ac,
            object_path,
            source_path,
            language,
    ):
        if language == CTargetBase.LANGUAGE_C:
            return self._gen_c_compile_flags(
                ac, object_path, source_path)
        elif language == CTargetBase.LANGUAGE_CXX:
            return self._gen_cxx_compile_flags(
                ac, object_path, source_path)
        else:
            raise ValueError('Unknown language')

    def _gen_compile(self, ac, object_path):
        # Validate that object_path is valid.
        object_paths = yield self.gen_object_paths(ac)
        if object_path not in object_paths:
            raise ValueError(
                'Illegal object_path ({})'.format(
                    object_path))

        source_path = yield self._gen_object_source(
            object_path)
        language = yield self._gen_source_language(
            ac, source_path)
        (compiler, flags) = yield gen_many(
            self.__gen_compiler(ac, language),
            self.__gen_compile_flags(
                ac, object_path, source_path, language),
        )
        yield gen_gcc(
            ac,
            compiler,
            ['-c', source_path] + flags,
            object_path,
        )

    def _gen_linker(self, ac):
        # TODO(strager)
        return self._gen_cxx_compiler(ac)

    def _gen_link_flags(self, ac):
        yield gen_return([])

    def _gen_link(self, ac):
        (ld, flags, object_paths, output_path) = \
            yield gen_many(
                self._gen_linker(ac),
                self._gen_link_flags(ac),
                self.gen_object_paths(ac),
                self.gen_output_path(ac),
            )
        yield gen_gcc(
            ac, ld, flags + object_paths, output_path)

    def _gen_output_path(self, ac):
        raise NotImplementedError()

    def _gen_object_paths(self, ac):
        raise NotImplementedError()

    @property
    def _object_file_suffix(self):
        return '.o'

    def _gen_object_source(self, object_path):
        # FIXME(strager): We should only trim from the end.
        yield gen_return(object_path.replace(
            self._object_file_suffix, ''))

    def _gen_source_language(self, ac, source_path):
        if source_path.endswith('.c'):
            yield gen_return(CTargetBase.LANGUAGE_C)
        else:
            yield gen_return(CTargetBase.LANGUAGE_CXX)

    def _gen_dispatch_question(self, ac):
        question = ac.question
        if isinstance(question, FileQuestion):
            output_path = yield self.gen_output_path(ac)
            if output_path == question.path:
                yield self.gen_link(ac)
                yield ac.gen_success()
                yield gen_return(True)
            object_paths = yield self.gen_object_paths(ac)
            if question.path in object_paths:
                yield self.gen_compile(ac, question.path)
                yield ac.gen_success()
                yield gen_return(True)
        yield gen_return(False)

    def gen_compile(self, ac, object_path):
        return self._gen_compile(ac, object_path)

    def gen_object_paths(self, ac):
        return self._gen_object_paths(ac)

    def gen_output_path(self, ac):
        return self._gen_output_path(ac)

    def gen_link(self, ac):
        return self._gen_link(ac);

    def gen_dispatch_question(self, ac):
        return self._gen_dispatch_question(ac)

class SimpleCTarget(CTargetBase):
    def __init__(
            self,
            name,
            sources,
            compile_flags=[],
            c_compile_flags=[],
            cxx_compile_flags=[],
    ):
        super(SimpleCTarget, self).__init__()
        self.__name = str(name)
        self.__sources = list(sources)
        self.__compile_flags = list(compile_flags)
        self.__c_compile_flags \
            = list(compile_flags + c_compile_flags)
        self.__cxx_compile_flags \
            = list(compile_flags + cxx_compile_flags)

    def _gen_c_compile_flags(
        self,
        ac,
        object_path,
        source_path,
    ):
        yield gen_return(self.__c_compile_flags)

    def _gen_cxx_compile_flags(
        self,
        ac,
        object_path,
        source_path,
    ):
        yield gen_return(self.__cxx_compile_flags)

    def _gen_link_flags(self, ac):
        # TODO(strager)
        yield gen_return(self.__cxx_compile_flags)

    def _gen_output_path(self, ac):
        yield gen_return(self.__name)

    def _gen_object_paths(self, ac):
        yield gen_return([
            s + self._object_file_suffix
            for s
            in self.__sources
        ])

dispatch_list = []

dispatch_list.append(SimpleCTarget(name='b-cc', sources=[
    'ex/1/main.cc',
    'src/Alloc.c',
    'src/AnswerContext.c',
    'src/Assert.c',
    'src/Database.c',
    'src/Deserialize.c',
    'src/Error.c',
    'src/File.c',
    'src/FilePath.c',
    'src/Log.c',
    'src/Main.c',
    'src/Misc.c',
    'src/OS.c',
    'src/Process.c',
    'src/QuestionAnswer.c',
    'src/QuestionDispatch.c',
    'src/QuestionQueue.c',
    'src/QuestionQueueRoot.c',
    'src/QuestionVTableSet.c',
    'src/RefCount.c',
    'src/SQLite3.c',
    'src/Serialize.c',
    'src/Thread.c',
    'src/UUID.c',
    'vendor/sqlite-3.8.4.1/sqlite3.c',
], compile_flags=[
    '-I',
    'vendor/sqlite-3.8.4.1',
    '-I',
    'include',
], c_compile_flags=[
    '-std=c99',
], cxx_compile_flags=[
    '-stdlib=libc++',
    '-std=c++11',
]))

class Foo(object):
    def gen_dispatch_question(self, ac):
        if isinstance(ac.question, FileQuestion):
            if not os.path.isfile(ac.question.path):
                yield gen_return(False)
            yield ac.gen_success_answer(
                ac.question.query_answer(),
            )
            yield gen_return(True)
        yield gen_return(False)
dispatch_list.append(Foo())

answer = b.main.main(
    question=FileQuestion.from_path('b-cc'),
    question_classes=[
        FileQuestion,
    ],
    dispatch_callback=b.main.list_dispatch_callback(
        dispatch_list,
    ),
    database_sqlite_path='b_database.sqlite3',
)

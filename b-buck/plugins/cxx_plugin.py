from buck.plugin import Plugin
from buck.target import Target
from buck.targetpath import TargetPath

class CxxPlugin(Plugin):
    def get_buckfile_globals(self):
        return {
            'cxx_executable': self.cxx_executable,
        }

    def get_target_classes(self):
        return [CxxExecutableTarget]

    def cxx_executable(self, name, deps, srcs):
        self._add_target(CxxExecutableTarget(
            name=name,
            deps=map(TargetPath.parse, deps),
            srcs=srcs,
        ))

class CxxExecutableTarget(Target):
    def __init__(self, name, deps, srcs):
        super(Target, self).__init__(name=name, deps=deps)
        self.__srcs = srcs

    def dispatch(self, question):
        if isinstance(question, FileQuestion):
            if question.path.endswith('.o'):
                self.compile()
                return True
            else:
                self.link()
                return True
        else:
            todo()
            return False


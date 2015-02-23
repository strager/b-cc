from buck.buckfile import BuckfileQuestion

class Target(Question):
    def __init__(self, path, deps):
        self.__path = path
        self.__deps = frozenset(deps)

    def query_answer(self):
        return None

class TargetAnswer(Answer):
    def __init__(self, target):
        self.__target = target

    @property
    def target(self):
        return self.__target

class TargetQuestion(Question):
    def __init__(self, target_path):
        self.__target_path = target_path

    def query_answer(self):
        buckfile = BuckfileQuestion(self.__path).answer()
        return TargetAnswer(
            buckfile.targets[self.__target_path],
        )

import buck.plugin
import buck.target

class _BuckfileTargetObserver(buck.plugin.TargetObserver):
    def __init__(self, buckfile_path):
        self.__buckfile_path = buckfile_path
        self.__targets = []

    def target_added(self, target):
        self.__targets.append(target)

    def get_targets(self):
        return list(self.__targets)

    def make_target_path(self, target_name):
        return buck.target.TargetPath(
            buckfile_path=self.__buckfile_path,
            target_name=target_name,
        )

class BuckfileParser(object):
    def __init__(self, plugins):
        # FIXME(strager): Should we sort plugins somehow?
        self.__plugins = list(plugins)

    def __globals(self):
        globals = {'__builtins__': {}}
        for plugin in self.__plugins:
            # FIXME(strager): Should we warn or error on
            # collisions?
            globals.update(plugin.get_buckfile_globals())
        return globals

    def exec_path(self, path):
        # FIXME(strager): Should we have an AnswerContext
        # here, so we can ask a FileQuestion about path?
        with open(path, 'rb') as path:
            script = path.read()
        # See help(compile) for details on what these
        # parameters mean.
        compiled = compile(script, path, 'eval', 0, True)

        target_observer = _BuckfileTargetObserver()
        for plugin in self.__plugins:
            plugin.set_target_observer(target_observer)
        eval(compiled, globals=self.__globals())
        for plugin in self.__plugins:
            plugin.unset_target_observer()
        return target_observer.get_targets()

class BuckfileAnswer(Answer):
    def __init__(self, targets):
        self.__targets = targets

    @property
    def targets(self):
        return dict(self.__targets)

class BuckfileQuestion(Question):
    def __init__(self, path):
        self.__path = path

    def answer(self):
        file_contents \
            = FileQuestion(self.__path).answer()
        targets = py_eval(file_contents)
        return BuckfileAnswer(targets)

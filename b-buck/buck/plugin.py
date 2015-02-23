import abc

class TargetObserver(object):
    __metaclass__ = abc.ABCMeta

    @abc.abstractmethod
    def target_added(self, target):
        pass

    @abc.abstractmethod
    def make_target_path(self, target_name):
        pass

class Plugin(object):
    __metaclass__ = abc.ABCMeta

    def __init__(self):
        self.__target_observer = None

    def set_target_observer(self, target_observer):
        self.__target_observer = target_observer

    def unset_target_observer(self):
        self.__target_observer = None

    def _add_target(self, target):
        observer = self.__target_observer
        if observer is None:
            raise Exception('No target observer set')
        observer.target_added(target)

    def make_target_path(self, target_name):
        return self.__target_observer.make_target_path(
            target_name=target_name,
        )

    @abc.abstractmethod
    def get_buckfile_globals(self):
        pass

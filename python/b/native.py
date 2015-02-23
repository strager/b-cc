import b.abc as abc

class NativeObject(abc.ABC):
    @abc.abstractmethod
    #@staticmethod
    def from_pointer(pointer):
        pass

    @abc.abstractmethod
    def to_pointer(self):
        pass

import abc

# Instead of re-exporting abc.ABC, use Python 3-specific
# syntax to force a SyntaxError for Python 2. Also, Python
# >=3.0 <3.4 does not support the ABC class anyway.
class ABC(metaclass=abc.ABCMeta):
    pass

abstractmethod = abc.abstractmethod
abstractproperty = abc.abstractproperty

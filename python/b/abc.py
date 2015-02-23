'''
Emulates Python 3.4's abc package for other Python versions.
'''

try:
    import b.abc3 as abc
except (ImportError, SyntaxError):
    import b.abc2 as abc

ABC = abc.ABC
abstractmethod = abc.abstractmethod
abstractproperty = abc.abstractproperty

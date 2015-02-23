import functools
import re

@functools.total_ordering
class TargetPath(object):
    __target_path_re = re.compile(
        r'^(?P<buckfile_path>.*)(?::(?P<target_name>.*))?$',
    )

    def __init__(self, buckfile_path, target_name):
        self.__buckfile_path = buckfile_path
        self.__target_name = target_name

    @property
    def buckfile_path(self):
        return self.__buckfile_path

    @property
    def target_name(self):
        return self.__target_name

    @classmethod
    def parse(cls, string):
        match = TargetPath.__target_path_re.match(string)
        if match is None:
            raise ValueError(
                'Failed to parse target path string',
            )
        return cls(
            buckfile_path=match.get('buckfile_path'),
            target_name=match.get('target_name'),
        )

    @classmethod
    def cast(cls, object):
        if isinstance(object, basestring):
            return cls.parse(object)
        elif isinstance(object, cls):
            return object
        else:
            raise TypeError('Bad target path type')

    def __key(self):
        return (self.__buckfile_path, self.__target_name)

    def __eq__(self, other):
        if type(self) != TargetPath:
            return NotImplemented
        if type(other) != TargetPath:
            return NotImplemented
        return self.__key() == other.__key()

    def __lt__(self, other):
        if type(self) != TargetPath:
            return NotImplemented
        if type(other) != TargetPath:
            return NotImplemented
        return self.__key() < other.__key()

    def __hash__(self, other):
        return hash(self.__key())

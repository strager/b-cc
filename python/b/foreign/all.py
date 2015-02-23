'''
This module re-exports the exports of all b.foreign modules
which correspond to C modules.
'''

import importlib

def __reexport(globals, modules):
    # Export attrs of each module.
    attrs = set()
    for module_name in modules:
        module = importlib.import_module(module_name)
        for attr in dir(module):
            attrs.add(attr)
            globals[attr] = getattr(module, attr)
    return list(attrs)

__all__ = __reexport(globals=globals(), modules=[
    'b.foreign.Alloc',
    'b.foreign.AnswerContext',
    'b.foreign.Database',
    'b.foreign.Deserialize',
    'b.foreign.Error',
    'b.foreign.File',
    'b.foreign.FilePath',
    'b.foreign.Main',
    'b.foreign.Process',
    'b.foreign.QuestionAnswer',
    'b.foreign.QuestionDispatch',
    'b.foreign.QuestionQueue',
    'b.foreign.QuestionVTableSet',
    'b.foreign.Serialize',
    'b.foreign.UUID',
])

from b.buildcontext import BuildContext
from b.callbacks import Callbacks
from b.rule import Rule
import b.exception as exception
import b.internal as b

import ctypes

class FileRule(Rule):
    def __init__(self):
        super(FileRule, self).__init__(
            ptr=b.lib.b_file_rule_allocate(),
            vtable=b.lib.b_file_rule_vtable())
        self._callbacks = Callbacks(b.FileRuleCallback)

    def append(self, filename, callback):
        def cb(ctx_ptr, filename):
            ctx = BuildContext.from_ptr(ctx_ptr)
            return callback(ctx, filename)
        b.lib.b_file_rule_add(
            self._ptr,
            filename,
            self._callbacks.new(
                exception.wrap_domestic(cb)))

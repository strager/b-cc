from b.database import Database
import b.internal as b

class DatabaseInMemory(Database):
    def __init__(self):
        super(DatabaseInMemory, self).__init__(
            ptr=b.lib.b_database_in_memory_allocate(),
            vtable=b.lib.b_database_in_memory_vtable())

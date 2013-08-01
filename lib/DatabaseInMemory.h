#ifndef DATABASEINMEMORY_H_A09D97D3_7884_445A_B1B4_858889806DF3
#define DATABASEINMEMORY_H_A09D97D3_7884_445A_B1B4_858889806DF3

#ifdef __cplusplus
extern "C" {
#endif

struct B_AnyDatabase;
struct B_DatabaseVTable;

// An in-memory Database is a simple Database which stores
// question-answers and dependency information in memory.

struct B_AnyDatabase *
b_database_in_memory_allocate();

void
b_database_in_memory_deallocate(
    struct B_AnyDatabase *
);

const struct B_DatabaseVTable *
b_database_in_memory_vtable();

#ifdef __cplusplus
}
#endif

#endif

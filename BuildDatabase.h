#ifndef BUILDDATABASE_H_8305F0BE_23DF_4665_BCCD_362980C37FAF
#define BUILDDATABASE_H_8305F0BE_23DF_4665_BCCD_362980C37FAF

struct B_AnyAnswer;
struct B_AnyQuestion;
struct B_BuildDatabaseVTable;

struct B_AnyBuildDatabase {
    struct B_BuildDatabaseVTable *vtable;
};

struct B_BuildDatabaseVTable {
    struct B_AnyAnswer *(*get)(
        struct B_BuildDatabase *,
        const struct B_AnyQuestion *,
    );
    void (*set)(
        struct B_BuildDatabase *,
        const struct B_AnyQuestion *,
        const struct B_AnyAnswer *,
    );
    void (*recheck)(
        struct B_BuildDatabase *,
        const struct B_AnyQuestion *,
    );
    void (*recheckAll)(
        struct B_BuildDatabase *,
    );
    void (*addDependency)(
        struct B_BuildDatabase *,
        const struct B_AnyQuestion *from,
        const struct B_AnyQuestion *to,
    );
};

struct B_AnyAnswer *
b_build_database_get(
    struct B_BuildDatabase *,
    const struct B_AnyQuestion *,
);

void
b_build_database_set(
    struct B_BuildDatabase *,
    const struct B_AnyQuestion *,
    const struct B_AnyAnswer *,
);

void
b_build_database_recheck(
    struct B_BuildDatabase *,
    const struct B_AnyQuestion *,
);

void
b_build_database_recheckAll(
    struct B_BuildDatabase *,
);

void
b_build_database_addDependency(
    struct B_BuildDatabase *,
    const struct B_AnyQuestion *from,
    const struct B_AnyQuestion *to,
);

#endif

#ifndef BUILDDATABASE_H_8305F0BE_23DF_4665_BCCD_362980C37FAF
#define BUILDDATABASE_H_8305F0BE_23DF_4665_BCCD_362980C37FAF

struct AnyAnswer;
struct AnyQuestion;
struct BuildDatabaseVTable;

struct AnyBuildDatabase {
    struct BuildDatabaseVTable *vtable;
};

struct BuildDatabaseVTable {
    struct AnyAnswer *(*get)(
        struct BuildDatabase *,
        const struct AnyQuestion *,
    );
    void (*set)(
        struct BuildDatabase *,
        const struct AnyQuestion *,
        const struct AnyAnswer *,
    );
    void (*recheck)(
        struct BuildDatabase *,
        const struct AnyQuestion *,
    );
    void (*recheckAll)(
        struct BuildDatabase *,
    );
    void (*addDependency)(
        struct BuildDatabase *,
        const struct AnyQuestion *from,
        const struct AnyQuestion *to,
    );
};

struct AnyAnswer *
b_build_database_get(
    struct BuildDatabase *,
    const struct AnyQuestion *,
);

void
b_build_database_set(
    struct BuildDatabase *,
    const struct AnyQuestion *,
    const struct AnyAnswer *,
);

void
b_build_database_recheck(
    struct BuildDatabase *,
    const struct AnyQuestion *,
);

void
b_build_database_recheckAll(
    struct BuildDatabase *,
);

void
b_build_database_addDependency(
    struct BuildDatabase *,
    const struct AnyQuestion *from,
    const struct AnyQuestion *to,
);

#endif

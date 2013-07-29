#ifndef DATABASE_H_8305F0BE_23DF_4665_BCCD_362980C37FAF
#define DATABASE_H_8305F0BE_23DF_4665_BCCD_362980C37FAF

struct B_AnyAnswer;
struct B_AnyQuestion;
struct B_Exception;
struct B_QuestionVTable;

struct B_AnyDatabase {
};

struct B_DatabaseVTable {
    void (*deallocate)(
        struct B_AnyDatabase *,
    );

    // Expresses a dependency between two Questions.  The
    // first Question depends upon the second.  The caller
    // maintains ownership of Questions.
    void (*add_dependency)(
        struct B_AnyDatabase *,
        const struct B_AnyQuestion *from,
        const struct B_QuestionVTable *from_vtable,
        const struct B_AnyQuestion *to,
        const struct B_QuestionVTable *to_vtable,
        struct B_Exception **,
    );

    // Looks up an Answer, returning NULL if no answer is
    // available.  The caller maintains ownership of the
    // Question.  The callee gains ownership of the returned
    // Answer.
    struct B_AnyAnswer *(*get_answer)(
        struct B_AnyDatabase *,
        const struct B_AnyQuestion *,
        const struct B_QuestionVTable *,
        struct B_Exception **,
    );

    // Records the Answer to a Question.  The caller
    // maintains ownership of the Question and Answer.
    void (*set_answer)(
        struct B_AnyDatabase *,
        const struct B_AnyQuestion *,
        const struct B_QuestionVTable *,
        const struct B_AnyAnswer *,
        struct B_Exception **,
    );
};

void
b_database_vtable_validate(
    const struct B_DatabaseVTable *
);

#endif

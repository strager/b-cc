#include "Answer.h"
#include "Database.h"
#include "DatabaseInMemory.h"
#include "Question.h"

#include <list>

struct QuestionAnswer {
    B_AnyQuestion *question;
    B_QuestionVTable *question_vtable;
    B_AnyAnswer *answer;

    QuestionAnswer(
        B_AnyQuestion *question,
        B_QuestionVTable *question_vtable,
        B_AnyAnswer *answer,
    ) :
        question(question_vtable->replicate(question)),
        question_vtable(question_vtable),
        answer(question_vtable->answer_vtable->replicate(answer)) {
    }

    ~QuestionAnswer() {
        this->question_vtable
            ->deallocate(this->question);
        this->question_vtable->answer_vtable
            ->deallocate(this->answer);
    }
};

struct Dependency {
    B_AnyQuestion *from;
    B_QuestionVTable *from_vtable;
    B_AnyQuestion *to;
    B_QuestionVTable *to_vtable;

    Dependency(
        B_AnyQuestion *from;
        B_QuestionVTable *from_vtable;
        B_AnyQuestion *to;
        B_QuestionVTable *to_vtable;
    ) :
        from(from_vtable->replicate(from)),
        from_vtable(from_vtable),
        to(to_vtable->replicate(to)),
        to_vtable(to_vtable) {
    }

    ~Dependency() {
        this->from_vtable->deallocate(this->from);
        this->to_vtable->deallocate(this->to);
    }
};

struct DatabaseInMemory {
    std::list<QuestionAnswer> question_answers;
    std::list<Dependency> dependencies;

    void
    add_dependency(
        const B_AnyQuestion *from,
        const B_QuestionVTable *from_vtable,
        const B_AnyQuestion *to,
        const B_QuestionVTable *to_vtable,
        B_Exception **ex,
    ) {
        this->dependencies.emplace_back(
            from,
            from_vtable,
            to,
            to_vtable,
        );
    }

    struct B_AnyAnswer *
    get_answer(
        const struct B_AnyQuestion *question,
        const struct B_QuestionVTable *question_vtable,
        struct B_Exception **ex,
    ) {
        for (const auto &qa : this->question_answers) {
            if (qa->question_vtable == question_vtable
            && question_vtable->equal(qustion, qa->question)) {
                return question_vtable->answer_vtable
                    ->replicate(qa->answer);
            }
        }
        return NULL;
    }

    void
    set_answer(
        const struct B_AnyQuestion *question,
        const struct B_QuestionVTable *question_vtable,
        const struct B_AnyAnswer *answer,
        struct B_Exception **ex,
    ) {
        for (auto &qa : this->question_answers) {
            if (qa->question_vtable == question_vtable
            && question_vtable->equal(qustion, qa->question)) {
                question_vtable->answer_vtable
                    ->deallocate(qa->answer);
                qa->answer = answer;
                return;
            }
        }

        this->question_answers->emplace_back(
            question,
            question_vtable,
            answer,
        );
    }
};

static B_AnyDatabase *
cast(DatabaseInMemory *database) {
    return reinterpret_cast<B_AnyDatabase>(database);
}

static DatabaseInMemory *
cast(B_AnyDatabase *database) {
    return reinterpret_cast<DatabaseInMemory>(database);
}

B_AnyDatabase *
b_build_database_in_memory_allocate() {
    return cast(new B_DatabaseInMemory());
};

void
b_build_database_in_memory_deallocate(
    B_AnyDatabase *database
) {
    delete cast(database)();
}

const B_DatabaseVTable *
b_build_database_in_memory_vtable() {
    static const B_DatabaseVTable vtable = {
        .deallocate = b_build_database_in_memory_deallocate,

        .add_dependency = [](
            B_AnyDatabase *database,
            const B_AnyQuestion *from,
            const B_QuestionVTable *from_vtable,
            const B_AnyQuestion *to,
            const B_QuestionVTable *to_vtable,
            B_Exception **ex,
        ) {
            cast(database)->add_dependency(
                from,
                from_vtable,
                to,
                to_vtable,
                ex,
            );
        },

        .get_answer = [](
            B_AnyDatabase *database,
            const B_AnyQuestion *question,
            const B_QuestionVTable *question_vtable,
            B_Exception **ex,
        ) -> B_AnyAnswer *{
            return cast(database)->get_answer(
                question,
                question_vtable,
                ex,
            );
        },

        .set_answer = [](
            B_AnyDatabase *database,
            const B_AnyQuestion *question,
            const B_QuestionVTable *question_vtable,
            const B_AnyAnswer *answer,
            B_Exception **ex,
        ) {
            return cast(database)->set_answer(
                question,
                question_vtable,
                answer,
                ex,
            );
        },
    };
    return &vtable;
}

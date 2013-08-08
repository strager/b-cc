#include "Answer.h"
#include "Database.h"
#include "DatabaseInMemory.h"
#include "Exception.h"
#include "Question.h"
#include "QuestionVTableList.h"
#include "Serialize.h"

#include <list>
#include <memory>
#include <vector>

// Serialization formats:
//
// DatabaseInMemory:
//
// - size_t question_answer_count;
// - QuestionAnswer question_answers[question_answer_count];
//
// - size_t dependency_count;
// - Dependency dependencies[dependency_count];
//
// QuestionAnswer:
//
// - size_t struct_size;
//
// - UUID question_vtable_uuid;
//
// - size_t question_size;
// - uint8_t question[question_size];
// - size_t answer_size;
// - uint8_t answer[answer_size];
//
// Dependency:
//
// - size_t struct_size;
//
// - UUID from_vtable_uuid;
// - UUID to_vtable_uuid;
//
// - size_t from_size;
// - uint8_t from[from_size];
// - size_t to_size;
// - uint8_t to[to_size];
//
// UUID:
//
// - size_t uuid_size;
// - uint8_t uuid[uuid_size];

static void
serialize_uuid(
    B_UUID uuid,
    B_Serializer s,
    void *c) {
    size_t size = strlen(uuid.uuid);
    b_serialize_size_t(size, s, c);
    s(uuid.uuid, size, c);
}

static bool
deserialize_uuid(
    B_UUID *uuid,
    B_Deserializer s,
    void *c) {
    bool ok;
    size_t size = b_deserialize_size_t(&ok, s, c);
    if (!ok) return false;

    auto buffer = std::unique_ptr<char>(new char[size + 1]);
    size_t read_size = s(buffer.get(), size, c);
    if (read_size != size) {
        return false;
    }
    buffer.get()[size] = '\0';

    *uuid = b_uuid_from_temp_string(buffer.get());
    return true;
}

enum TransferOwnershipTag { TransferOwnership };

struct Question {
    B_AnyQuestion *question;
    const B_QuestionVTable *question_vtable;

    Question(
        const Question &other) :
        Question(other.question, other.question_vtable) {
    }

    Question(
        const B_QuestionVTable *question_vtable) :
        question(nullptr),
        question_vtable(question_vtable) {
    }

    Question(
        const B_AnyQuestion *question,
        const B_QuestionVTable *question_vtable) :
        question(question_vtable->replicate(question)),
        question_vtable(question_vtable) {
    }

    Question(
        B_AnyQuestion *question,
        const B_QuestionVTable *question_vtable,
        TransferOwnershipTag) :
        question(question),
        question_vtable(question_vtable) {
    }

    ~Question() {
        if (this->question) {
            this->question_vtable
                ->deallocate(this->question);
        }
    }

    Question &
    operator=(
        const Question &other) {
        if (this == &other) {
            return *this;
        }
        this->question = other.question_vtable
            ->replicate(other.question);
        this->question_vtable = other.question_vtable;
        return *this;
    }

    const B_AnswerVTable *
    answer_vtable() const {
        return this->question_vtable->answer_vtable;
    }
};

struct QuestionAnswer {
    Question question;
    B_AnyAnswer *answer;

    QuestionAnswer(
        const QuestionAnswer &other) :
        QuestionAnswer(
            other.question,
            other.answer) {
    }

    QuestionAnswer(
        const B_QuestionVTable *question_vtable) :
        question(question_vtable),
        answer(nullptr) {
    }

    QuestionAnswer(
        const Question &question,
        const B_AnyAnswer *answer) :
        question(question),
        answer(question.answer_vtable()->replicate(answer)) {
    }

    QuestionAnswer(
        const B_AnyQuestion *question,
        const B_QuestionVTable *question_vtable,
        const B_AnyAnswer *answer) :
        QuestionAnswer(
            Question(question, question_vtable),
            answer) {
    }

    QuestionAnswer(
        B_AnyQuestion *question,
        const B_QuestionVTable *question_vtable,
        B_AnyAnswer *answer,
        TransferOwnershipTag) :
        question(question, question_vtable, TransferOwnership),
        answer(answer) {
    }

    QuestionAnswer(
        Question &&question,
        B_AnyAnswer *answer,
        TransferOwnershipTag) :
        question(std::move(question)),
        answer(answer) {
    }

    ~QuestionAnswer() {
        if (this->answer) {
            this->question.answer_vtable()
                ->deallocate(this->answer);
        }
    }

    QuestionAnswer &
    operator=(
        const QuestionAnswer &other) {
        if (this == &other) {
            return *this;
        }
        this->question = other.question;
        this->answer = this->question.answer_vtable()
            ->replicate(other.answer);
        return *this;
    }

    void
    serialize(
        B_Serializer s,
        void *c) const {
        serialize_uuid(
            this->question.question_vtable->uuid,
            s,
            c);
        b_serialize_sized(
            this->question.question,
            b_serialize_size_t,
            this->question.question_vtable->serialize,
            s,
            c);
        b_serialize_sized(
            this->answer,
            b_serialize_size_t,
            this->question.answer_vtable()->serialize,
            s,
            c);
    }

    static std::unique_ptr<QuestionAnswer>
    deserialize(
        const B_QuestionVTableList *question_vtables,
        B_Deserializer s,
        void *c) {
        B_UUID uuid;
        bool ok;
        ok = deserialize_uuid(&uuid, s, c);
        if (!ok) return nullptr;

        const B_QuestionVTable *question_vtable
            = b_question_vtable_list_find_by_uuid(
                question_vtables,
                uuid);
        if (!question_vtable) return nullptr;

        Question question(
            static_cast<B_AnyQuestion *>(b_deserialize_sized0(
                b_deserialize_size_t,
                question_vtable->deserialize,
                s,
                c)),
            question_vtable,
            TransferOwnership);
        if (!question.question) return nullptr;

        B_AnyAnswer *answer
            = static_cast<B_AnyAnswer *>(b_deserialize_sized0(
                b_deserialize_size_t,
                question_vtable->answer_vtable->deserialize,
                s,
                c));
        if (!answer) return nullptr;

        return std::unique_ptr<QuestionAnswer>(
            new QuestionAnswer(
                std::move(question),
                answer,
                TransferOwnership));
    }
};

struct Dependency {
    Question from;
    Question to;

    Dependency(
        const Dependency &other) :
        Dependency(other.from, other.to) {
    }

    Dependency(
        const B_QuestionVTable *from_vtable,
        const B_QuestionVTable *to_vtable) :
        from(from_vtable),
        to(to_vtable) {
    }

    Dependency(
        Question from,
        Question to) :
        from(from),
        to(to) {
    }

    Dependency(
        Question &&from,
        Question &&to) :
        from(std::move(from)),
        to(std::move(to)) {
    }

    Dependency(
        const B_AnyQuestion *from,
        const B_QuestionVTable *from_vtable,
        const B_AnyQuestion *to,
        const B_QuestionVTable *to_vtable) :
        from(from, from_vtable),
        to(to, to_vtable) {
    }

    Dependency &
    operator=(const Dependency &other) {
        if (this == &other) {
            return *this;
        }
        this->from = other.from;
        this->to = other.from;
        return *this;
    }

    const B_QuestionVTable *
    from_vtable() const {
        return this->from.question_vtable;
    }

    const B_QuestionVTable *
    to_vtable() const {
        return this->to.question_vtable;
    }

    void
    serialize(
        B_Serializer s,
        void *c) const {
        serialize_uuid(
            this->from_vtable()->uuid,
            s,
            c);
        serialize_uuid(
            this->to_vtable()->uuid,
            s,
            c);
        b_serialize_sized(
            this->from.question,
            b_serialize_size_t,
            this->from_vtable()->serialize,
            s,
            c);
        b_serialize_sized(
            this->to.question,
            b_serialize_size_t,
            this->to_vtable()->serialize,
            s,
            c);
    }

    static std::unique_ptr<Dependency>
    deserialize(
        const B_QuestionVTableList *question_vtables,
        B_Deserializer s,
        void *c) {
        bool ok;

        B_UUID from_uuid;
        ok = deserialize_uuid(&from_uuid, s, c);
        if (!ok) return nullptr;

        const B_QuestionVTable *from_vtable
            = b_question_vtable_list_find_by_uuid(
                question_vtables,
                from_uuid);
        if (!from_vtable) return nullptr;

        B_UUID to_uuid;
        ok = deserialize_uuid(&to_uuid, s, c);
        if (!ok) return nullptr;

        const B_QuestionVTable *to_vtable
            = b_question_vtable_list_find_by_uuid(
                question_vtables,
                to_uuid);
        if (!to_vtable) return nullptr;

        Question from(
            static_cast<B_AnyQuestion *>(b_deserialize_sized0(
                b_deserialize_size_t,
                from_vtable->deserialize,
                s,
                c)),
            from_vtable,
            TransferOwnership);
        if (!from.question) return nullptr;

        Question to(
            static_cast<B_AnyQuestion *>(b_deserialize_sized0(
                b_deserialize_size_t,
                to_vtable->deserialize,
                s,
                c)),
            to_vtable,
            TransferOwnership);
        if (!to.question) return nullptr;

        return std::unique_ptr<Dependency>(
            new Dependency(from, to));
    }

};

template<typename T>
std::unique_ptr<T>
deserialize_sized_with_question_vtables(
    const B_QuestionVTableList *question_vtables,
    B_Deserializer s,
    void *c) {
    return b_deserialize_sized(
        b_deserialize_size_t,
        [question_vtables](
            B_Deserializer s,
            void *c) {
            return T::deserialize(question_vtables, s, c);
        },
        s,
        c);
}

struct DatabaseInMemory {
    std::list<QuestionAnswer> question_answers;
    std::list<Dependency> dependencies;

    void
    add_dependency(
        const B_AnyQuestion *from,
        const B_QuestionVTable *from_vtable,
        const B_AnyQuestion *to,
        const B_QuestionVTable *to_vtable,
        B_Exception **ex) {
        this->dependencies.emplace_back(
            from,
            from_vtable,
            to,
            to_vtable);
    }

    std::list<QuestionAnswer>::iterator
    find_question(
        const struct B_AnyQuestion *question,
        const struct B_QuestionVTable *question_vtable) {
        auto &qas = this->question_answers;
        return std::find_if(
            qas.begin(),
            qas.end(),
            [question, question_vtable](const QuestionAnswer &qa) {
                return qa.question.question_vtable == question_vtable
                    && question_vtable->equal(question, qa.question.question);
            }
        );
    }

    struct B_AnyAnswer *
    get_answer(
        const struct B_AnyQuestion *question,
        const struct B_QuestionVTable *question_vtable,
        struct B_Exception **ex) {
        auto i = find_question(question, question_vtable);
        if (i == this->question_answers.end()) {
            return nullptr;
        } else {
            return question_vtable->answer_vtable
                ->replicate(i->answer);
        }
    }

    void
    set_answer(
        const struct B_AnyQuestion *question,
        const struct B_QuestionVTable *question_vtable,
        const struct B_AnyAnswer *answer,
        struct B_Exception **ex) {
        auto i = find_question(question, question_vtable);
        if (i == this->question_answers.end()) {
            this->question_answers.emplace_back(
                question,
                question_vtable,
                answer);
        } else {
            question_vtable->answer_vtable
                ->deallocate(i->answer);
            i->answer = question_vtable->answer_vtable
                ->replicate(answer);
        }
    }

    void
    recheck_all() {
        std::vector<Question> dirty_questions;
        for (const auto &qa : this->question_answers) {
            // TODO Exception safety.
            // Ensure the question's answer is up to date.
            // If not, dirty it.
            auto q_vtable = qa.question.question_vtable;
            auto a_vtable = q_vtable->answer_vtable;

            struct B_Exception *ex = nullptr;
            B_AnyAnswer *answer = q_vtable->answer(
                qa.question.question,
                &ex);
            if (ex || !a_vtable->equal(answer, qa.answer)) {
                dirty_questions.emplace_back(qa.question);
            }
            if (ex) {
                b_exception_deallocate(ex);
            }
            a_vtable->deallocate(answer);
        }

        for (const auto &question : dirty_questions) {
            this->dirty(question);
        }
    }

    void
    dirty(const Question &q) {
        auto question = q.question;
        auto question_vtable = q.question_vtable;

        std::list<Dependency> new_dependencies;
        std::vector<Question> dependants;
        for (const auto &dep : this->dependencies) {
            if (question_vtable == dep.to_vtable()
            && question_vtable->equal(question, dep.to.question)) {
                dependants.emplace_back(dep.from);
            } else {
                // TODO Move and stuff.
                new_dependencies.emplace_back(dep);
            }
        }

        std::list<QuestionAnswer> new_question_answers;
        for (const auto &qa : this->question_answers) {
            if (question_vtable == qa.question.question_vtable
                && question_vtable->equal(question, qa.question.question)) {
                // Match; remove.
            } else {
                // TODO Move.
                new_question_answers.emplace_back(qa);
            }
        }

        this->dependencies = std::move(new_dependencies);
        this->question_answers = std::move(new_question_answers);

        for (const auto &q : dependants) {
            this->dirty(q);
        }
    }

    void
    serialize(
        B_Serializer s,
        void *c) const {
        b_serialize_size_t(this->question_answers.size(), s, c);
        for (const QuestionAnswer &qa : this->question_answers) {
            b_serialize_sized(qa, b_serialize_size_t, s, c);
        }

        b_serialize_size_t(this->dependencies.size(), s, c);
        for (const Dependency &dep : this->dependencies) {
            b_serialize_sized(dep, b_serialize_size_t, s, c);
        }
    }

    static std::unique_ptr<DatabaseInMemory>
    deserialize(
        const B_QuestionVTableList *question_vtables,
        B_Deserializer s,
        void *c) {
        bool ok;
        auto db = std::unique_ptr<DatabaseInMemory>(
            new DatabaseInMemory());

        size_t qa_count = b_deserialize_size_t(&ok, s, c);
        if (!ok) return nullptr;
        for (size_t i = 0; i < qa_count; ++i) {
            auto qa = deserialize_sized_with_question_vtables<QuestionAnswer>(
                question_vtables,
                s,
                c);
            if (!qa) return nullptr;
            // TODO Move.
            db->question_answers.push_back(*qa);
        }

        size_t dep_count = b_deserialize_size_t(&ok, s, c);
        if (!ok) return nullptr;
        for (size_t i = 0; i < dep_count; ++i) {
            auto dep = deserialize_sized_with_question_vtables<Dependency>(
                question_vtables,
                s,
                c);
            if (!dep) return nullptr;
            // TODO Move.
            db->dependencies.emplace_back(*dep);
        }

        return db;
    }
};

static B_AnyDatabase *
cast(DatabaseInMemory *database) {
    return reinterpret_cast<B_AnyDatabase *>(database);
}

static DatabaseInMemory *
cast(B_AnyDatabase *database) {
    return reinterpret_cast<DatabaseInMemory *>(database);
}

B_AnyDatabase *
b_database_in_memory_allocate() {
    return cast(new DatabaseInMemory());
};

void
b_database_in_memory_deallocate(
    B_AnyDatabase *database) {
    delete cast(database);
}

const B_DatabaseVTable *
b_database_in_memory_vtable() {
    static const B_DatabaseVTable vtable = {
        .deallocate = b_database_in_memory_deallocate,

        .add_dependency = [](
            B_AnyDatabase *database,
            const B_AnyQuestion *from,
            const B_QuestionVTable *from_vtable,
            const B_AnyQuestion *to,
            const B_QuestionVTable *to_vtable,
            B_Exception **ex) {
            cast(database)->add_dependency(
                from,
                from_vtable,
                to,
                to_vtable,
                ex);
        },

        .get_answer = [](
            B_AnyDatabase *database,
            const B_AnyQuestion *question,
            const B_QuestionVTable *question_vtable,
            B_Exception **ex) -> B_AnyAnswer *{
            return cast(database)->get_answer(
                question,
                question_vtable,
                ex);
        },

        .set_answer = [](
            B_AnyDatabase *database,
            const B_AnyQuestion *question,
            const B_QuestionVTable *question_vtable,
            const B_AnyAnswer *answer,
            B_Exception **ex) {
            return cast(database)->set_answer(
                question,
                question_vtable,
                answer,
                ex);
        },

        .recheck_all = [](
            B_AnyDatabase *database,
            B_Exception **ex) {
            (void) ex;
            cast(database)->recheck_all();
        }
    };
    return &vtable;
}

void
b_database_in_memory_serialize(
    const void *database,
    B_Serializer s,
    void *c) {
    b_serialize<DatabaseInMemory>(database, s, c);
}

void *
b_database_in_memory_deserialize(
    const B_QuestionVTableList *question_vtables,
    B_Deserializer s,
    void *c) {
    return DatabaseInMemory::deserialize(
        question_vtables,
        s,
        c).release();
}

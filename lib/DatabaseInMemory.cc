#include "Answer.h"
#include "Database.h"
#include "DatabaseInMemory.h"
#include "Question.h"
#include "Serialize.h"

#include <algorithm>
#include <list>
#include <map>
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

struct Question {
    B_AnyQuestion *question;
    const B_QuestionVTable *question_vtable;

    Question(
        const Question &other) :
        Question(
            other.question,
            other.question_vtable) {
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

    ~Question() {
        if (this->question) {
            this->question_vtable
                ->deallocate(this->question);
        }
    }

    Question &
    operator=(const Question &other) = delete;
};

struct QuestionAnswer {
    B_AnyQuestion *question;
    const B_QuestionVTable *question_vtable;
    B_AnyAnswer *answer;

    QuestionAnswer(
        const QuestionAnswer &other) :
        QuestionAnswer(
            other.question,
            other.question_vtable,
            other.answer) {
    }

    QuestionAnswer(
        const B_QuestionVTable *question_vtable) :
        question(nullptr),
        question_vtable(question_vtable),
        answer(nullptr) {
    }

    QuestionAnswer(
        const B_AnyQuestion *question,
        const B_QuestionVTable *question_vtable,
        const B_AnyAnswer *answer) :
        question(question_vtable->replicate(question)),
        question_vtable(question_vtable),
        answer(question_vtable->answer_vtable->replicate(answer)) {
    }

    ~QuestionAnswer() {
        if (this->question) {
            this->question_vtable
                ->deallocate(this->question);
        }
        if (this->answer) {
            this->question_vtable->answer_vtable
                ->deallocate(this->answer);
        }
    }

    QuestionAnswer &
    operator=(const QuestionAnswer &other) = delete;

    void
    serialize(
        B_Serializer s,
        void *c) const {
        serialize_uuid(
            this->question_vtable->uuid,
            s,
            c);
        b_serialize_sized(
            this->question,
            b_serialize_size_t,
            this->question_vtable->serialize,
            s,
            c);
        b_serialize_sized(
            this->answer,
            b_serialize_size_t,
            this->question_vtable->answer_vtable->serialize,
            s,
            c);
    }
};

struct Dependency {
    B_AnyQuestion *from;
    const B_QuestionVTable *from_vtable;
    B_AnyQuestion *to;
    const B_QuestionVTable *to_vtable;

    Dependency(
        const Dependency &other) :
        Dependency(
            other.from,
            other.from_vtable,
            other.to,
            other.to_vtable) {
    }

    Dependency(
        const B_QuestionVTable *from_vtable,
        const B_QuestionVTable *to_vtable) :
        from(nullptr),
        from_vtable(from_vtable),
        to(nullptr),
        to_vtable(to_vtable) {
    }

    Dependency(
        const B_AnyQuestion *from,
        const B_QuestionVTable *from_vtable,
        const B_AnyQuestion *to,
        const B_QuestionVTable *to_vtable) :
        from(from_vtable->replicate(from)),
        from_vtable(from_vtable),
        to(to_vtable->replicate(to)),
        to_vtable(to_vtable) {
    }

    ~Dependency() {
        if (this->from) {
            this->from_vtable->deallocate(this->from);
        }
        if (this->to) {
            this->to_vtable->deallocate(this->to);
        }
    }

    Dependency &
    operator=(const Dependency &other) = delete;

    void
    serialize(
        B_Serializer s,
        void *c) const {
        serialize_uuid(
            this->from_vtable->uuid,
            s,
            c);
        serialize_uuid(
            this->to_vtable->uuid,
            s,
            c);
        b_serialize_sized(
            this->from,
            b_serialize_size_t,
            this->from_vtable->serialize,
            s,
            c);
        b_serialize_sized(
            this->to,
            b_serialize_size_t,
            this->to_vtable->serialize,
            s,
            c);
    }
};

struct UnresolvedQuestionAnswer {
    B_UUID question_vtable_uuid;

    const char *question_data;
    size_t question_data_size;

    const char *answer_data;
    size_t answer_data_size;

    static std::unique_ptr<UnresolvedQuestionAnswer>
    deserialize(
        B_Deserializer s,
        void *c) {
        auto qa = std::unique_ptr<UnresolvedQuestionAnswer>(
            new UnresolvedQuestionAnswer());

        bool ok;
        ok = deserialize_uuid(
            &qa->question_vtable_uuid,
            s,
            c);
        if (!ok) return nullptr;

        qa->question_data = static_cast<const char *>(
            b_deserialize_sized_blob(
                &qa->question_data_size,
                b_deserialize_size_t,
                s,
                c));
        if (!qa->question_data) return nullptr;

        qa->answer_data = static_cast<const char *>(
            b_deserialize_sized_blob(
                &qa->answer_data_size,
                b_deserialize_size_t,
                s,
                c));
        if (!qa->answer_data) return nullptr;

        return qa;
    }

    QuestionAnswer
    resolve(
        const B_QuestionVTable *question_vtable) const {
        QuestionAnswer qa(question_vtable);
        qa.question = static_cast<B_AnyQuestion *>(
            b_deserialize_from_memory(
                this->question_data,
                this->question_data_size,
                question_vtable->deserialize));
        qa.answer = static_cast<B_AnyAnswer *>(
            b_deserialize_from_memory(
                this->answer_data,
                this->answer_data_size,
                question_vtable->answer_vtable->deserialize));
        return qa;
    }
};

struct UnresolvedDependency {
    B_UUID from_vtable_uuid;
    const char *from_data;
    size_t from_data_size;

    B_UUID to_vtable_uuid;
    const char *to_data;
    size_t to_data_size;

    static std::unique_ptr<UnresolvedDependency>
    deserialize(
        B_Deserializer s,
        void *c) {
        auto dep = std::unique_ptr<UnresolvedDependency>(
            new UnresolvedDependency());

        bool ok;

        ok = deserialize_uuid(
            &dep->from_vtable_uuid,
            s,
            c);
        if (!ok) return nullptr;

        ok = deserialize_uuid(
            &dep->to_vtable_uuid,
            s,
            c);
        if (!ok) return nullptr;

        dep->from_data = static_cast<const char *>(
            b_deserialize_sized_blob(
                &dep->from_data_size,
                b_deserialize_size_t,
                s,
                c));
        if (!dep->from_data) return nullptr;

        dep->to_data = static_cast<const char *>(
            b_deserialize_sized_blob(
                &dep->to_data_size,
                b_deserialize_size_t,
                s,
                c));
        if (!dep->to_data) return nullptr;

        return dep;
    }

    Dependency
    resolve(
        const B_QuestionVTable *from_vtable,
        const B_QuestionVTable *to_vtable) const {
        Dependency dep(from_vtable, to_vtable);
        dep.from = static_cast<B_AnyQuestion *>(
            b_deserialize_from_memory(
                this->from_data,
                this->from_data_size,
                from_vtable->deserialize));
        dep.to = static_cast<B_AnyQuestion *>(
            b_deserialize_from_memory(
                this->to_data,
                this->to_data_size,
                to_vtable->deserialize));
        return dep;
    }
};

struct DatabaseInMemory {
    std::list<QuestionAnswer> question_answers;
    std::list<Dependency> dependencies;

    std::map<B_UUID, const B_QuestionVTable *> question_vtables;

    std::multimap<B_UUID, UnresolvedQuestionAnswer> unresolved_question_answers;
    std::multimap<B_UUID, std::shared_ptr<UnresolvedDependency>> unresolved_dependencies;

    void
    resolve(
        const B_QuestionVTable *vtable) {
        B_UUID uuid = vtable->uuid;
        auto &vtables = this->question_vtables;
        if (vtables.find(uuid) != vtables.end()) {
            // Already resolved.
            return;
        }

        vtables[uuid] = vtable;
        resolve_question_answers(vtable);
        resolve_dependencies(vtable);
    }

    void
    resolve_question_answers(
        const B_QuestionVTable *vtable) {
        auto &qas = this->unresolved_question_answers;
        auto p = qas.equal_range(vtable->uuid);
        auto begin = p.first;
        auto end = p.second;
        for (auto i = begin; i != end; ++i) {
            this->question_answers.push_back(
                i->second.resolve(vtable));
        }
        qas.erase(begin, end);
    }

    void
    resolve_dependencies(
        const B_QuestionVTable *vtable) {
        auto &vtables = this->question_vtables;
        auto &deps = this->unresolved_dependencies;

        auto p = deps.equal_range(vtable->uuid);
        auto begin = p.first;
        auto end = p.second;
        for (auto i = begin; i != end; ++i) {
            const auto &dep = *i->second;

            auto from_entry = vtables.find(dep.from_vtable_uuid);
            if (from_entry == vtables.end()) {
                continue;
            }
            auto to_entry = vtables.find(dep.to_vtable_uuid);
            if (to_entry == vtables.end()) {
                continue;
            }

            this->dependencies.push_back(dep.resolve(
                from_entry->second,
                to_entry->second));

            // TODO Erase dependency from
            // this->unresolved_dependencies.
        }
    }

    void
    add_dependency(
        const B_AnyQuestion *from,
        const B_QuestionVTable *from_vtable,
        const B_AnyQuestion *to,
        const B_QuestionVTable *to_vtable,
        B_Exception **ex) {
        this->resolve(from_vtable);
        this->resolve(to_vtable);
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
        this->resolve(question_vtable);

        auto &qas = this->question_answers;
        return std::find_if(
            qas.begin(),
            qas.end(),
            [question, question_vtable](const QuestionAnswer &qa) {
                return qa.question_vtable == question_vtable
                    && question_vtable->equal(question, qa.question);
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
        // FIXME Does not work for unresolved.
        std::vector<Question> dirty_questions;
        for (const auto &qa : this->question_answers) {
            // TODO Exception safety.
            // Ensure the question's answer is up to date.
            // If not, dirty it.
            auto q_vtable = qa.question_vtable;
            auto a_vtable = q_vtable->answer_vtable;

            struct B_Exception *ex = nullptr;
            B_AnyAnswer *answer = q_vtable->answer(qa.question, &ex);
            if (ex || !a_vtable->equal(answer, qa.answer)) {
                dirty_questions.emplace_back(
                    qa.question,
                    qa.question_vtable);
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
            if (question_vtable == dep.to_vtable
            && question_vtable->equal(question, dep.to)) {
                dependants.emplace_back(
                    dep.from,
                    dep.from_vtable);
            } else {
                // TODO Move and stuff.
                new_dependencies.emplace_back(dep);
            }
        }

        std::list<QuestionAnswer> new_question_answers;
        for (const auto &qa : this->question_answers) {
            if (question_vtable == qa.question_vtable
                && question_vtable->equal(question, qa.question)) {
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
        // TODO Serialize unresolved question-answers.

        b_serialize_size_t(this->dependencies.size(), s, c);
        for (const Dependency &dep : this->dependencies) {
            b_serialize_sized(dep, b_serialize_size_t, s, c);
        }
        // TODO Serialize unresolved dependencies.
    }

    static std::unique_ptr<DatabaseInMemory>
    deserialize(
        B_Deserializer s,
        void *c) {
        bool ok;
        auto db = std::unique_ptr<DatabaseInMemory>(
            new DatabaseInMemory());

        size_t qa_count = b_deserialize_size_t(&ok, s, c);
        if (!ok) return nullptr;
        for (size_t i = 0; i < qa_count; ++i) {
            std::unique_ptr<UnresolvedQuestionAnswer> qa
                = b_deserialize_sized<UnresolvedQuestionAnswer>(
                        b_deserialize_size_t,
                        s,
                        c);
            if (!qa) return nullptr;
            db->unresolved_question_answers.emplace(
                qa->question_vtable_uuid,
                *qa);
        }

        size_t dep_count = b_deserialize_size_t(&ok, s, c);
        if (!ok) return nullptr;
        for (size_t i = 0; i < dep_count; ++i) {
            auto dep = std::shared_ptr<UnresolvedDependency>(
                b_deserialize_sized<UnresolvedDependency>(
                    b_deserialize_size_t,
                    s,
                    c).release());
            if (!dep) return nullptr;
            db->unresolved_dependencies.emplace(
                dep->from_vtable_uuid,
                dep);
            db->unresolved_dependencies.emplace(
                dep->to_vtable_uuid,
                dep);
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
    B_AnyDatabase *database
) {
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
    B_Deserializer s,
    void *c) {
    return b_deserialize<DatabaseInMemory>(s, c).release();
}

void
b_database_in_memory_resolve(
    struct B_AnyDatabase *database,
    const B_QuestionVTable *question_vtable) {
    cast(database)->resolve(question_vtable);
}

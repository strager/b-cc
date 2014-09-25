// TODO(strager): Use b_new, b_free, etc. for containers.

#include <B/Alloc.h>
#include <B/Assert.h>
#include <B/Deserialize.h>
#include <B/PBX/PBXParser.h>
#include <B/PBX/PBXProjectFile.h>
#include <B/Private/Misc.h>
#include <B/QuestionAnswer.h>
#include <B/Serialize.h>

#include <cstring>
#include <errno.h>
#include <map>
#include <string>

static bool
operator==(
        B_PBXValueRange const &a,
        B_PBXValueRange const &b) {
    return a.start == b.start && a.end == b.end;
}

typedef std::string PBXObjectID_;

class PBXProjectFileAnswer_ :
        public B_AnswerClass<PBXProjectFileAnswer_> {
public:
    explicit
    PBXProjectFileAnswer_() {
    }

    PBXProjectFileAnswer_(
            uint32_t archive_version,
            uint32_t object_version,
            PBXObjectID_ &&root,
            std::map<
                PBXObjectID_,
                B_PBXValueRange> &&objects) :
            archive_version(archive_version),
            object_version(object_version),
            root(root),
            objects(objects) {
    }

    static B_FUNC
    equal(
            PBXProjectFileAnswer_ const *a,
            PBXProjectFileAnswer_ const *b,
            B_OUTPTR bool *out,
            B_ErrorHandler const *eh) {
        (void) eh;  // TODO(strager)
        *out = *a == *b;
        return true;
    }

    static B_FUNC
    replicate(
            PBXProjectFileAnswer_ const *self,
            B_OUTPTR PBXProjectFileAnswer_ **out,
            B_ErrorHandler const *eh) {
        (void) eh;  // TODO(strager)
        *out = new PBXProjectFileAnswer_(*self);
        return true;
    }

    static B_FUNC
    deallocate(
            PBXProjectFileAnswer_ *self,
            B_ErrorHandler const *eh) {
        return b_delete(self, eh);
    }

    static B_FUNC
    serialize(
            PBXProjectFileAnswer_ const *self,
            B_ByteSink *sink,
            B_ErrorHandler const *eh) {
        B_CHECK_PRECONDITION(eh, sink);

        // TODO(strager): Version field.

        if (!b_serialize_4_be(
                sink,
                self->archive_version,
                eh)) {
            return false;
        }
        if (!b_serialize_4_be(
                sink,
                self->object_version,
                eh)) {
            return false;
        }
        if (!b_serialize_data_and_size_8_be(
                sink,
                self->root,
                eh)) {
            return false;
        }

        if (!b_serialize_8_be(
                sink,
                self->objects.size(),
                eh)) {
            return false;
        }
        for (auto const &kvp : self->objects) {
            if (!b_serialize_data_and_size_8_be(
                    sink,
                    kvp.first,
                    eh)) {
                return false;
            }
            if (!b_serialize_8_be(
                    sink,
                    kvp.second.start,
                    eh)) {
                return false;
            }
            if (!b_serialize_8_be(
                    sink,
                    kvp.second.end,
                    eh)) {
                return false;
            }
        }

        return true;
    }

    static B_FUNC
    deserialize(
            B_ByteSource *source,
            B_OUTPTR PBXProjectFileAnswer_ **out,
            B_ErrorHandler const *eh) {
        B_CHECK_PRECONDITION(eh, source);
        B_CHECK_PRECONDITION(eh, out);

        uint32_t archive_version;
        if (!b_deserialize_4_be(
                source,
                &archive_version,
                eh)) {
            return false;
        }

        uint32_t object_version;
        if (!b_deserialize_4_be(
                source,
                &object_version,
                eh)) {
            return false;
        }

        PBXObjectID_ root;
        if (!b_deserialize_data_and_size_8_be(
                source,
                &root,
                eh)) {
            return false;
        }

        uint64_t objects_count;
        if (!b_deserialize_8_be(
                source,
                &objects_count,
                eh)) {
            return false;
        }

        std::map<PBXObjectID_, B_PBXValueRange> objects;
        for (uint64_t i = 0; i < objects_count; ++i) {
            PBXObjectID_ object_id;
            if (!b_deserialize_data_and_size_8_be(
                    source,
                    &object_id,
                    eh)) {
                return false;
            }

            uint64_t start;
            if (!b_deserialize_8_be(source, &start, eh)) {
                return false;
            }
            uint64_t end;
            if (!b_deserialize_8_be(source, &end, eh)) {
                return false;
            }

            objects.emplace(
                std::move(object_id),
                B_PBXValueRange{start, end});
        }

        // TODO(strager): Raise if not EOF.

        PBXProjectFileAnswer_ *answer;
        if (!b_new(
                &answer,
                eh,
                archive_version,
                object_version,
                std::move(root),
                std::move(objects))) {
            return false;
        }

        *out = answer;
        return true;
    }

    bool
    operator==(
            PBXProjectFileAnswer_ const &other) const {
        return other.archive_version == this->archive_version
            && other.object_version == this->object_version
            && other.root == this->root
            && other.objects == this->objects;
    }

    static B_FUNC
    new_from_path(
            B_FilePath const *path,
            B_OUTPTR PBXProjectFileAnswer_ **out,
            B_ErrorHandler const *eh) {
        B_CHECK_PRECONDITION(eh, path);
        B_CHECK_PRECONDITION(eh, out);

        bool ok;
        void *data = NULL;
        size_t data_size;

        B_PBXHeader header;
        std::map<PBXObjectID_, B_PBXValueRange> objects;

        auto read_object_id = [](
                B_PBXObjectID object_id) {
            return std::string(
                reinterpret_cast<char const *>(
                    object_id.utf8),
                object_id.size);
        };

        if (!b_map_file_by_path(
                path,
                &data,
                &data_size,
                eh)) {
            goto fail;
        }

        if (!b_pbx_parse(
                reinterpret_cast<uint8_t const *>(data),
                data_size,
                &header,
                [&objects, read_object_id](
                        B_PBXObjectID object_id,
                        B_PBXValueRange value_range,
                        B_ErrorHandler const *) {
                    objects.emplace(
                        read_object_id(object_id),
                        value_range);
                    return true;
                },
                eh)) {
            goto fail;
        }

        if (!b_new(
                out,
                eh,
                header.archive_version,
                header.object_version,
                read_object_id(header.root_object_id),
                std::move(objects))) {
            goto fail;
        }

        ok = true;
done:
        if (data != NULL) {
            (void) b_unmap_file(data, data_size, eh);
        }
        return ok;

fail:
        ok = false;
        goto done;
    }

    uint32_t archive_version;
    uint32_t object_version;
    PBXObjectID_ root;
    std::map<PBXObjectID_, B_PBXValueRange> objects;
};

struct PBXProjectFileQuestion_ :
        public B_QuestionClass<
            PBXProjectFileQuestion_,
            B_FilePath> {
    typedef PBXProjectFileAnswer_ AnswerClass;

    PBXProjectFileQuestion_() = delete;
    PBXProjectFileQuestion_(
            PBXProjectFileQuestion_ const &) = delete;
    PBXProjectFileQuestion_(
            PBXProjectFileQuestion_ &&) = delete;
    PBXProjectFileQuestion_ &
    operator=(
            PBXProjectFileQuestion_ const &) = delete;
    PBXProjectFileQuestion_ &
    operator=(
            PBXProjectFileQuestion_ &&) = delete;

    static B_FUNC
    answer(
            B_FilePath const *path,
            B_OUTPTR PBXProjectFileAnswer_ **out,
            B_ErrorHandler const *eh) {
        B_CHECK_PRECONDITION(eh, path);
        B_CHECK_PRECONDITION(eh, out);

        return PBXProjectFileAnswer_::new_from_path(
            path,
            out,
            eh);
    }

    static B_FUNC
    equal(
            B_FilePath const *a,
            B_FilePath const *b,
            B_OUTPTR bool *out,
            B_ErrorHandler const *eh) {
        return b_file_path_equal(a, b, out, eh);
    }

    static B_FUNC
    replicate(
            B_FilePath const *path,
            B_OUTPTR B_FilePath **out,
            B_ErrorHandler const *eh) {
        B_CHECK_PRECONDITION(eh, out);

        char *string;
        if (!b_strdup(path, &string, eh)) {
            return false;
        }
        *out = string;
        return true;
    }

    static B_FUNC
    deallocate(
            B_TRANSFER B_FilePath *path,
            B_ErrorHandler const *eh) {
        return b_deallocate(path, eh);
    }

    static B_FUNC
    serialize(
            B_FilePath const *path,
            B_ByteSink *sink,
            B_ErrorHandler const *eh) {
        // FIXME(strager): This is duplicated in File.cc.

        B_CHECK_PRECONDITION(eh, sink);

        return b_serialize_data_and_size_8_be(
            sink,
            reinterpret_cast<uint8_t const *>(path),
            strlen(path),
            eh);
    }

    static B_FUNC
    deserialize(
            B_ByteSource *source,
            B_OUTPTR B_FilePath **out,
            B_ErrorHandler const *eh) {
        // FIXME(strager): This is duplicated in File.cc.

        B_CHECK_PRECONDITION(eh, source);
        B_CHECK_PRECONDITION(eh, out);

        uint8_t *data;
        size_t data_size;
        if (!b_deserialize_data_and_size_8_be(
                source,
                &data,
                &data_size,
                eh)) {
            return false;
        }

        char *path;
        if (!b_strndup(
                reinterpret_cast<char const *>(data),
                data_size,
                &path,
                eh)) {
            (void) b_deallocate(data, eh);
            return false;
        }

        (void) b_deallocate(data, eh);

        *out = path;
        return true;
    }
};

template<>
B_UUID
B_QuestionClass<PBXProjectFileQuestion_, B_FilePath>::uuid
    = B_UUID_LITERAL("57E7AD72-6D7A-420A-9EA6-09292A29F290");

struct B_QuestionVTable const *
b_pbx_project_file_question_vtable(
        void) {
    return PBXProjectFileQuestion_::vtable();
}

static B_PBXObjectID
string_object_id_(
        B_BORROWED std::string const &s) {
    return B_PBXObjectID{
        reinterpret_cast<uint8_t const *>(s.c_str()),
        s.size()};
}

B_EXPORT_FUNC
b_pbx_project_file_answer(
        struct B_Answer const *answer,
        B_OUT struct B_PBXHeader *header,
        B_PBXObjectRangeCallback *callback,
        void *callback_opaque,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, answer);
    B_CHECK_PRECONDITION(eh, header);
    B_CHECK_PRECONDITION(eh, callback);

    auto a = static_cast<PBXProjectFileAnswer_ const *>(
        answer);
    header->archive_version = a->archive_version;
    header->object_version = a->object_version;
    header->root_object_id = string_object_id_(a->root);

    for (auto const &kvp : a->objects) {
        if (!callback(
                string_object_id_(kvp.first),
                kvp.second,
                callback_opaque,
                eh)) {
            return false;
        }
    }

    return true;
}

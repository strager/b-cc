#include <B/Assert.h>
#include <B/Error.h>
#include <B/Log.h>
#include <B/PBX/PBXParser.h>
#include <B/PBX/PBXScanner.h>

#include <errno.h>
#include <string.h>

class HeaderAndObjectsVisitor_ :
        public B_PBXValueVisitorClass<
            HeaderAndObjectsVisitor_> {
public:
    HeaderAndObjectsVisitor_(
            B_OUT B_PBXHeader *header,
            B_PBXObjectRangeCallback *callback,
            void *callback_opaque) :
            ancestry(Ancestry::EMPTY),
            dict_key(DictKey::NONE),
            header(header),
            callback(callback),
            callback_opaque(callback_opaque) {
    }

    B_FUNC
    visit_string(
            B_PBXScanner *,
            uint8_t const *data_utf8,
            size_t data_size,
            B_ErrorHandler const *eh) {
        B_LOG(
            B_DEBUG,
            "visit_string(%.*s)",
            (int) data_size,
            data_utf8);

        switch (this->ancestry) {
        case Ancestry::EMPTY:
        case Ancestry::TOP_CLASSES:
        case Ancestry::TOP_OBJECTS:
            return unexpected(eh);

        case Ancestry::TOP:
            switch (this->dict_key) {
            case DictKey::ANY_ID:
            case DictKey::CLASSES:
            case DictKey::NONE:
            case DictKey::OBJECTS:
                return unexpected(eh);

            case DictKey::ARCHIVE_VERSION:
                if (!string_as_int(
                        data_utf8,
                        data_size,
                        &this->header->archive_version,
                        eh)) {
                    return false;
                }
                this->dict_key = DictKey::NONE;
                this->did_encounter_archive_version = true;
                return true;

            case DictKey::OBJECT_VERSION:
                if (!string_as_int(
                        data_utf8,
                        data_size,
                        &this->header->object_version,
                        eh)) {
                    return false;
                }
                this->dict_key = DictKey::NONE;
                this->did_encounter_object_version = true;
                return true;

            case DictKey::ROOT_OBJECT:
                this->header->root_object_id.utf8
                    = data_utf8;
                this->header->root_object_id.size
                    = data_size;
                this->dict_key = DictKey::NONE;
                this->did_encounter_root_object = true;
                return true;
            }
            B_BUG();
        }
        B_BUG();
    }

    B_FUNC
    visit_quoted_string(
            B_PBXScanner *,
            uint8_t const *,
            size_t,
            B_ErrorHandler const *) {
        // TODO(strager)
        B_NYI();
        return false;
    }

    B_FUNC
    visit_array_begin(
            B_PBXScanner *,
            B_OUT bool *,
            B_ErrorHandler const *eh) {
        B_LOG(B_DEBUG, "visit_array_begin()");
        return unexpected(eh);
    }

    B_FUNC
    visit_array_end(
            B_PBXScanner *,
            B_ErrorHandler const *eh) {
        B_LOG(B_DEBUG, "visit_array_end()");
        return unexpected(eh);
    }

    B_FUNC
    visit_dict_begin(
            B_PBXScanner *scanner,
            B_OUT bool *recurse,
            B_ErrorHandler const *eh) {
        B_LOG(B_DEBUG, "visit_dict_begin()");
        switch (this->ancestry) {
        case Ancestry::EMPTY:
            *recurse = true;
            this->ancestry = Ancestry::TOP;
            return true;

        case Ancestry::TOP_CLASSES:
            // TODO(strager)
            *recurse = false;
            return true;

        case Ancestry::TOP_OBJECTS:
            if (!b_pbx_scanner_offset(
                    scanner,
                    &this->object_value_start,
                    eh)) {
                return false;
            }
            *recurse = false;
            return true;

        case Ancestry::TOP:
            switch (this->dict_key) {
            case DictKey::ANY_ID:
            case DictKey::ARCHIVE_VERSION:
            case DictKey::NONE:
            case DictKey::OBJECT_VERSION:
            case DictKey::ROOT_OBJECT:
                return unexpected(eh);

            case DictKey::CLASSES:
                this->dict_key = DictKey::NONE;
                this->ancestry = Ancestry::TOP_CLASSES;
                *recurse = true;
                return true;

            case DictKey::OBJECTS:
                this->dict_key = DictKey::NONE;
                this->ancestry = Ancestry::TOP_OBJECTS;
                *recurse = true;
                return true;
            }
            B_BUG();
        }
        B_BUG();
    }

    B_FUNC
    visit_dict_key(
            B_PBXScanner *,
            uint8_t const *data_utf8,
            size_t data_size,
            B_ErrorHandler const *eh) {
        B_LOG(
            B_DEBUG,
            "visit_dict_key(%.*s)",
            (int) data_size,
            data_utf8);
        char const *data
            = reinterpret_cast<char const *>(data_utf8);
        switch (this->ancestry) {
        case Ancestry::EMPTY:
            return unexpected(eh);

        case Ancestry::TOP:
            if (this->dict_key != DictKey::NONE) {
                return unexpected(eh);
            }

#define B_CHECK_DICT_KEY(_key, _dict_key) \
    do { \
        if ((strlen((_key)) == data_size && \
                strncmp((_key), data, data_size) == 0)) { \
            this->dict_key = (_dict_key); \
            return true; \
        } \
    } while (0)
            B_CHECK_DICT_KEY(
                "archiveVersion",
                DictKey::ARCHIVE_VERSION);
            B_CHECK_DICT_KEY("classes", DictKey::CLASSES);
            B_CHECK_DICT_KEY(
                "objectVersion",
                DictKey::OBJECT_VERSION);
            B_CHECK_DICT_KEY("objects", DictKey::OBJECTS);
            B_CHECK_DICT_KEY(
                "rootObject",
                DictKey::ROOT_OBJECT);
            return unexpected(eh);
#undef B_CHECK_DICT_KEY

        case Ancestry::TOP_CLASSES:
            // TODO(strager)
            return unexpected(eh);

        case Ancestry::TOP_OBJECTS:
            this->object_id.utf8 = data_utf8;
            this->object_id.size = data_size;
            this->dict_key = DictKey::ANY_ID;
            return true;
        }
        B_BUG();
    }

    B_FUNC
    visit_dict_end(
            B_PBXScanner *scanner,
            B_ErrorHandler const *eh) {
        B_LOG(B_DEBUG, "visit_dict_end()");
        switch (this->ancestry) {
        case Ancestry::EMPTY:
            return unexpected(eh);

        case Ancestry::TOP:
            if (this->dict_key != DictKey::NONE) {
                return unexpected(eh);
            }
            this->ancestry = Ancestry::EMPTY;
            return true;

        case Ancestry::TOP_CLASSES:
            switch (this->dict_key) {
            case DictKey::NONE:
                this->ancestry = Ancestry::TOP;
                return true;
            case DictKey::ANY_ID:
                // TODO(strager)
                B_NYI();
                return true;
            default:
                return unexpected(eh);
            }

        case Ancestry::TOP_OBJECTS:
            switch (this->dict_key) {
            case DictKey::NONE:
                this->ancestry = Ancestry::TOP;
                this->did_encounter_object_dict = true;
                return true;
            case DictKey::ANY_ID:
                size_t cur_offset;
                if (!b_pbx_scanner_offset(
                        scanner,
                        &cur_offset,
                        eh)) {
                    return false;
                }
                if (!this->callback(
                    this->object_id,
                    B_PBXValueRange{
                        this->object_value_start,
                        cur_offset + 1,  // Skip '{'.
                    },
                    this->callback_opaque,
                    eh)) {
                    return false;
                }
                this->dict_key = DictKey::NONE;
                return true;
            default:
                return unexpected(eh);
            }
            return true;
        }
        B_BUG();
    }

    bool
    parse_completed() const {
        return did_encounter_archive_version
            && did_encounter_object_dict
            && did_encounter_object_version
            && did_encounter_root_object;
    }

private:
    // "List" of parent dictionaries.
    //
    // /*EMPTY*/
    // {
    //     /*TOP*/
    //     classes = { /*TOP_CLASSES*/ };
    //     /*TOP*/
    //     rootObject = foo;
    //     /*TOP*/
    //     objects = { /*TOP_OBJECTS*/ };
    //     /*TOP*/
    // }
    // /*EMPTY*/
    enum class Ancestry {
        EMPTY,
        TOP,
        TOP_CLASSES,
        TOP_OBJECTS,
    };

    enum class DictKey {
        ANY_ID,
        ARCHIVE_VERSION,
        CLASSES,
        NONE,
        OBJECTS,
        OBJECT_VERSION,
        ROOT_OBJECT,
    };

    static B_FUNC
    unexpected(
            B_ErrorHandler const *eh) {
        (void) B_RAISE_ERRNO_ERROR(
            eh,
            EINVAL,
            "b_pbx_parse");
        return false;
    }

    static B_FUNC
    string_as_int(
            uint8_t const *data_utf8,
            size_t data_size,
            B_OUT uint32_t *out,
            B_ErrorHandler const *eh) {
        uint8_t bytes[30];
        size_t length = std::min(
            data_size,
            sizeof(bytes) - 1);
        memcpy(bytes, data_utf8, length);
        bytes[length] = 0;

        int x;
        B_STATIC_ASSERT(
            sizeof(*out) == sizeof(x),
            "Please modify the scanf modifier and type");
        int rc = sscanf(
            reinterpret_cast<char *>(bytes),
            "%i",
            &x);
        if (rc != 1) {
            return unexpected(eh);
        }

        *out = static_cast<uint32_t>(x);
        return true;
    }

    Ancestry ancestry;
    DictKey dict_key;

    B_PBXObjectID object_id;
    size_t object_value_start;

    B_PBXHeader *header;
    B_PBXObjectRangeCallback *callback;
    void *callback_opaque;

    bool did_encounter_archive_version;
    bool did_encounter_object_dict;
    bool did_encounter_object_version;
    bool did_encounter_root_object;
};

B_EXPORT_FUNC
b_pbx_parse(
        uint8_t const *data_utf8,
        size_t data_size,
        B_OUT B_PBXHeader *header,
        B_PBXObjectRangeCallback *callback,
        void *callback_opaque,
        B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, data_utf8);
    B_CHECK_PRECONDITION(eh, header);
    B_CHECK_PRECONDITION(eh, callback);

    HeaderAndObjectsVisitor_ visitor(
        header,
        callback,
        callback_opaque);
    if (!b_pbx_scan_value(
            data_utf8,
            data_size,
            &visitor,
            eh)) {
        return false;
    }

    if (!visitor.parse_completed()) {
        // FIXME(strager)
        (void) B_RAISE_ERRNO_ERROR(eh, EINVAL, "");
        return false;
    }

    // out should have been assigned by the visitor.
    return true;
}

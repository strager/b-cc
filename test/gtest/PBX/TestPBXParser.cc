#include "../Util.h"

#include <B/PBX/PBXParser.h>

#include <gtest/gtest.h>
#include <memory>

namespace {

    struct Object_ {
        B_PBXObjectID id;
        B_PBXValueRange value_range;
    };

    B_FUNC
    parse_pbx_(
            char const *data,
            B_OUT B_PBXHeader *header,
            std::vector<Object_> &objects,
            B_ErrorHandler const *eh) {
        return b_pbx_parse(
            reinterpret_cast<uint8_t const *>(data),
            strlen(data),
            header,
            [&objects](
                    B_PBXObjectID object_id,
                    B_PBXValueRange value_range,
                    B_ErrorHandler const *) {
                objects.emplace_back(
                    Object_{object_id, value_range});
                return true;
            },
            eh);
    }

    B_FUNC
    parse_pbx_(
            char const *data,
            B_OUT B_PBXHeader *header,
            B_ErrorHandler const *eh) {
        std::vector<Object_> objects;
        bool ok = parse_pbx_(data, header, objects, eh);
        if (ok) {
            EXPECT_TRUE(objects.empty());
        }
        return ok;
    }

    std::string
    object_id_string_(
            B_PBXObjectID const *object_id) {
        return std::string(
            reinterpret_cast<char const *>(
                object_id->utf8),
            object_id->size);
    }
}

TEST(TestPBXParser, ParseStringWithoutObjectsOrWhitespace) {
    B_ErrorHandler const *eh = nullptr;
    B_PBXHeader header;
    ASSERT_TRUE(parse_pbx_(
        "{archiveVersion=1;classes={};objectVersion=46;objects={};rootObject=x;}",
        &header,
        eh));
    EXPECT_EQ(1U, header.archive_version);
    EXPECT_EQ(46U, header.object_version);
    EXPECT_EQ(
        "x",
        object_id_string_(&header.root_object_id));
}

TEST(TestPBXParser, ParseStringWithoutObjects) {
    B_ErrorHandler const *eh = nullptr;
    B_PBXHeader header;
    ASSERT_TRUE(parse_pbx_(
        "// !$*UTF8*$!\n"
        "{\n"
        "\tarchiveVersion = 1;\n"
        "\tclasses = {\n"
        "\t};\n"
        "\tobjectVersion = 46;\n"
        "\tobjects = {\n"
        "\t};\n"
        "\trootObject = CCAD436218E286AA00EEE595 /* Project object */;\n"
        "}\n",
        &header,
        eh));
    EXPECT_EQ(1U, header.archive_version);
    EXPECT_EQ(46U, header.object_version);
    EXPECT_EQ(
        "CCAD436218E286AA00EEE595",
        object_id_string_(&header.root_object_id));
}

TEST(TestPBXParser, ParseStringWithObjects) {
    B_ErrorHandler const *eh = nullptr;
    B_PBXHeader header;
    std::vector<Object_> objects;
    ASSERT_TRUE(parse_pbx_(
        "// !$*UTF8*$!\n"
        "{\n"
        "\tarchiveVersion = 1;\n"
        "\tclasses = {\n"
        "\t};\n"
        "\tobjectVersion = 46;\n"
        "\tobjects = {\n"
        "/* Begin PBXBuildFile section */\n"
        "\t\tCCAD436E18E286AA00EEE595 /* main.c in Sources */ = {isa = PBXBuildFile; fileRef = CCAD436D18E286AA00EEE595 /* main.c */; };\n"
        "\t\tCCAD437018E286AA00EEE595 /* tool_name.1 in CopyFiles */ = {isa = PBXBuildFile; fileRef = CCAD436F18E286AA00EEE595 /* tool_name.1 */; };\n"
        "/* End PBXBuildFile section */\n"
        "\t};\n"
        "\trootObject = CCAD436218E286AA00EEE595 /* Project object */;\n"
        "}\n",
        &header,
        objects,
        eh));
    EXPECT_EQ(1U, header.archive_version);
    EXPECT_EQ(46U, header.object_version);
    EXPECT_EQ(
        "CCAD436218E286AA00EEE595",
        object_id_string_(&header.root_object_id));

    EXPECT_EQ(2U, objects.size());
    if (objects.size() == 2) {
        EXPECT_EQ(
            "CCAD436E18E286AA00EEE595",
            object_id_string_(&objects[0].id));
        EXPECT_EQ(174U, objects[0].value_range.start);
        EXPECT_EQ(245U, objects[0].value_range.end);

        EXPECT_EQ(
            "CCAD437018E286AA00EEE595",
            object_id_string_(&objects[1].id));
        EXPECT_EQ(307U, objects[1].value_range.start);
        EXPECT_EQ(383U, objects[1].value_range.end);
    }
}

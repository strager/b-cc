#include "../Util.h"

#include <B/PBX/PBXParser.h>
#include <B/PBX/PBXProjectFile.h>
#include <B/QuestionAnswer.h>

#include <cstdio>
#include <gtest/gtest.h>

namespace {
    struct Object_ {
        B_PBXObjectID id;
        B_PBXValueRange value_range;
    };

    std::string
    object_id_string_(
            B_PBXObjectID const *object_id) {
        return std::string(
            reinterpret_cast<char const *>(
                object_id->utf8),
            object_id->size);
    }
}

TEST(TestPBXProjectFile, AnswerExample) {
    B_ErrorHandler const *eh = nullptr;

    auto temp_dir = B_TemporaryDirectory::create();
    std::string file_path
        = temp_dir.path() + "/project.pbxproj";

    {
        FILE *file = fopen(file_path.c_str(), "wb");
        ASSERT_NE(nullptr, file);
        char const data[] = ""
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
            "}\n";
        ASSERT_EQ(
            sizeof(data) - 1,
            fwrite(data, 1, sizeof(data) - 1, file));
        ASSERT_EQ(0, fclose(file));
    }


    B_QuestionVTable const *question_vtable
        = b_pbx_project_file_question_vtable();
    B_Answer *answer;
    ASSERT_TRUE(question_vtable->answer(
        static_cast<B_Question const *>(
            static_cast<void const *>(file_path.c_str())),
        &answer,
        eh));
    ASSERT_NE(nullptr, answer);

    B_PBXHeader header;
    std::vector<Object_> objects;
    ASSERT_TRUE(b_pbx_project_file_answer(
        answer,
        &header,
        [&objects](
                B_PBXObjectID object_id,
                B_PBXValueRange value_range,
                B_ErrorHandler const *) {
            objects.emplace_back(
                Object_{object_id, value_range});
            return true;
        },
        eh));

    EXPECT_EQ(1U, header.archive_version);
    EXPECT_EQ(46U, header.object_version);
    EXPECT_EQ(
        "CCAD436218E286AA00EEE595",
        object_id_string_(&header.root_object_id));

    EXPECT_EQ(2U, objects.size());
    if (objects.size() == 2) {
        // FIXME(strager): Should the order of objects
        // matter?

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

#include <B/PBX/PBXScanner.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <tuple>

using namespace testing;

class MockValueVisitor :
        public B_PBXValueVisitorClass<MockValueVisitor> {
public:
    MOCK_METHOD4(visit_string, B_FUNC(
        B_PBXScanner *,
        uint8_t const *data_utf8,
        size_t data_size,
        B_ErrorHandler const *));

    MOCK_METHOD4(visit_quoted_string, B_FUNC(
        B_PBXScanner *,
        uint8_t const *data_utf8,
        size_t data_size,
        B_ErrorHandler const *));

    MOCK_METHOD3(visit_array_begin, B_FUNC(
        B_PBXScanner *,
        B_OUT bool *recurse,
        B_ErrorHandler const *));

    MOCK_METHOD2(visit_array_end, B_FUNC(
        B_PBXScanner *,
        B_ErrorHandler const *));

    MOCK_METHOD3(visit_dict_begin, B_FUNC(
        B_PBXScanner *,
        B_OUT bool *recurse,
        B_ErrorHandler const *));

    MOCK_METHOD4(visit_dict_key, B_FUNC(
        B_PBXScanner *,
        uint8_t const *data_utf8,
        size_t data_size,
        B_ErrorHandler const *));

    MOCK_METHOD2(visit_dict_end, B_FUNC(
        B_PBXScanner *,
        B_ErrorHandler const *));
};

static B_FUNC
pbx_scan_value_(
        char const *data,
        B_PBXValueVisitor *visitor,
        B_ErrorHandler const *eh) {
    return b_pbx_scan_value(
        reinterpret_cast<uint8_t const *>(data),
        strlen(data),
        visitor,
        eh);
}

static size_t
offset_(
        B_PBXScanner *scanner) {
    B_ErrorHandler const *eh = nullptr;
    size_t offset;
    bool ok = b_pbx_scanner_offset(scanner, &offset, eh);
    EXPECT_TRUE(ok);
    if (!ok) {
        return 0;
    }
    return offset;
}

TEST(TestPBXScanner, ScanEmptyDictWithRecurse) {
    B_ErrorHandler const *eh = nullptr;

    StrictMock<MockValueVisitor> visitor;
    {
        InSequence sequence;
        EXPECT_CALL(visitor, visit_dict_begin(
                ResultOf(offset_, 0),
                _,
                _))
            .WillOnce(DoAll(
                SetArgPointee<1>(true), // recurse
                Return(true)));
        EXPECT_CALL(visitor, visit_dict_end(
                ResultOf(offset_, 2),
                _))
            .WillOnce(Return(true));
    }

    ASSERT_TRUE(pbx_scan_value_(
        "{}",
        &visitor,
        eh));
}

TEST(TestPBXScanner, ScanEmptyDictWithRecurseWithSilence) {
    B_ErrorHandler const *eh = nullptr;

    StrictMock<MockValueVisitor> visitor;
    {
        InSequence sequence;
        EXPECT_CALL(visitor, visit_dict_begin(
                ResultOf(offset_, 1),
                _,
                _))
            .WillOnce(DoAll(
                SetArgPointee<1>(true), // recurse
                Return(true)));
        EXPECT_CALL(visitor, visit_dict_end(
                ResultOf(offset_, 13),
                _))
            .WillOnce(Return(true));
    }

    ASSERT_TRUE(pbx_scan_value_(
        "\n{\t/* hi */\t} // yup",
        &visitor,
        eh));
}

static std::string
utf8_string_(
        std::tuple<uint8_t const *, size_t> tuple) {
    uint8_t const *data;
    size_t data_size;
    std::tie(data, data_size) = tuple;
    return std::string(
        reinterpret_cast<char const *>(data),
        data_size);
}

TEST(TestPBXScanner, ScanStringWithLeadingSlash) {
    B_ErrorHandler const *eh = nullptr;

    StrictMock<MockValueVisitor> visitor;
    EXPECT_CALL(
            visitor,
            visit_string(ResultOf(offset_, 0), _, _, _))
        .With(Args<1, 2>(ResultOf(
            utf8_string_,
            "/this/is/a/string")))
        .WillOnce(Return(true));

    ASSERT_TRUE(pbx_scan_value_(
        "/this/is/a/string",
        &visitor,
        eh));
}

TEST(TestPBXScanner, ScanEmptyArrayWithRecurse) {
    B_ErrorHandler const *eh = nullptr;

    StrictMock<MockValueVisitor> visitor;
    {
        InSequence sequence;
        EXPECT_CALL(visitor, visit_array_begin(
                ResultOf(offset_, 0),
                _,
                _))
            .WillOnce(DoAll(
                SetArgPointee<1>(true), // recurse
                Return(true)));
        EXPECT_CALL(visitor, visit_array_end(
                ResultOf(offset_, 2),
                _))
            .WillOnce(Return(true));
    }

    ASSERT_TRUE(pbx_scan_value_(
        "()",
        &visitor,
        eh));
}

TEST(TestPBXScanner, ScanQuotedStringWithSpace) {
    B_ErrorHandler const *eh = nullptr;

    StrictMock<MockValueVisitor> visitor;
    EXPECT_CALL(visitor, visit_quoted_string(
            ResultOf(offset_, 0),
            _,
            _,
            _))
        .With(Args<1, 2>(ResultOf(
            utf8_string_,
            "\"hello world\"")))
        .WillOnce(Return(true));

    ASSERT_TRUE(pbx_scan_value_(
        "\"hello world\"",
        &visitor,
        eh));
}

TEST(TestPBXScanner, ScanQuotedEmptyString) {
    B_ErrorHandler const *eh = nullptr;

    StrictMock<MockValueVisitor> visitor;
    EXPECT_CALL(visitor, visit_quoted_string(
            ResultOf(offset_, 0),
            _,
            _,
            _))
        .With(Args<1, 2>(ResultOf(
            utf8_string_,
            "\"\"")))
        .WillOnce(Return(true));

    ASSERT_TRUE(pbx_scan_value_(
        "\"\"",
        &visitor,
        eh));
}

#if 0
TEST(TestPBXScanner, ScanUnterminatedQuoteString) {
    B_ErrorHandler const *eh = nullptr;

    StrictMock<MockValueVisitor> visitor;
    ASSERT_FALSE(pbx_scan_value_(
        "\"",
        &visitor,
        eh));
}

TEST(TestPBXScanner, ScanUnterminatedQuoteStringEscape) {
    B_ErrorHandler const *eh = nullptr;

    StrictMock<MockValueVisitor> visitor;
    ASSERT_FALSE(pbx_scan_value_(
        "\"\\",
        &visitor,
        eh));
}
#endif

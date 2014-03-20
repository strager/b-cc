#ifndef B_HEADER_GUARD_95037BE5_7D49_4763_B842_56AF0F404060
#define B_HEADER_GUARD_95037BE5_7D49_4763_B842_56AF0F404060

#include <B/Assert.h>
#include <B/QuestionAnswer.h>
#include <B/RefCount.h>
#include <B/UUID.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace testing;

template<typename T>
struct RemoveMockWrapper {
    typedef T type;
};

template<typename TOther>
struct RemoveMockWrapper<NiceMock<TOther>> {
    typedef TOther type;
};

template<typename TOther>
struct RemoveMockWrapper<NaggyMock<TOther>> {
    typedef TOther type;
};

template<typename TOther>
struct RemoveMockWrapper<StrictMock<TOther>> {
    typedef TOther type;
};

template<typename T>
static void
MockRefCounting(
        T &self) {
    typedef typename RemoveMockWrapper<T>::type NonMock;

    EXPECT_CALL(self, deallocate(_))
        .WillRepeatedly(Invoke([&self](
                B_ErrorHandler const *eh) {
            bool should_deallocate = false;
            if (!B_RELEASE(
                    &self,
                    &should_deallocate,
                    eh)) {
                return false;
            }
            EXPECT_FALSE(should_deallocate);
            return true;
        }));

    EXPECT_CALL(self, replicate(_, _))
        .WillRepeatedly(Invoke([&self](
                NonMock **out,
                B_ErrorHandler const *eh) {
            if (!B_RETAIN(&self, eh)) {
                return false;
            }
            *out = &self;
            return true;
        }));
}

class MockAnswer :
        public B_AnswerClass<MockAnswer> {
public:
    B_REF_COUNTED_OBJECT;

    MockAnswer() :
            B_REF_COUNT_CONSTRUCTOR_INITIALIZER {
    }

    MOCK_CONST_METHOD3(equals, B_FUNC(
        MockAnswer const &,
        B_OUTPTR bool *,
        B_ErrorHandler const *));

    static B_FUNC
    equal(
            MockAnswer const &a,
            MockAnswer const &b,
            B_OUTPTR bool *out,
            B_ErrorHandler const *eh) {
        return a.equals(b, out, eh);
    }

    MOCK_CONST_METHOD2(replicate, B_FUNC(
        B_OUTPTR MockAnswer **,
        B_ErrorHandler const *));

    MOCK_METHOD1(deallocate, B_FUNC(
        B_ErrorHandler const *));

    MOCK_CONST_METHOD2(serialize, B_FUNC(
        B_OUT B_Serialized *,
        B_ErrorHandler const *));

    static B_FUNC
    deserialize(
            B_BORROWED B_Serialized,
            B_OUTPTR MockAnswer **,
            B_ErrorHandler const *) {
        B_NYI();
    }
};

class MockQuestion :
        public B_QuestionClass<MockQuestion> {
public:
    typedef MockAnswer AnswerClass;
    B_REF_COUNTED_OBJECT;

    MockQuestion() :
            B_REF_COUNT_CONSTRUCTOR_INITIALIZER {
    }

    MOCK_CONST_METHOD2(answer, B_FUNC(
        B_OUTPTR MockAnswer **,
        B_ErrorHandler const *));

    MOCK_CONST_METHOD3(equals, B_FUNC(
        MockQuestion const &,
        B_OUTPTR bool *,
        B_ErrorHandler const *));

    static B_FUNC
    equal(
            MockQuestion const &a,
            MockQuestion const &b,
            B_OUTPTR bool *out,
            B_ErrorHandler const *eh) {
        return a.equals(b, out, eh);
    }

    MOCK_CONST_METHOD2(replicate, B_FUNC(
        B_OUTPTR MockQuestion **,
        B_ErrorHandler const *));

    MOCK_METHOD1(deallocate, B_FUNC(
        B_ErrorHandler const *));

    MOCK_CONST_METHOD2(serialize, B_FUNC(
        B_OUT B_Serialized *,
        B_ErrorHandler const *));

    static B_FUNC
    deserialize(
            B_BORROWED B_Serialized,
            B_OUTPTR MockQuestion **,
            B_ErrorHandler const *) {
        B_NYI();
    }
};

#endif

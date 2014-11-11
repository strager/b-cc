#ifndef B_HEADER_GUARD_95037BE5_7D49_4763_B842_56AF0F404060
#define B_HEADER_GUARD_95037BE5_7D49_4763_B842_56AF0F404060

#include <B/Assert.h>
#include <B/Error.h>
#include <B/Private/RefCount.h>
#include <B/QuestionAnswer.h>
#include <B/UUID.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <stddef.h>

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

    ~MockAnswer() {
        uint32_t ref_count;
        bool ok = B_CURRENT_REF_COUNT_DEBUG(
            this,
            &ref_count,
            nullptr);
        EXPECT_TRUE(ok);
        if (ok) {
            EXPECT_EQ(1U, ref_count);
        }
    }

    MOCK_CONST_METHOD3(equals, bool(
        MockAnswer const &,
        B_OUTPTR bool *,
        B_ErrorHandler const *));

    static B_FUNC
    equal(
            MockAnswer const *a,
            MockAnswer const *b,
            B_OUTPTR bool *out,
            B_ErrorHandler const *eh) {
        return a->equals(*b, out, eh);
    }

    MOCK_CONST_METHOD2(replicate, bool(
        B_OUTPTR MockAnswer **,
        B_ErrorHandler const *));

    static B_FUNC
    replicate(
            MockAnswer const *self,
            B_OUTPTR MockAnswer **out,
            B_ErrorHandler const *eh) {
        return self->replicate(out, eh);
    }

    MOCK_METHOD1(deallocate, bool(
        B_ErrorHandler const *));

    static B_FUNC
    deallocate(
            MockAnswer *self,
            B_ErrorHandler const *eh) {
        return self->deallocate(eh);
    }

    MOCK_CONST_METHOD2(serialize, bool(
        B_ByteSink *,
        B_ErrorHandler const *));

    static B_FUNC
    serialize(
            MockAnswer const *self,
            B_ByteSink *sink,
            B_ErrorHandler const *eh) {
        return self->serialize(sink, eh);
    }

    static B_FUNC
    deserialize(
            B_ByteSource *,
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

    ~MockQuestion() {
        uint32_t ref_count;
        bool ok = B_CURRENT_REF_COUNT_DEBUG(
            this,
            &ref_count,
            nullptr);
        EXPECT_TRUE(ok);
        if (ok) {
            EXPECT_EQ(1U, ref_count);
        }
    }

    MOCK_CONST_METHOD2(answer, bool(
        B_OUTPTR MockAnswer **,
        B_ErrorHandler const *));

    static B_FUNC
    answer(
            MockQuestion const *self,
            B_OUTPTR MockAnswer **out,
            B_ErrorHandler const *eh) {
        return self->answer(out, eh);
    }

    MOCK_CONST_METHOD3(equals, bool(
        MockQuestion const &,
        B_OUTPTR bool *,
        B_ErrorHandler const *));

    static B_FUNC
    equal(
            MockQuestion const *a,
            MockQuestion const *b,
            B_OUTPTR bool *out,
            B_ErrorHandler const *eh) {
        return a->equals(*b, out, eh);
    }

    MOCK_CONST_METHOD2(replicate, bool(
        B_OUTPTR MockQuestion **,
        B_ErrorHandler const *));

    static B_FUNC
    replicate(
            MockQuestion const *self,
            B_OUTPTR MockQuestion **out,
            B_ErrorHandler const *eh) {
        return self->replicate(out, eh);
    }

    MOCK_METHOD1(deallocate, bool(
        B_ErrorHandler const *));

    static B_FUNC
    deallocate(
            MockQuestion *self,
            B_ErrorHandler const *eh) {
        return self->deallocate(eh);
    }

    MOCK_CONST_METHOD2(serialize, bool(
        B_ByteSink *,
        B_ErrorHandler const *));

    static B_FUNC
    serialize(
            MockQuestion const *self,
            B_ByteSink *sink,
            B_ErrorHandler const *eh) {
        return self->serialize(sink, eh);
    }

    static B_FUNC
    deserialize(
            B_ByteSource *,
            B_OUTPTR MockQuestion **,
            B_ErrorHandler const *) {
        B_NYI();
    }
};

class MockErrorHandler :
        public B_ErrorHandler {
public:
    MockErrorHandler() :
            B_ErrorHandler{handle_error_} {
    }

    MOCK_CONST_METHOD1(handle_error, B_ErrorHandlerResult(
        B_Error));

private:
    static B_ErrorHandlerResult
    handle_error_(
            struct B_ErrorHandler const *eh,
            struct B_Error error) {
        return static_cast<MockErrorHandler const *>(eh)
            ->handle_error(error);
    }
};

#endif

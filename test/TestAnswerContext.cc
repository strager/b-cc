#include <B/AnswerContext.h>
#include <B/Assert.h>
#include <B/QuestionAnswer.h>
#include <B/QuestionQueue.h>
#include <B/RefCount.h>

#include <memory>
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

template<>
B_UUID
B_QuestionClass<MockQuestion>::uuid
    = B_UUID_LITERAL("B7E06E83-B1D7-42BA-B9EA-EBA122B75697");

// Ensures b_answer_context_need_one enqueues the question
// on the QuestionQueue exactly once.
TEST(TestAnswerContext, NeedOneEnqueues) {
    B_ErrorHandler const *eh = nullptr;

    // Must be alive while question_queue is destructed.
    StrictMock<MockQuestion> question;
    MockRefCounting(question);
    StrictMock<MockQuestion> needed_question;
    MockRefCounting(needed_question);
    B_AnswerContext answer_context;

    B_QuestionQueue *question_queue_raw;
    ASSERT_TRUE(b_question_queue_allocate(
        &question_queue_raw,
        eh));
    std::unique_ptr<B_QuestionQueue, B_QuestionQueueDeleter>
        question_queue(question_queue_raw, eh);
    question_queue_raw = nullptr;

    answer_context.question = &question;
    answer_context.question_vtable = MockQuestion::vtable();
    answer_context.answer_callback = [](
            B_Answer *,
            void *,
            B_ErrorHandler const *) {
        // TODO(strager)
        return false;
    };
    answer_context.answer_callback_opaque = nullptr;
    answer_context.question_queue = question_queue.get();
    answer_context.dependency_delegate = nullptr;

    ASSERT_TRUE(b_answer_context_need_one(
        &answer_context,
        &needed_question,
        MockQuestion::vtable(),
        [](
                B_TRANSFER B_Answer *,
                B_ErrorHandler const *) {
            return false;
        },
        [](
                B_ErrorHandler const *) {
            return false;
        },
        eh));

    B_QuestionQueueItemObject *first_queue_item_raw;
    ASSERT_TRUE(b_question_queue_try_dequeue(
        question_queue.get(),
        &first_queue_item_raw,
        eh));
    ASSERT_NE(nullptr, first_queue_item_raw);
    std::unique_ptr<
            B_QuestionQueueItemObject,
            B_QuestionQueueItemDeleter>
        first_queue_item(first_queue_item_raw, eh);
    first_queue_item_raw = nullptr;

    EXPECT_EQ(&needed_question, first_queue_item->question);
    EXPECT_EQ(
        MockQuestion::vtable(),
        first_queue_item->question_vtable);

    B_QuestionQueueItemObject *second_queue_item_raw;
    ASSERT_TRUE(b_question_queue_try_dequeue(
        question_queue.get(),
        &second_queue_item_raw,
        eh));
    ASSERT_EQ(nullptr, second_queue_item_raw);
}

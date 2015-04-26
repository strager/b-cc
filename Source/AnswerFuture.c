#define B_DEBUG_ANSWER_FUTURE_ 0

#include <B/AnswerFuture.h>
#include <B/Memory.h>
#include <B/Private/AnswerFuture.h>
#include <B/Private/Assertions.h>
#include <B/Private/Log.h>
#include <B/Private/Memory.h>
#include <B/QuestionAnswer.h>

#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <string.h>

#if B_DEBUG_ANSWER_FUTURE_
# include <dlfcn.h>
# include <stdio.h>
#endif

// Force maximum alignment.
// TODO(strager): Vector types (SSE, NEON)?
// TODO(strager): Float types (long double, complex)?
union B_UserData_ {
  uint8_t bytes[1];
  char char_padding;
  long long_padding;
  long long long_long_padding;
  void *pointer_padding;
  void (*function_padding)(void);
};

struct B_AnswerFutureCallbackEntry {
  B_SLIST_ENTRY(B_AnswerFutureCallbackEntry) link;
  B_AnswerFutureCallback *callback;
  union B_UserData_ user_data;
};

union B_AnswerOrError_ {
  struct B_IAnswer *answer;
  struct B_Error error;
};

struct B_AnswerFutureAnswerEntry {
  struct B_AnswerVTable const *answer_vtable;
  enum B_AnswerFutureState state;
  union B_AnswerOrError_ result;
};

struct B_AnswerFuture {
  B_SLIST_HEAD(, B_AnswerFutureCallbackEntry) callbacks;
  uint32_t retain_count;
  size_t answer_entry_count;
  // Variadic.
  struct B_AnswerFutureAnswerEntry answer_entries[1];
};

static B_FUNC void
b_answer_future_dump_(
    B_BORROW struct B_AnswerFuture *future,
    B_BORROW void *return_address,
    B_BORROW char const *function) {
#if B_DEBUG_ANSWER_FUTURE_
  Dl_info info;
  if (!dladdr(return_address, &info)) {
    info.dli_sname = "???";
  }
  fprintf(
    stderr,
    "%s: %s(%p) -> %llu\n",
    info.dli_sname,
    function,
    (void *) future,
    (unsigned long long) future->retain_count);
#else
  (void) future;
  (void) return_address;
  (void) function;
#endif
}

static B_WUR B_FUNC bool
b_answer_future_check_callbacks_(
    B_BORROW struct B_AnswerFuture *future,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(future);
  B_OUT_PARAMETER(e);

  enum B_AnswerFutureState state;
  if (!b_answer_future_state(future, &state, e)) {
    return false;
  }
  switch (state) {
  case B_FUTURE_PENDING:
    return true;
  case B_FUTURE_RESOLVED:
  case B_FUTURE_FAILED:
    {
      // Call the callbacks!
      struct B_AnswerFutureCallbackEntry *callback_entry;
      struct B_AnswerFutureCallbackEntry *temp_entry;
      B_SLIST_FOREACH_SAFE(
          callback_entry,
          &future->callbacks,
          link,
          temp_entry) {
        if (!callback_entry->callback(
            future,
            callback_entry->user_data.bytes,
            &(struct B_Error) {})) {
          B_NYI();
        }
        b_deallocate(callback_entry);
      }
      B_SLIST_INIT(&future->callbacks);
      return true;
    }
  }
  B_UNREACHABLE();
}

static B_WUR B_FUNC size_t
b_answer_future_size_(
    size_t answer_count) {
  // FIXME(strager): This may overallocate.
  // TODO(strager): Check for overflow.
  return offsetof(
    struct B_AnswerFuture, answer_entries[answer_count]);
}

static B_FUNC void
b_answer_future_initialize_(
    B_BORROW struct B_AnswerFuture *future,
    size_t answer_count) {
  B_PRECONDITION(future);
  B_PRECONDITION(answer_count > 0);

  *future = (struct B_AnswerFuture) {
    .callbacks = B_SLIST_HEAD_INITIALIZER(
      &future->callbacks),
    .retain_count = 1,
    .answer_entry_count = answer_count,
    // .answer_entries
  };
  for (size_t i = 0; i < answer_count; ++i) {
    struct B_AnswerFutureAnswerEntry *entry
      = &future->answer_entries[i];
    b_scribble(entry, sizeof(*entry));
    entry->state = B_FUTURE_PENDING;
  }
}

B_WUR B_EXPORT_FUNC bool
b_answer_future_allocate_one(
    B_BORROW struct B_AnswerVTable const *answer_vtable,
    B_OUT_TRANSFER struct B_AnswerFuture **out,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(answer_vtable);
  B_OUT_PARAMETER(out);
  B_OUT_PARAMETER(e);

  struct B_AnswerFuture *future;
  if (!b_allocate(
      b_answer_future_size_(1), (void **) &future, e)) {
    return false;
  }
  b_answer_future_initialize_(future, 1);
  future->answer_entries[0].answer_vtable = answer_vtable;
  b_answer_future_dump_(
    future,
    __builtin_return_address(0),
    "b_answer_future_allocate");
  *out = future;
  return true;
}

B_WUR B_EXPORT_FUNC bool
b_answer_future_resolve(
    B_BORROW struct B_AnswerFuture *future,
    B_TRANSFER struct B_IAnswer *answer,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(future);
  B_PRECONDITION(future->answer_entry_count == 1);
  B_PRECONDITION(answer);
  B_OUT_PARAMETER(e);

  struct B_AnswerFutureAnswerEntry *entry
    = &future->answer_entries[0];
  B_PRECONDITION(entry->state == B_FUTURE_PENDING);
  entry->state = B_FUTURE_RESOLVED;
  entry->result.answer = answer;
  if (!b_answer_future_check_callbacks_(
      future, &(struct B_Error) {})) {
    B_NYI();
  }
  return true;
}

B_WUR B_EXPORT_FUNC bool
b_answer_future_fail(
    B_BORROW struct B_AnswerFuture *future,
    struct B_Error error,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(future);
  B_PRECONDITION(future->answer_entry_count == 1);
  B_OUT_PARAMETER(e);

  struct B_AnswerFutureAnswerEntry *entry
    = &future->answer_entries[0];
  B_PRECONDITION(entry->state == B_FUTURE_PENDING);
  entry->state = B_FUTURE_FAILED;
  entry->result.error = error;
  if (!b_answer_future_check_callbacks_(
      future, &(struct B_Error) {})) {
    B_NYI();
  }
  return true;
}

B_WUR B_EXPORT_FUNC bool
b_answer_future_state(
    B_BORROW struct B_AnswerFuture *future,
    B_OUT enum B_AnswerFutureState *state,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(future);
  B_OUT_PARAMETER(state);
  B_OUT_PARAMETER(e);

  // If any are pending, return pending.
  // Otherwise, if any have failed, return failed.
  // Otherwise, return resolved.
  bool any_failed = false;
  for (size_t i = 0; i < future->answer_entry_count; ++i) {
    struct B_AnswerFutureAnswerEntry *entry
      = &future->answer_entries[i];
    switch (entry->state) {
    case B_FUTURE_PENDING:
      *state = B_FUTURE_PENDING;
      return true;
    case B_FUTURE_FAILED:
      any_failed = true;
      break;
    case B_FUTURE_RESOLVED:
      break;
    }
  }
  if (any_failed) {
    *state = B_FUTURE_FAILED;
  } else {
    *state = B_FUTURE_RESOLVED;
  }
  return true;
}

B_WUR B_EXPORT_FUNC bool
b_answer_future_answer_count(
    B_BORROW struct B_AnswerFuture *future,
    B_OUT size_t *out,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(future);
  B_OUT_PARAMETER(out);
  B_OUT_PARAMETER(e);

  enum B_AnswerFutureState state;
  if (!b_answer_future_state(future, &state, e)) {
    return false;
  }
  if (state != B_FUTURE_RESOLVED) {
    *e = (struct B_Error) {.posix_error = EOPNOTSUPP};
    return false;
  }
  *out = future->answer_entry_count;
  return true;
}

B_WUR B_EXPORT_FUNC bool
b_answer_future_answer(
    B_BORROW struct B_AnswerFuture *future,
    size_t index,
    B_OUT_BORROW struct B_IAnswer const **out,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(future);
  B_OUT_PARAMETER(out);
  B_OUT_PARAMETER(e);

  enum B_AnswerFutureState state;
  if (!b_answer_future_state(future, &state, e)) {
    return false;
  }
  if (state != B_FUTURE_RESOLVED) {
    *e = (struct B_Error) {.posix_error = EOPNOTSUPP};
    return false;
  }
  B_PRECONDITION(index < future->answer_entry_count);
  *out = future->answer_entries[index].result.answer;
  return true;
}

B_WUR B_EXPORT_FUNC bool
b_answer_future_add_callback(
    B_BORROW struct B_AnswerFuture *future,
    B_TRANSFER B_AnswerFutureCallback *callback,
    B_TRANSFER void const *callback_data,
    size_t callback_data_size,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(future);
  B_PRECONDITION(callback);
  B_PRECONDITION(callback_data);
  B_OUT_PARAMETER(e);

  enum B_AnswerFutureState state;
  if (!b_answer_future_state(future, &state, e)) {
    return false;
  }
  switch (state) {
  case B_FUTURE_PENDING:
    {
      // TODO(strager): Check for overflow.
      size_t entry_size = offsetof(
        struct B_AnswerFutureCallbackEntry,
        user_data.bytes[callback_data_size]);
      struct B_AnswerFutureCallbackEntry *entry;
      if (!b_allocate(entry_size, (void **) &entry, e)) {
        return false;
      }
      *entry = (struct B_AnswerFutureCallbackEntry) {
        // .link
        .callback = callback,
        // .user_data
      };
      memcpy(
        entry->user_data.bytes,
        callback_data,
        callback_data_size);
      B_SLIST_INSERT_HEAD(&future->callbacks, entry, link);
      return true;
    }
  case B_FUTURE_RESOLVED:
  case B_FUTURE_FAILED:
    if (!callback(
        future, callback_data, &(struct B_Error) {})) {
      B_NYI();
    }
    return true;
  }
  B_UNREACHABLE();
}

B_WUR B_EXPORT_FUNC void
b_answer_future_retain(
    B_BORROW struct B_AnswerFuture *future) {
  B_PRECONDITION(future);
  B_PRECONDITION(future->retain_count != 0);

  future->retain_count += 1;
  b_answer_future_dump_(
    future,
    __builtin_return_address(0),
    "b_answer_future_retain");
}

B_WUR B_EXPORT_FUNC void
b_answer_future_release(
    B_TRANSFER struct B_AnswerFuture *future) {
  B_PRECONDITION(future);
  B_PRECONDITION(future->retain_count != 0);

  future->retain_count -= 1;
  b_answer_future_dump_(
    future,
    __builtin_return_address(0),
    "b_answer_future_release");
  if (future->retain_count == 0) {
    B_ASSERT(B_SLIST_EMPTY(&future->callbacks));
    for (
        size_t i = 0;
        i < future->answer_entry_count;
        ++i) {
      struct B_AnswerFutureAnswerEntry *e
        = &future->answer_entries[i];
      switch (e->state) {
      case B_FUTURE_PENDING:
      case B_FUTURE_FAILED:
        break;
      case B_FUTURE_RESOLVED:
        e->answer_vtable->deallocate(e->result.answer);
        break;
      }
    }
    b_deallocate(future);
  }
}

struct B_AnswerFutureJoinChildClosure_ {
  struct B_AnswerFuture *parent_future;
  size_t parent_answer_entry_index;
};

static B_WUR B_FUNC bool
b_answer_future_join_child_callback_(
    B_BORROW struct B_AnswerFuture *future,
    B_BORROW void const *callback_data,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(future);
  B_PRECONDITION(callback_data);
  B_OUT_PARAMETER(e);

  struct B_AnswerFutureJoinChildClosure_ const *closure
    = callback_data;
  struct B_AnswerFutureAnswerEntry *parent_entries
    = &closure->parent_future->answer_entries[
      closure->parent_answer_entry_index];
  for (size_t i = 0; i < future->answer_entry_count; ++i) {
    struct B_AnswerFutureAnswerEntry *parent
      = &parent_entries[i];
    struct B_AnswerFutureAnswerEntry const *child
      = &future->answer_entries[i];
    B_ASSERT(parent->state == B_FUTURE_PENDING);
    B_ASSERT(parent->answer_vtable == child->answer_vtable);
    switch (child->state) {
    case B_FUTURE_PENDING:
      B_NYI();  // B_BUG();?
    case B_FUTURE_FAILED:
      parent->state = B_FUTURE_FAILED;
      parent->result.error = child->result.error;
      break;
    case B_FUTURE_RESOLVED:
      parent->state = B_FUTURE_RESOLVED;
      parent->result.error = child->result.error;
      if (!child->answer_vtable->replicate(
          child->result.answer,
          &parent->result.answer,
          e)) {
        B_NYI();
      }
      break;
    }
  }
  if (!b_answer_future_check_callbacks_(
      closure->parent_future, &(struct B_Error) {})) {
    B_NYI();
  }
  b_answer_future_release(closure->parent_future);
  return true;
}

B_WUR B_EXPORT_FUNC bool
b_answer_future_join(
    B_BORROW struct B_AnswerFuture *const *futures,
    size_t future_count,
    B_OUT_TRANSFER struct B_AnswerFuture **out,
    B_OUT struct B_Error *e) {
  B_PRECONDITION(futures);
  B_PRECONDITION(future_count > 0);
  B_OUT_PARAMETER(out);
  B_OUT_PARAMETER(e);

  struct B_AnswerFuture *future = NULL;

  size_t answer_entry_count = 0;
  for (size_t i = 0; i < future_count; ++i) {
    // TODO(strager): Check for overflow.
    answer_entry_count += futures[i]->answer_entry_count;
  }

  // TODO(strager): It'd be nice if we could allocate all
  // closures and the future in one heap allocation.
  if (!b_allocate(
      b_answer_future_size_(answer_entry_count),
      (void **) &future,
      e)) {
    future = NULL;
    goto fail;
  }
  b_answer_future_initialize_(future, answer_entry_count);
  b_answer_future_dump_(
    future,
    __builtin_return_address(0),
    "b_answer_future_join");
  // Add to each child future a callback which propagates
  // answers to the parent.
  size_t entry_index = 0;
  for (size_t i = 0; i < future_count; ++i) {
    struct B_AnswerFutureJoinChildClosure_ closure = {
      .parent_future = future,
      .parent_answer_entry_index = entry_index,
    };
    for (
        size_t j = 0;
        j < futures[i]->answer_entry_count;
        ++j) {
      future->answer_entries[entry_index + j].answer_vtable
        = futures[i]->answer_entries[j].answer_vtable;
    }
    // We must retain before adding the callback, because
    // the callback may be called immediately.
    b_answer_future_retain(future);
    if (!b_answer_future_add_callback(
        futures[i],
        b_answer_future_join_child_callback_,
        &closure,
        sizeof(closure),
        e)) {
      b_answer_future_release(future);
      goto fail;
    }
    entry_index += futures[i]->answer_entry_count;
  }
  B_ASSERT(entry_index == answer_entry_count);
  *out = future;
  return true;

fail:
  if (future) {
    B_NYI();
  }
  return false;
}

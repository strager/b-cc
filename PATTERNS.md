# Design Patterns

This document describes design patterns used throughout the
b codebase.

## Virtual Tables

To enable runtime polymorphism, b uses *virtual tables*
(a.k.a. *vtables*), i.e. structures containing function
pointers.  Unlike languages like C++ and Java, vtables in b
are generally passed into functions which need to use them,
rather than attached to object instances.  In fact, b's
vtable design was taken directly from the Glasgow Haskell
Compiler's typeclass dictionary design.

A function within a vtable is called a *virtual function*.

Virtual table structures are suffixed with `VTable`.  Types
associated with vtables in an object-oriented manner are
prefixed with `Any`.  For example, the vtable
`B_QuestionVTable` contains functions for operating on
instances of `B_AnyQuestion`.

Why structure vtables this way?

* Associated types.  One vtable can refer to another.  For
  example, for a given `B_Question` vtable there is an
  associated `B_Answer` vtable.  These must be synchronized:
  answering a question should always result in the same
  type of answer.

* Safety.  Implementing value equality is more safe as there
  is only one vtable present when comparing, not two.  You
  cannot accidentally compare two values with a different
  comparison function by just changing the order of
  arguments.  Compare:

        bool
        values_equal(Value *a, Value *b) {
            if (a->vtable != b->vtable) {
                // return false?  error?
                // ignore and compromise correctness?
            }
            return a->vtable->equals(a, b);
        }

        values_equal(foo_a, foo_b);

  with:

        bool
        values_equal(ValueVTable *vtable, AnyValue *a, AnyValue *b) {
            return vtable->equals(a, b);
        }

        values_equal(foo_vtable, foo_a, foo_b);

  In the latter case, the wrapper function isn't even
  necessary:

        foo_vtable->equals(foo_a, foo_b);

* Terseness.  See example under 'Safety'.

* Power.  By allowing a vtable to be used without an
  attached object instance, expressing serialization and
  deserialization functions is more natural:

        struct SerializableVTable {
            void (*serialize)(const AnySerializable *, Writer *);
            AnySerializable *(*deserialize)(Reader *);
        };

  This vtable cannot be expressed in C++ or Java without a
  separate class.

* Separation of concerns.  A value does not need to be tied
  to a particular vtable.  If there are multiple ways to
  serialize or deserialize a value, or replicate and
  deallocate a value, or order two values, different vtables
  can be used.

## Virtual Replication and Deallocation

Sometimes, values need to be copied or persisted in a
polymorphic way.  Sometimes, values are no longer needed but
the creator of a value is far from its final reference.  b
uses one or two virtual functions, depending upon semantic
requirements: replication (virtual function `replicate`) and
deallocation (virtual function `deallocate`).

Replication is conceptually identical to cloning or copying.
Replication of a C string can be performed using the BSD
`strdup` function, for example.  However, replication does
not need to copy or allocate memory at all.  Replication
must only follow the following law:

All other operations on the replicated value must be
independent of the original value.  That is, mutations or
deallocation of the original value must not affect the
semantics of the replicated value, and mutations or
deallocation of the replicated value must not affect the
semantics of the original value.

Example implementations of replication and deallocation:

* Copying, where replication allocates a new value and
  copies owned objects, and deallocation frees owned objects
  and frees the value.

* Reference counting, where the value is immutable,
  replication increments the reference counter, and
  deallocation decrements it.

* Copy-on-write, a union between reference counting and
  copying, where reference counting is used before mutation
  and copying replication occurs upon mutation.

* No-op.  There are many possible reasons to have no-op
  replication and/or deallocation:

  * The value is stored for the lifetime of the application,
    is immutable, and has a no-op deallocator.  For example,
    a constant, static C string.

  * The value is in a memory pool and is immutable, and the
    memory pool will be cleared outside the scope of any
    retainers.

  * The value fits within an `intptr_t`, in which case the
    value is immutable and has a no-op deallocator.

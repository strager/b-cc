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

## Virtual Replication and Deallocation

Sometimes, values need to be copied or persisted in a
polymorphic way.  Sometimes, values are no longer needed but
the creator of a value is far from its final reference.  b
uses one or two virtual functions, depending upon semantic
requirements: replication (virtual function `replicate`) and
deallocation (virtual function `deallocate`).

Replication is conceptually identially to cloning or
copying.  Replication of a C string can be performed using
the BSD 'strdup' function, for example.  However,
replication does not need to copy or allocate memory at all.
Replication must only follow the following law:

All other operations on the replicated value must be
independent of the original value.  That is, mutations or
deallocation of the original value must not affect the
semantics of the replicated value, and mutations or
deallocation of the replicated value must not affect the
semantics of the original value.

Examples implementations of replication and deallocation:

* Copying, where replication allocates a new value and
  copies owned objects and deallocation frees owned objects
  and frees the value.

* Reference counting, where the value is immutable, and
  replication increments the reference counter and
  dealloation decrements it.

* No-op.  There are many possible reasons to have no-op
  replication and/or deallocation:

  * The value is stored for the livetime of the application,
    is immutable, and has a no-op deallocator.  For example,
    a constant, static C string.

  * The value is in a memory pool and is immutable, and the
    memory pool will be cleared outside the scope of any
    retainers.

  * The value fits within an `intptr_t`, in which case the
    value is immutable and has a no-op deallocator.

#pragma once

#include <B/Attributes.h>

#include <Python.h>
#include <stdbool.h>

struct B_ByteSink;
struct B_PyByteSink;

#if defined(__cplusplus)
extern "C" {
#endif

B_WUR B_FUNC bool
b_py_byte_sink_init(
    B_BORROW PyObject *module);

B_WUR B_FUNC B_OUT_BORROW struct B_PyByteSink *
b_py_byte_sink(
    B_BORROW PyObject *);

B_WUR B_FUNC B_OUT_TRANSFER struct B_PyByteSink *
b_py_byte_sink_from_pointer(
    B_TRANSFER struct B_ByteSink *);

#if defined(__cplusplus)
}
#endif

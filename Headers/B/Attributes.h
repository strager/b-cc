#pragma once

// Calling convention and export attributes.
// TODO(strager)
#define B_FUNC_CDECL
#define B_FUNC
#define B_EXPORT_
#define B_EXPORT_FUNC B_FUNC B_EXPORT_
#define B_EXPORT_FUNC_CDECL B_FUNC_CDECL B_EXPORT_

// Abbreviations for GNU static analysis attributes.
// TODO(strager)
#define B_WUR

// Ownership annotations.
// TODO(strager)
#define B_BORROW
#define B_BORROW_OPTIONAL
#define B_IN_OUT
#define B_OPTIONAL
#define B_OPTIONAL_OUT_TRANSFER
#define B_OUT
#define B_OUT_BORROW
#define B_OUT_TRANSFER
#define B_TRANSFER

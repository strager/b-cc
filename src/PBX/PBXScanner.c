#include <B/Assert.h>
#include <B/Error.h>
#include <B/PBX/PBXScanner.h>

#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct B_PBXScanner {
    uint8_t const *data;
    size_t data_size;

    size_t offset;
    size_t token_start_offset;
};

static B_FUNC
scan_value_(
        struct B_PBXScanner *,
        struct B_PBXValueVisitor *,
        struct B_ErrorHandler const *);

static B_FUNC
scan_dict_(
        struct B_PBXScanner *,
        struct B_PBXValueVisitor *,
        struct B_ErrorHandler const *);

static B_FUNC
scan_dict_key_(
        struct B_PBXScanner *,
        struct B_PBXValueVisitor *,
        struct B_ErrorHandler const *);

static B_FUNC
scan_array_(
        struct B_PBXScanner *,
        struct B_PBXValueVisitor *,
        struct B_ErrorHandler const *);

static B_FUNC
scan_quoted_string_(
        struct B_PBXScanner *,
        struct B_PBXValueVisitor *,
        struct B_ErrorHandler const *);

static B_FUNC
quoted_string_end_(
        struct B_PBXScanner const *,
        size_t *end_offset,
        struct B_ErrorHandler const *);

static B_FUNC
scan_unquoted_string_(
        struct B_PBXScanner *,
        struct B_PBXValueVisitor *,
        struct B_ErrorHandler const *);

static void
unquoted_string_end_(
        struct B_PBXScanner const *,
        size_t *end_offset);

static B_FUNC
skip_silence_(
        struct B_PBXScanner *,
        struct B_ErrorHandler const *);

static B_FUNC
skip_silence_then_byte_(
        struct B_PBXScanner *,
        uint8_t byte,
        struct B_ErrorHandler const *);

static void
unexpected_(
        struct B_PBXScanner const *,
        struct B_ErrorHandler const *);

static bool
eof_(
        struct B_PBXScanner const *);

static bool
is_whitespace_(
        struct B_PBXScanner const *,
        B_OUT size_t *size);

B_EXPORT_FUNC
b_pbx_scan_value(
        uint8_t const *data,
        size_t data_size,
        struct B_PBXValueVisitor *visitor,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, data);
    B_CHECK_PRECONDITION(eh, visitor);

    struct B_PBXScanner scanner = {
        .data = data,
        .data_size = data_size,
        .offset = 0,
        .token_start_offset = 0,
    };

    if (!skip_silence_(&scanner, eh)) {
        return false;
    }
    if (!scan_value_(&scanner, visitor, eh)) {
        return false;
    }
    if (!skip_silence_(&scanner, eh)) {
        return false;
    }
    if (!eof_(&scanner)) {
        unexpected_(&scanner, eh);
        return false;
    }

    return true;
}

B_EXPORT_FUNC
b_pbx_scanner_offset(
        struct B_PBXScanner *scanner,
        B_OUT size_t *out,
        struct B_ErrorHandler const *eh) {
    B_CHECK_PRECONDITION(eh, scanner);
    B_CHECK_PRECONDITION(eh, out);

    *out = scanner->token_start_offset;
    return true;
}

static B_FUNC
scan_value_(
        struct B_PBXScanner *scanner,
        struct B_PBXValueVisitor *visitor,
        struct B_ErrorHandler const *eh) {
    B_ASSERT(scanner);

    if (eof_(scanner)) {
        unexpected_(scanner, eh);
        return false;
    }
    uint8_t byte = scanner->data[scanner->offset];
    switch (byte) {
    case '{':
        return scan_dict_(scanner, visitor, eh);
    case '(':
        return scan_array_(scanner, visitor, eh);
    case '"':
        return scan_quoted_string_(scanner, visitor, eh);

    case ' ':
    case '\t':
    case '\n':
        // Caller should have called skip_silence_.
        B_BUG();
        return false;

    case '/':
        // '/' starts a comment, but also can start a string
        // literal.
        // FALL THROUGH
    default:
        // Assume anything else begins an unquoted string.
        B_ASSERT(!is_whitespace_(scanner, NULL));
        return scan_unquoted_string_(scanner, visitor, eh);
    }
}

static B_FUNC
scan_dict_(
        struct B_PBXScanner *scanner,
        struct B_PBXValueVisitor *visitor,
        struct B_ErrorHandler const *eh) {
    B_ASSERT(scanner);
    B_ASSERT(!eof_(scanner));
    B_ASSERT(scanner->data[scanner->offset] == '{');

    bool recurse = false;
    if (visitor) {
        scanner->token_start_offset = scanner->offset;
        if (!visitor->visit_dict_begin(
                visitor,
                scanner,
                &recurse,
                eh)) {
            return false;
        }
    }

    // Skip '{' (beginning of dict).
    scanner->offset += 1;
    if (!skip_silence_(scanner, eh)) {
        return false;
    }

    // TODO(strager): Perform fast brace matching if
    // !recurse.
    struct B_PBXValueVisitor *v = recurse ? visitor : NULL;
    for (;;) {
        // Check for '}' (end of dict).
        if (eof_(scanner)) {
            unexpected_(scanner, eh);
            return false;
        }
        if (scanner->data[scanner->offset] == '}') {
            // End of dict.
            break;
        }

        if (!scan_dict_key_(scanner, v, eh)) {
            return false;
        }
        if (!skip_silence_then_byte_(scanner, '=', eh)) {
            return false;
        }
        if (!scan_value_(scanner, v, eh)) {
            return false;
        }
        if (!skip_silence_then_byte_(scanner, ';', eh)) {
            return false;
        }
    }

    // Skip '}'.
    B_ASSERT(scanner->data[scanner->offset] == '}');
    scanner->offset += 1;

    if (visitor) {
        scanner->token_start_offset = scanner->offset;
        if (!visitor->visit_dict_end(
                visitor,
                scanner,
                eh)) {
            return false;
        }
    }

    return true;
}

static B_FUNC
scan_dict_key_(
        struct B_PBXScanner *scanner,
        struct B_PBXValueVisitor *visitor,
        struct B_ErrorHandler const *eh) {
    B_ASSERT(scanner);

    size_t start_offset = scanner->offset;
    size_t end_offset;
    unquoted_string_end_(scanner, &end_offset);
    B_ASSERT(scanner->offset == start_offset);

    if (visitor) {
        scanner->token_start_offset = start_offset;
        if (!visitor->visit_dict_key(
                visitor,
                scanner,
                &scanner->data[start_offset],
                end_offset - start_offset,
                eh)) {
            return false;
        }
    }

    scanner->offset = end_offset;
    return true;
}

static B_FUNC
scan_array_(
        struct B_PBXScanner *scanner,
        struct B_PBXValueVisitor *visitor,
        struct B_ErrorHandler const *eh) {
    B_ASSERT(scanner);
    B_ASSERT(!eof_(scanner));
    B_ASSERT(scanner->data[scanner->offset] == '(');

    bool recurse = false;
    if (visitor) {
        scanner->token_start_offset = scanner->offset;
        if (!visitor->visit_array_begin(
                visitor,
                scanner,
                &recurse,
                eh)) {
            return false;
        }
    }

    // Skip '(' (beginning of array).
    scanner->offset += 1;
    if (!skip_silence_(scanner, eh)) {
        return false;
    }

    // TODO(strager): Perform fast brace matching if
    // !recurse.
    struct B_PBXValueVisitor *v = recurse ? visitor : NULL;
    for (;;) {
        // Check for ')' (end of array).
        if (eof_(scanner)) {
            unexpected_(scanner, eh);
            return false;
        }
        if (scanner->data[scanner->offset] == ')') {
            // End of array.
            break;
        }

        if (!scan_value_(scanner, v, eh)) {
            return false;
        }
        if (!skip_silence_then_byte_(scanner, ',', eh)) {
            return false;
        }
    }

    // Skip ')'.
    B_ASSERT(scanner->data[scanner->offset] == ')');
    scanner->offset += 1;

    if (visitor) {
        scanner->token_start_offset = scanner->offset;
        if (!visitor->visit_array_end(
                visitor,
                scanner,
                eh)) {
            return false;
        }
    }

    return true;
}

static B_FUNC
scan_quoted_string_(
        struct B_PBXScanner *scanner,
        struct B_PBXValueVisitor *visitor,
        struct B_ErrorHandler const *eh) {
    B_ASSERT(scanner);

    size_t start_offset = scanner->offset;
    size_t end_offset;
    if (!quoted_string_end_(scanner, &end_offset, eh)) {
        return false;
    }
    B_ASSERT(scanner->offset == start_offset);

    if (visitor) {
        scanner->token_start_offset = start_offset;
        if (!visitor->visit_quoted_string(
                visitor,
                scanner,
                &scanner->data[start_offset],
                end_offset - start_offset,
                eh)) {
            return false;
        }
    }

    scanner->offset = end_offset;
    return true;
}

static B_FUNC
quoted_string_end_(
        struct B_PBXScanner const *scanner,
        size_t *end_offset,
        struct B_ErrorHandler const *eh) {
    B_ASSERT(scanner);
    B_ASSERT(end_offset);

    struct B_PBXScanner tmp_scanner = *scanner;
    B_ASSERT(tmp_scanner.data[tmp_scanner.offset] == '"');
    tmp_scanner.offset += 1;

    while (true) {
        if (eof_(&tmp_scanner)) {
            // FIXME(strager): Wrong scanner?
            unexpected_(&tmp_scanner, eh);
            return false;
        }

        uint8_t data = tmp_scanner.data[tmp_scanner.offset];
        if (data == '\\') {
            // Skip slash.
            tmp_scanner.offset += 1;

            // Skip escaped character.
            // FIXME(strager): Do we need to handle octal
            // digits and such?
            if (eof_(&tmp_scanner)) {
                // FIXME(strager): Wrong scanner?
                unexpected_(&tmp_scanner, eh);
                return false;
            }
            tmp_scanner.offset += 1;
            continue;
        } else if (data == '"') {
            break;
        }

        tmp_scanner.offset += 1;
    }

    B_ASSERT(tmp_scanner.data[tmp_scanner.offset] == '"');
    tmp_scanner.offset += 1;
    *end_offset = tmp_scanner.offset;
    return true;
}

static B_FUNC
scan_unquoted_string_(
        struct B_PBXScanner *scanner,
        struct B_PBXValueVisitor *visitor,
        struct B_ErrorHandler const *eh) {
    B_ASSERT(scanner);

    size_t start_offset = scanner->offset;
    size_t end_offset;
    unquoted_string_end_(scanner, &end_offset);
    B_ASSERT(scanner->offset == start_offset);

    if (visitor) {
        scanner->token_start_offset = start_offset;
        if (!visitor->visit_string(
                visitor,
                scanner,
                &scanner->data[start_offset],
                end_offset - start_offset,
                eh)) {
            return false;
        }
    }

    scanner->offset = end_offset;
    return true;
}

static void
unquoted_string_end_(
        struct B_PBXScanner const *scanner,
        size_t *end_offset) {
    B_ASSERT(scanner);
    B_ASSERT(end_offset);

    struct B_PBXScanner tmp_scanner = *scanner;
    while (!eof_(&tmp_scanner)) {
        switch (tmp_scanner.data[tmp_scanner.offset]) {
        case '=':
        case ';':
        case ',':
        case '}':
        case ')':
            goto done;

        case '"':
            // FIXME(strager): We should call unexpected_
            // here.
            B_BUG();

        case ' ':
        case '\t':
        case '\n':
        default:
            if (is_whitespace_(&tmp_scanner, NULL)) {
                goto done;
            }
            break;
        }

        tmp_scanner.offset += 1;
    }

done:
    *end_offset = tmp_scanner.offset;
}

static void
skip_line_comment_(
        struct B_PBXScanner *scanner) {
    B_ASSERT(scanner);
    B_ASSERT(scanner->data[scanner->offset + 0] == '/');
    B_ASSERT(scanner->data[scanner->offset + 1] == '/');

    scanner->offset += 2;
    while (!eof_(scanner)
                && scanner->data[scanner->offset] != '\n') {
        scanner->offset += 1;
    }
}

static B_FUNC
skip_block_comment_(
        struct B_PBXScanner *scanner,
        struct B_ErrorHandler const *eh) {
    B_ASSERT(scanner);
    B_ASSERT(scanner->data[scanner->offset + 0] == '/');
    B_ASSERT(scanner->data[scanner->offset + 1] == '*');

    scanner->offset += 2;
    for (;;) {
        if (eof_(scanner)) {
            unexpected_(scanner, eh);
            return false;
        }
        if (scanner->offset + 1 >= scanner->data_size) {
            unexpected_(scanner, eh);
            return false;
        }

        if (scanner->data[scanner->offset + 0] == '*'
                && scanner->data[scanner->offset + 1]
                    == '/') {
            scanner->offset += 2;
            return true;
        }

        scanner->offset += 1;
    }
}

static B_FUNC
skip_silence_(
        struct B_PBXScanner *scanner,
        struct B_ErrorHandler const *eh) {
    B_ASSERT(scanner);

    while (!eof_(scanner)) {
        size_t whitespace_size;
        if (is_whitespace_(scanner, &whitespace_size)) {
            // Skip whitespace.
            scanner->offset += whitespace_size;
            continue;
        }

        uint8_t byte = scanner->data[scanner->offset];
        if (byte != '/') {
            // Not a comment, and not whitespace (above).
            break;
        }

        if (scanner->offset + 1 >= scanner->data_size) {
            // Not a comment.
            break;
        }

        uint8_t next_byte
            = scanner->data[scanner->offset + 1];
        if (next_byte == '/') {
            // // (C++-style) comment.
            skip_line_comment_(scanner);
        } else if (next_byte == '*') {
            // /* */ (C89-style) comment.
            if (!skip_block_comment_(scanner, eh)) {
                return false;
            }
        } else {
            // Not a comment.
            break;
        }
    }

    return true;
}

static B_FUNC
skip_silence_then_byte_(
        struct B_PBXScanner *scanner,
        uint8_t byte,
        struct B_ErrorHandler const *eh) {
    if (!skip_silence_(scanner, eh)) {
        return false;
    }
    if (eof_(scanner)) {
        unexpected_(scanner, eh);
        return false;
    }
    if (scanner->data[scanner->offset] != byte) {
        unexpected_(scanner, eh);
        return false;
    }
    scanner->offset += 1;
    if (!skip_silence_(scanner, eh)) {
        return false;
    }
    return true;
}

static void
unexpected_(
        struct B_PBXScanner const *scanner,
        struct B_ErrorHandler const *eh) {
    B_ASSERT(scanner);
    (void) eh;
    B_NYI();
}

static bool
eof_(
        struct B_PBXScanner const *scanner) {
    return scanner->offset >= scanner->data_size;
}

static bool
is_whitespace_(
        struct B_PBXScanner const *scanner,
        B_OUT size_t *size) {
    if (eof_(scanner)) {
        return false;
    }

    // FIXME(strager): This is almost certainly incorrect.
    if (isspace(scanner->data[scanner->offset])) {
        if (size) {
            *size = 1;
        }
        return true;
    }

    // TODO(strager): Handle multi-byte whitespace.

    return false;
}

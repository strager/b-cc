#ifndef FILERULE_H_D702399A_1532_4FFC_958E_E97F0147A981
#define FILERULE_H_D702399A_1532_4FFC_958E_E97F0147A981

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct B_AnyRule;
struct B_BuildContext;
struct B_Exception;
struct B_RuleVTable;

typedef void (*B_FileRuleCallback)(
    struct B_BuildContext *,
    const char *file_path,
    struct B_Exception **);

struct B_AnyRule *
b_file_rule_allocate();

void
b_file_rule_deallocate(
    struct B_AnyRule *);

void
b_file_rule_add(
    struct B_AnyRule *,
    const char *,
    B_FileRuleCallback);

void
b_file_rule_add_many(
    struct B_AnyRule *,
    const char **,
    size_t count,
    B_FileRuleCallback);

const struct B_RuleVTable *
b_file_rule_vtable();

#ifdef __cplusplus
}
#endif

#endif

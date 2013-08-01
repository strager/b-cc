import b

import ctypes
import os
import sys

c_object_files = [
    'example/Example.c.o',
    'lib/Answer.c.o',
    'lib/BuildContext.c.o',
    'lib/Database.c.o',
    'lib/Exception.c.o',
    'lib/FileQuestion.c.o',
    'lib/FileRule.c.o',
    'lib/Portable.c.o',
    'lib/Question.c.o',
    'lib/Rule.c.o',
    'lib/RuleQueryList.c.o',
    'lib/UUID.c.o',
]

cc_object_files = [
    'lib/DatabaseInMemory.cc.o',
]

object_files = c_object_files + cc_object_files

output_file = 'b-cc-example-in-py'

def run_command(args):
    import subprocess
    print str.join(' ', args)
    subprocess.call(args, shell=False)

def drop_extension(path):
    return os.path.splitext(path)[0]

def run_c_compile(ctx, object_path, ex):
    c_path = drop_extension(object_path)
    run_command(['clang', '-Ilib', '-o', object_path, '-c', c_path])

def run_cc_compile(ctx, object_path, ex):
    cc_path = drop_extension(object_path)
    run_command(['clang++', '-Ilib', '-std=c++11', '-stdlib=libc++',
        '-o', object_path, '-c', cc_path])

def depend_upon_object_files(ctx, ex):
    count = len(object_files)
    question_vtable = b.lib.b_file_question_vtable()

    questions = [
            b.lib.b_file_question_allocate(f)
            for f in object_files]
    question_vtables = [question_vtable] * count

    b.lib.b_build_context_need(
        ctx,
        (ctypes.c_void_p * count)(*questions),
        (ctypes.c_void_p * count)(*question_vtables),
        count,
        ex)

    for q in questions:
        b.lib.b_file_question_deallocate(q)

def run_cc_link(ctx, output_path, ex):
    depend_upon_object_files(ctx, ex)
    if ex.contents:
        return
    run_command(['clang++', '-stdlib=libc++',
        '-o', output_path] + object_files)

def main(argv):
    database = b.lib.b_database_in_memory_allocate()
    database_vtable = b.lib.b_database_in_memory_vtable()

    rule = b.lib.b_file_rule_allocate()
    rule_vtable = b.lib.b_file_rule_vtable()

    run_c_compile_callback = b.FileRuleCallback(run_c_compile)
    run_cc_compile_callback = b.FileRuleCallback(run_cc_compile)
    run_cc_link_callback = b.FileRuleCallback(run_cc_link)

    for f in c_object_files:
        b.lib.b_file_rule_add(rule, f, run_c_compile_callback)
    for f in cc_object_files:
        b.lib.b_file_rule_add(rule, f, run_cc_compile_callback)
    b.lib.b_file_rule_add(rule, output_file, run_cc_link_callback)

    question = b.lib.b_file_question_allocate(output_file)
    question_vtable = b.lib.b_file_question_vtable()

    ctx = b.lib.b_build_context_allocate(
        database,
        database_vtable,
        rule,
        rule_vtable)

    ex = ctypes.POINTER(b.BException)()

    b.lib.b_build_context_need_one(
        ctx,
        question,
        question_vtable,
        ctypes.byref(ex))

    if ex:
        print "Error occured: %s" % ex.contents.message
        return 1

    b.lib.b_build_context_deallocate(ctx)
    b.lib.b_file_rule_deallocate(rule)
    b.lib.b_database_in_memory_deallocate(database)

if __name__ == '__main__':
    sys.exit(main(sys.argv))

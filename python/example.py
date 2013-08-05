import os
import sys
sys.path.append(os.path.dirname(os.path.realpath(__file__)))

import b.easy as b

import ctypes

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
    'lib/Serialize.c.o',
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
    subprocess.check_call(args, shell=False)

def drop_extension(path):
    return os.path.splitext(path)[0]

def run_c_compile(ctx, object_path):
    c_path = drop_extension(object_path)
    run_command(['clang', '-Ilib', '-o', object_path, '-c', c_path])

def run_cc_compile(ctx, object_path):
    cc_path = drop_extension(object_path)
    run_command(['clang++', '-Ilib', '-std=c++11', '-stdlib=libc++',
        '-o', object_path, '-c', cc_path])

def run_cc_link(ctx, output_path):
    ctx.need([b.FileQuestion(obj) for obj in object_files])
    run_command(['clang++', '-stdlib=libc++',
        '-o', output_path] + object_files)

def main(argv):
    database = b.DatabaseInMemory()
    rule = b.FileRule()

    for f in c_object_files:
        rule.append(f, run_c_compile)
    for f in cc_object_files:
        rule.append(f, run_cc_compile)
    rule.append(output_file, run_cc_link)

    ctx = b.BuildContext(database, rule)
    ctx.need_one(b.FileQuestion(output_file))

if __name__ == '__main__':
    sys.exit(main(sys.argv))
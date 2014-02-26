out_dir := out

shared_extension := .dylib  # FIXME(strager)
b_lib_shared_file := $(out_dir)/libb$(shared_extension)

h_files := $(wildcard include/B/*.h)
c_files := $(wildcard src/*.c)
cc_files := $(wildcard src/*.cc)
o_files := \
	$(addprefix $(out_dir)/,$(c_files:.c=.c.o) $(cc_files:.cc=.cc.o))

c_cc_flags := -Iinclude -g -Wall -Wextra -pedantic -Werror
CFLAGS := $(c_cc_flags) -std=c11
CXXFLAGS := $(c_cc_flags) -std=c++11 -stdlib=libc++
LDFLAGS := $(c_cc_flags)

# Suppress: empty struct is a GNU extension
# TODO(strager): Remove need for this extension.
CFLAGS += -Wno-gnu

.PHONY: all
all: b examples

.PHONY: b
b: $(b_lib_shared_file)

.PHONY: examples
examples: ex1

.PHONY: ex1
ex1: $(out_dir)/ex1

$(out_dir):
	@mkdir -p $(out_dir)/src/

$(b_lib_shared_file): $(o_files) Makefile | $(out_dir)
	$(CXX) -shared -o $@ $(LDFLAGS) $(o_files)

$(out_dir)/%.c.o: %.c $(h_files) Makefile | $(out_dir)
	$(CC) -c -o $@ $(CFLAGS) $<

$(out_dir)/%.cc.o: %.cc $(h_files) Makefile | $(out_dir)
	$(CXX) -c -o $@ $(CXXFLAGS) $<

$(out_dir)/ex1: ex/1/main.cc $(h_files) $(b_lib_shared_file) Makefile | $(out_dir)
	$(CXX) -o $@ $(CXXFLAGS) $(b_lib_shared_file) ex/1/main.cc

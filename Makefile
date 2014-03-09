include configure.make

build_files := Makefile configure.make

out_dir := out
vendor_gtest := vendor/gtest-1.7.0

out_dirs := \
	$(out_dir)/src \
	$(out_dir)/test \
	$(out_dir)/$(vendor_gtest)/src

b_lib_shared_file := $(out_dir)/libb$(shared_extension)

h_files := $(wildcard include/B/*.h)
c_files := $(wildcard src/*.c)
cc_files := $(wildcard src/*.cc)
o_files := \
	$(addprefix $(out_dir)/,$(c_files:.c=.c.o) $(cc_files:.cc=.cc.o))

gtest_cc_files := \
	$(wildcard test/*.cc) \
	$(vendor_gtest)/src/gtest-all.cc \
	$(vendor_gtest)/src/gtest_main.cc
gtest_o_files := \
	$(addprefix $(out_dir)/,$(gtest_cc_files:.cc=.cc.o))

c_cc_flags := -Iinclude -g -Wall -Wextra -pedantic -Werror
CFLAGS += $(c_cc_flags) -std=c11
CXXFLAGS += $(c_cc_flags) -std=c++11
LDFLAGS += $(c_cc_flags)

# TODO(strager): Remove need for these extensions.
ifeq ($(cc_is_clang),1)
	# Suppress: empty struct is a GNU extension
	CFLAGS += -Wno-gnu
endif
ifeq ($(cc_is_gcc),1)
	# Suppress: struct has no members
	CFLAGS += -Wno-pedantic
endif

# On Darwin, libc++ is best.  On Linux, libc++ is uncommon,
# so assume libstdc++.
ifeq ($(cc_is_clang),1)
	ifeq ($(uname),Darwin)
		CXXFLAGS += -stdlib=libc++
		LDFLAGS += -stdlib=libc++
	endif
	ifeq ($(uname),Linux)
		CXXFLAGS += -stdlib=libstdc++
		LDLAGS += -stdlib=libstdc++
	endif
endif

ifeq ($(target_elf),1)
	CFLAGS += -fPIC
	CXXFLAGS += -fPIC
endif

ifeq ($(uname),Linux)
	LDFLAGS += -lpthread
endif

.PHONY: all
all: b examples gtest

.PHONY: test
# TODO(strager): Include test-ex1 once ex1 supports $CC and
# $CXX environment variables.
test: test-gtest

.PHONY: b
b: $(b_lib_shared_file)

.PHONY: examples
examples: ex1

.PHONY: ex1
ex1: $(out_dir)/ex1

.PHONY: gtest
gtest: $(out_dir)/test/gtest

.PHONY: test-ex1
test-ex1: ex1
	$(out_dir)/ex1

.PHONY: test-gtest
test-gtest: ex1
	$(out_dir)/test/gtest

.PHONY: clean
clean: | $(out_dirs)
	@rm -r $(out_dir)

$(out_dirs):
	@mkdir -p $(out_dirs)

$(b_lib_shared_file): $(o_files) $(build_files) | $(out_dirs)
	$(CXX) -shared -o $@ $(LDFLAGS) $(o_files)

$(out_dir)/test/%.cc.o: test/%.cc $(h_files) $(build_files) | $(out_dirs)
	$(CXX) -c -o $@ $(CXXFLAGS) "-I$(vendor_gtest)/include" $<

$(out_dir)/$(vendor_gtest)/%.cc.o: $(vendor_gtest)/%.cc $(build_files) | $(out_dirs)
	$(CXX) -c -o $@ $(CXXFLAGS) "-I$(vendor_gtest)/include" "-I$(vendor_gtest)" -Wno-missing-field-initializers $<

$(out_dir)/%.c.o: %.c $(h_files) $(build_files) | $(out_dirs)
	$(CC) -c -o $@ $(CFLAGS) $<

$(out_dir)/%.cc.o: %.cc $(h_files) $(build_files) | $(out_dirs)
	$(CXX) -c -o $@ $(CXXFLAGS) $<

$(out_dir)/ex1: ex/1/main.cc $(h_files) $(b_lib_shared_file) $(build_files) | $(out_dirs)
	$(CXX) -o $@ $(CXXFLAGS) $(b_lib_shared_file) ex/1/main.cc

$(out_dir)/test/gtest: $(gtest_o_files) $(h_files) $(b_lib_shared_file) $(build_files) | $(out_dirs)
	$(CXX) -o $@ $(LDFLAGS) $(b_lib_shared_file) $(gtest_o_files)

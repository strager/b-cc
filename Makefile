include configure.make

# For example: make test-gtest run_with=valgrind
run_with :=

# Set to 1 to compile and link vendor/sqlite-3.4.1/
# statically.  Set to non-1 to use the system sqlite3.
sqlite3_static := 1

build_files := Makefile configure.make

out_dir := out
vendor_gmock := vendor/gmock-1.7.0
vendor_sqlite3 := vendor/sqlite-3.8.4.1

out_dirs := \
	$(out_dir)/ex/1 \
	$(out_dir)/src \
	$(out_dir)/test \
	$(out_dir)/$(vendor_gmock)/gtest/src \
	$(out_dir)/$(vendor_gmock)/src \
	$(out_dir)/$(vendor_sqlite3)

b_lib_shared_file := $(out_dir)/libb$(shared_extension)

h_files := $(wildcard include/B/*.h)
c_files := $(wildcard src/*.c)
cc_files := $(wildcard src/*.cc)

ifeq ($(sqlite3_static),1)
	c_files += $(vendor_sqlite3)/sqlite3.c
	h_files += $(vendor_sqlite3)/sqlite3.h
endif

o_files := \
	$(addprefix $(out_dir)/,$(c_files:.c=.c.o) $(cc_files:.cc=.cc.o))

ex1_cc_files := $(wildcard ex/1/*.cc)
ex1_o_files := \
	$(addprefix $(out_dir)/,$(ex1_cc_files:.cc=.cc.o))

test_h_files := $(wildcard test/*.h)

gtest_cc_files := \
	$(wildcard test/*.cc) \
	$(vendor_gmock)/gtest/src/gtest-all.cc \
	$(vendor_gmock)/src/gmock-all.cc \
	$(vendor_gmock)/src/gmock_main.cc
gtest_o_files := \
	$(addprefix $(out_dir)/,$(gtest_cc_files:.cc=.cc.o))

c_cc_flags := -Iinclude -g -Wall -Wextra -pedantic -Werror
CFLAGS += $(c_cc_flags) -std=c11
CXXFLAGS += $(c_cc_flags) -std=c++11
LDFLAGS +=

ifeq ($(sqlite3_static),1)
	sqlite3_user_cflags := "-I$(vendor_sqlite3)"
else
	sqlite3_user_cflags :=
	LDFLAGS += -lsqlite3
endif

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
	ifeq ($(sqlite3_static),1)
		LDFLAGS += -ldl
	endif
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
ex1: $(out_dir)/ex/1/ex1

.PHONY: gtest
gtest: $(out_dir)/test/gtest

.PHONY: test-ex1
test-ex1: ex1
	$(run_with) $(out_dir)/ex/1/ex1

.PHONY: test-gtest
test-gtest: gtest
	$(run_with) $(out_dir)/test/gtest

.PHONY: clean
clean: | $(out_dirs)
	@rm -r $(out_dir)

$(out_dirs):
	@mkdir -p $(out_dirs)

$(b_lib_shared_file): $(o_files) $(build_files) | $(out_dirs)
	$(CXX) -shared -o $@ $(LDFLAGS) $(o_files)

$(out_dir)/test/%.cc.o: test/%.cc $(h_files) $(test_h_files) $(build_files) | $(out_dirs)
	$(CXX) -c -o $@ $(CXXFLAGS) "-I$(vendor_gmock)/include" "-I$(vendor_gmock)/gtest/include" $<

$(out_dir)/$(vendor_gmock)/gtest/%.cc.o: $(vendor_gmock)/gtest/%.cc $(build_files) | $(out_dirs)
	$(CXX) -c -o $@ $(CXXFLAGS) "-I$(vendor_gmock)/gtest/include" "-I$(vendor_gmock)/gtest" -Wno-missing-field-initializers $<

$(out_dir)/$(vendor_gmock)/%.cc.o: $(vendor_gmock)/%.cc $(build_files) | $(out_dirs)
	$(CXX) -c -o $@ $(CXXFLAGS) "-I$(vendor_gmock)/gtest/include" "-I$(vendor_gmock)/include" "-I$(vendor_gmock)" -Wno-missing-field-initializers $<

$(out_dir)/$(vendor_sqlite3)/%.c.o: $(vendor_sqlite3)/%.c $(build_files) | $(out_dirs)
	$(CC) -c -o $@ $(CFLAGS) -Wno-unused-variable $<

$(out_dir)/%.c.o: %.c $(h_files) $(build_files) | $(out_dirs)
	$(CC) -c -o $@ $(CFLAGS) $(sqlite3_user_cflags) $<

$(out_dir)/%.cc.o: %.cc $(h_files) $(build_files) | $(out_dirs)
	$(CXX) -c -o $@ $(CXXFLAGS) $(sqlite3_user_cflags) $<

$(out_dir)/ex/1/%.cc.o: ex/1/%.cc $(h_files) $(build_files) | $(out_dirs)
	$(CXX) -c -o $@ $(CXXFLAGS) $(sqlite3_user_cflags) $<

$(out_dir)/ex/1/ex1: $(ex1_o_files) $(h_files) $(b_lib_shared_file) $(build_files) | $(out_dirs)
	$(CXX) -o $@ $(ex1_o_files) $(b_lib_shared_file) $(LDFLAGS)

$(out_dir)/test/gtest: $(gtest_o_files) $(h_files) $(b_lib_shared_file) $(build_files) | $(out_dirs)
	$(CXX) -o $@ $(gtest_o_files) $(b_lib_shared_file) $(LDFLAGS)

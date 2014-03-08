include configure.make

build_files := Makefile configure.make

out_dir := out

b_lib_shared_file := $(out_dir)/libb$(shared_extension)

h_files := $(wildcard include/B/*.h)
c_files := $(wildcard src/*.c)
cc_files := $(wildcard src/*.cc)
o_files := \
	$(addprefix $(out_dir)/,$(c_files:.c=.c.o) $(cc_files:.cc=.cc.o))

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
	endif
	ifeq ($(uname),Linux)
		CXXFLAGS += -stdlib=libstdc++
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
all: b examples

.PHONY: b
b: $(b_lib_shared_file)

.PHONY: examples
examples: ex1

.PHONY: ex1
ex1: $(out_dir)/ex1

.PHONY: clean
clean: | $(out_dir)
	@rm -r $(out_dir)

$(out_dir):
	@mkdir -p $(out_dir)/src/

$(b_lib_shared_file): $(o_files) $(build_files) | $(out_dir)
	$(CXX) -shared -o $@ $(LDFLAGS) $(o_files)

$(out_dir)/%.c.o: %.c $(h_files) $(build_files) | $(out_dir)
	$(CC) -c -o $@ $(CFLAGS) $<

$(out_dir)/%.cc.o: %.cc $(h_files) $(build_files) | $(out_dir)
	$(CXX) -c -o $@ $(CXXFLAGS) $<

$(out_dir)/ex1: ex/1/main.cc $(h_files) $(b_lib_shared_file) $(build_files) | $(out_dir)
	$(CXX) -o $@ $(CXXFLAGS) $(b_lib_shared_file) ex/1/main.cc

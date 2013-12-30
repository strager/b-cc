VENDOR_GTEST := vendor/gtest-1.7.0

OUT_DIR := out
OUT_DIRS := \
	$(OUT_DIR)/lib/src \
	$(OUT_DIR)/lib/src/Internal \
	$(OUT_DIR)/example \
	$(OUT_DIR)/test \
	$(OUT_DIR)/$(VENDOR_GTEST)/src

LIB_H_FILES := $(wildcard lib/include/B/*.h) $(wildcard lib/include/B/Internal/*.h)
LIB_C_FILES := $(wildcard lib/src/*.c) $(wildcard lib/src/Internal/*.c)
LIB_CXX_FILES := $(wildcard lib/src/*.cc) $(wildcard lib/src/Internal/*.cc)
LIB_S_FILES := $(wildcard lib/src/Internal/*.S)

EXAMPLE_C_FILES := example/Example2.c

TEST_CXX_FILES := \
	$(wildcard test/*.cc) \
	$(VENDOR_GTEST)/src/gtest-all.cc \
	$(VENDOR_GTEST)/src/gtest_main.cc

BUILD_FILES := Makefile

LIB_O_FILES := \
	$(addprefix $(OUT_DIR)/,$(LIB_C_FILES:.c=.c.o)) \
	$(addprefix $(OUT_DIR)/,$(LIB_CXX_FILES:.cc=.cc.o)) \
	$(addprefix $(OUT_DIR)/,$(LIB_S_FILES:.S=.S.o))

EXAMPLE_O_FILES := \
	$(addprefix $(OUT_DIR)/,$(EXAMPLE_C_FILES:.c=.c.o))

TEST_O_FILES := \
	$(addprefix $(OUT_DIR)/,$(TEST_CXX_FILES:.cc=.cc.o))

UNAME := $(shell uname -s)
ifeq ($(strip $(UNAME)),Darwin)
SHARED_EXT := .dylib
else
SHARED_EXT := .so
endif

LIBS := -lzmq

LIB := $(OUT_DIR)/lib/libb$(SHARED_EXT)
EXAMPLE := $(OUT_DIR)/example/example
TEST := $(OUT_DIR)/test/test

WARNING_FLAGS := -Wall -Werror
CC_FLAGS := $(CFLAGS) $(WARNING_FLAGS) -g -std=c99
CXX_FLAGS := $(CFLAGS) $(CXXFLAGS) $(WARNING_FLAGS) -g -std=c++11 -stdlib=libc++
LD_FLAGS := $(CFLAGS) $(LDFLAGS) $(WARNING_FLAGS) -L$(OUT_DIR)/lib $(LIBS) -stdlib=libc++

.PHONY: all
all: $(LIB) $(EXAMPLE) $(TEST) test

.PHONY: clean
clean:
	rm -rf \
		$(OUT_DIR) \
		b-cc-example

.PHONY: haskell
haskell:
	$(MAKE) -C haskell

.PHONY: test
test: $(TEST)
	$(TEST)

$(LIB): $(LIB_O_FILES) $(BUILD_FILES) | $(OUT_DIRS)
	$(CXX) $(LD_FLAGS) -shared -o $@ $(LIB_O_FILES)

$(EXAMPLE): $(EXAMPLE_O_FILES) $(LIB) $(BUILD_FILES) | $(OUT_DIRS)
	$(CXX) $(LD_FLAGS) -lb -o $@ $(EXAMPLE_O_FILES)

$(TEST): $(TEST_O_FILES) $(LIB) $(BUILD_FILES) | $(OUT_DIRS)
	$(CXX) $(LD_FLAGS) -lb -o $@ $(TEST_O_FILES)

$(OUT_DIR)/$(VENDOR_GTEST)/%.cc.o: $(VENDOR_GTEST)/%.cc $(BUILD_FILES) | $(OUT_DIRS)
	$(CXX) $(CXX_FLAGS) "-I$(VENDOR_GTEST)/include" "-I$(VENDOR_GTEST)" -c -o $@ $<

$(OUT_DIR)/test/%.cc.o: test/%.cc $(BUILD_FILES) | $(OUT_DIRS)
	$(CXX) $(CXX_FLAGS) "-I$(VENDOR_GTEST)/include" -Ilib/include -c -o $@ $<

$(OUT_DIR)/%.c.o: %.c $(LIB_H_FILES) $(BUILD_FILES) | $(OUT_DIRS)
	$(CC) $(CC_FLAGS) -Ilib/include -c -o $@ $<

$(OUT_DIR)/%.cc.o: %.cc $(LIB_H_FILES) $(BUILD_FILES) | $(OUT_DIRS)
	$(CXX) $(CXX_FLAGS) -Ilib/include -c -o $@ $<

$(OUT_DIR)/%.S.o: %.S $(BUILD_FILES) | $(OUT_DIRS)
	$(CC) $(CC_FLAGS) -c -o $@ $<

$(OUT_DIRS):
	@mkdir -p $@

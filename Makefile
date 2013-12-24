OUT_DIR := out
OUT_DIRS := $(OUT_DIR)/lib $(OUT_DIR)/example

LIB_H_FILES := $(wildcard lib/*.h)
LIB_C_FILES := $(wildcard lib/*.c)
LIB_CXX_FILES := $(wildcard lib/*.cc)

EXAMPLE_C_FILES := example/Example2.c

BUILD_FILES := Makefile

LIB_O_FILES := \
	$(addprefix $(OUT_DIR)/,$(LIB_C_FILES:.c=.c.o)) \
	$(addprefix $(OUT_DIR)/,$(LIB_CXX_FILES:.cc=.cc.o))

EXAMPLE_O_FILES := \
	$(addprefix $(OUT_DIR)/,$(EXAMPLE_C_FILES:.c=.c.o))

UNAME := $(shell uname -s)
ifeq ($(strip $(UNAME)),Darwin)
SHARED_EXT := .dylib
else
SHARED_EXT := .so
endif

LIBS := -lzmq

LIB := $(OUT_DIR)/lib/libb$(SHARED_EXT)
EXAMPLE := $(OUT_DIR)/example/example

WARNING_FLAGS := -Wall -Werror
CC_FLAGS := $(CFLAGS) $(WARNING_FLAGS) -g -std=c99
CXX_FLAGS := $(CFLAGS) $(CXXFLAGS) $(WARNING_FLAGS) -g -std=c++11 -stdlib=libc++
LD_FLAGS := $(CFLAGS) $(LDFLAGS) $(WARNING_FLAGS) -L$(OUT_DIR)/lib $(LIBS) -stdlib=libc++

.PHONY: all
all: $(LIB) $(EXAMPLE)

.PHONY: clean
clean:
	rm -rf \
		$(OUT_DIR) \
		b-cc-example

.PHONY: haskell
haskell:
	$(MAKE) -C haskell

$(LIB): $(LIB_O_FILES) $(BUILD_FILES) | $(OUT_DIRS)
	$(CXX) $(LD_FLAGS) -shared -o $@ $(LIB_O_FILES)

$(EXAMPLE): $(EXAMPLE_O_FILES) $(LIB) $(BUILD_FILES) | $(OUT_DIRS)
	$(CXX) $(LD_FLAGS) -lb -o $@ $(EXAMPLE_O_FILES)

$(OUT_DIR)/%.c.o: %.c $(LIB_H_FILES) $(BUILD_FILES) | $(OUT_DIRS)
	$(CC) $(CC_FLAGS) -c -o $@ $<

$(OUT_DIR)/%.cc.o: %.cc $(LIB_H_FILES) $(BUILD_FILES) | $(OUT_DIRS)
	$(CXX) $(CXX_FLAGS) -c -o $@ $<

$(OUT_DIRS):
	@mkdir -p $@

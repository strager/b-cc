LIB_H_FILES := $(wildcard lib/*.h)
LIB_C_FILES := $(wildcard lib/*.c)
LIB_CXX_FILES := $(wildcard lib/*.cc)

EXAMPLE_C_FILES := example/Example.c

BUILD_FILES := Makefile

LIB_O_FILES := \
	$(LIB_C_FILES:.c=.c.o) \
	$(LIB_CXX_FILES:.cc=.cc.o)

EXAMPLE_O_FILES := \
	$(EXAMPLE_C_FILES:.c=.c.o)

UNAME := $(shell uname -s)
ifeq ($(strip $(UNAME)),Darwin)
SHARED_EXT := .dylib
else
SHARED_EXT := .so
endif

LIB := lib/libb$(SHARED_EXT)
EXAMPLE := example/example

WARNING_FLAGS := -Wall -Werror
CC_FLAGS := $(CFLAGS) $(WARNING_FLAGS) -Ilib -g
CXX_FLAGS := $(CXXFLAGS) $(WARNING_FLAGS) -Ilib -g -std=c++11 -stdlib=libc++
LD_FLAGS := $(LDFLAGS) $(WARNING_FLAGS) -Llib -stdlib=libc++

.PHONY: all
all: $(LIB) $(EXAMPLE)

.PHONY: clean
clean:
	rm -rf \
		$(LIB_O_FILES) $(EXAMPLE_O_FILES) \
		$(LIB) $(EXAMPLE) \
		b-cc-example

$(LIB): $(LIB_O_FILES) $(BUILD_FILES)
	$(CXX) $(LD_FLAGS) -shared -o $@ $(LIB_O_FILES)

$(EXAMPLE): $(EXAMPLE_O_FILES) $(LIB) $(BUILD_FILES)
	$(CXX) $(LD_FLAGS) -lb -o $@ $(EXAMPLE_O_FILES)

%.c.o: %.c $(LIB_H_FILES) $(BUILD_FILES)
	$(CC) $(CC_FLAGS) -c -o $@ $<

%.cc.o: %.cc $(LIB_H_FILES) $(BUILD_FILES)
	$(CXX) $(CXX_FLAGS) -c -o $@ $<

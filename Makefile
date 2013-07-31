H_FILES := $(wildcard *.h)
C_FILES := $(wildcard *.c)
CXX_FILES := $(wildcard *.cc)
BUILD_FILES := Makefile

O_FILES := \
	$(C_FILES:.c=.c.o) \
	$(CXX_FILES:.cc=.cc.o)

OUTPUT := b-cc

WARNING_FLAGS := -Wall -Werror
CC_FLAGS := $(CFLAGS) $(WARNING_FLAGS)
CXX_FLAGS := $(CXXFLAGS) $(WARNING_FLAGS) -std=c++11 -stdlib=libc++
LD_FLAGS := $(LDFLAGS) $(WARNING_FLAGS) -stdlib=libc++

.PHONY: all
all: $(OUTPUT)

.PHONY: clean
clean:
	rm -rf $(O_FILES) $(OUTPUT)

$(OUTPUT): $(O_FILES) $(BUILD_FILES)
	$(CXX) $(LD_FLAGS) -o $@ $(O_FILES)

%.c.o: %.c $(H_FILES) $(BUILD_FILES)
	$(CC) $(CC_FLAGS) -c -o $@ $<

%.cc.o: %.cc $(H_FILES) $(BUILD_FILES)
	$(CXX) $(CXX_FLAGS) -c -o $@ $<

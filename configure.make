target_elf := 0
target_macho := 0
target_pe := 0

cc_is_gcc := 0
cc_is_clang := 0

# HACK(strager)
uname := $(shell uname -s)
ifeq ($(strip $(uname)),Darwin)
	target_macho := 1
endif
ifeq ($(strip $(uname)),Linux)
	target_elf := 1
endif

ifneq ($(shell $(CC) --version | grep -c gcc),0)
	cc_is_gcc := 1
endif
ifneq ($(shell $(CC) --version | grep -c clang),0)
	cc_is_clang := 1
endif

shared_extension :=
ifeq ($(target_elf),1)
	shared_extension := .so
endif
ifeq ($(target_macho),1)
	shared_extension := .dylib
endif
ifeq ($(target_pe),1)
	shared_extension := .dll
endif

# vim: noet

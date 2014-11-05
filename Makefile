# For example: make test-gtest run_with=valgrind
run_with :=

# Set to 1 to compile and link vendor/sqlite-3.4.1/
# statically.  Set to non-1 to use the system sqlite3.
sqlite3_static := 1

# Set to 1 to enable tweaks for clang's static analysis
# tools.
clang_static_analysis := 0

# Make non-empty when using the clang-static-analysis
# target.
scan_build :=

out_dir := out

scan_build_flags := $(addprefix -enable-checker ,\
    alpha.core.BoolAssignment \
    alpha.core.CastSize \
    alpha.core.CastToStruct \
    alpha.core.FixedAddr \
    alpha.core.PointerSub \
    alpha.core.SizeofPtr \
    alpha.cplusplus.NewDeleteLeaks \
    alpha.cplusplus.VirtualCall \
    alpha.deadcode.IdempotentOperations \
    alpha.security.ArrayBound \
    alpha.security.ArrayBoundV2 \
    alpha.security.MallocOverflow \
    alpha.security.ReturnPtrRange \
    alpha.security.taint.TaintPropagation \
    alpha.unix.Chroot \
    alpha.unix.MallocWithAnnotations \
    alpha.unix.PthreadLock \
    alpha.unix.SimpleStream \
    alpha.unix.Stream \
    alpha.unix.cstring.BufferOverlap \
    alpha.unix.cstring.NotNullTerminated \
    alpha.unix.cstring.OutOfBounds)
# alpha.core.PointerArithm # Noisy in gtest macros.
# alpha.deadcode.UnreachableCode # Noisy false positives.

# List of all goals defined by this Makefile.
real_goals := \
	$(outdir)/.exists \
	$(outdir)/Makefile \
	all \
	build \
	clang-static-analysis \
	clean \
	configure \
	test

# Goals given but unrecognized by this Makefile.  Will be
# passed to child invocations of 'make'.
unknown_goals := $(filter-out $(real_goals),$(MAKECMDGOALS))

# Define a goal for every unknown goal which depends on
# 'build'.  This will cause 'build' to run (only once).
$(foreach goal,$(unknown_goals), \
$(goal): build)

.PHONY: all
all: build

.PHONY: clean
clean:
	@rm -rf $(out_dir)

.PHONY: configure
configure: $(out_dir)/.exists
	cd $(out_dir) && cmake \
		-G 'Unix Makefiles' \
		-DSQLITE3_STATIC=$(sqlite3_static) \
		-DSTATIC_ANALYSIS=$(clang_static_analysis) \
		$(CURDIR)

$(out_dir)/Makefile: $(out_dir)/.exists
	@$(MAKE) -f $(firstword $(MAKEFILE_LIST)) configure

.PHONY: build
build: $(out_dir)/Makefile
	@$(MAKE) -C $(out_dir) $(unknown_goals)

# TODO(strager): Pass down -j argument from Make to ctest.
.PHONY: test
test: build $(out_dir)/.exists
	cd $(out_dir) && B_RUN_TEST_PREFIX="$(run_with)" \
		ctest --output-on-failure

$(out_dir)/.exists:
	@mkdir -p $(out_dir)/
	@touch $(out_dir)/.exists

.PHONY: clang-static-analysis
clang-static-analysis:
ifeq ($(scan_build),)
	@echo "Please set the scan_build target: $(MAKE) scan_build=path/to/scan-build" >&2
	@echo "Download scan-build from http://clang-analyzer.llvm.org/" >&2
	@exit 1
endif
	$(scan_build) $(scan_build_flags) $(MAKE) clang_static_analysis=1

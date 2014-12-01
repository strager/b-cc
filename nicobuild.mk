real_goals := bar.mk __build
unknown_goals := $(filter-out $(real_goals),$(MAKECMDGOALS))
create_unknown_goal = $(goal): __build
$(foreach goal,$(unknown_goals),$(eval $(create_unknown_goal)))

.PHONY: __build
__build: __nicobuild.mk
	@$(MAKE) -f $(<) $(unknown_goals)

.PHONY: __nicobuild.mk
__nicobuild.mk:
	@{ \
	    set -e; \
	    set -u; \
	    echo "all:"; \
	    cat Nicobuild | while read -r line; do \
		case "$${line}" in \
		    "#"*) continue ;; \
		    "") continue ;; \
		    "exe "*:*) \
			line="$${line#exe }"; \
			name="$${line%%:*}"; \
			source_globs="$${line#*:}"; \
			for source in $${source_globs}; do \
			    printf '%s\n' "$${source}.o: $${source}"; \
			    if grep -q '\.c$$' <<<"$${source}"; then \
				printf '\t$$(CC) -c $$(CPPFLAGS) $$(CFLAGS) -o $$(@) $$(<)\n'; \
			    elif grep -q '\.c\(xx\|pp\|c\)$$' <<<"$${source}"; then \
				printf '\t$$(CXX) -c $$(CPPFLAGS) $$(CXXFLAGS) -o $$(@) $$(<)\n'; \
			    else \
				echo "Unknown language for $${source}" >&2; \
				exit 1; \
			    fi; \
			done; \
			printf '%s\n' "__tmp := $$(printf '%s ' $${source_globs})"; \
			printf '%s\n' "$${name}: \$$(foreach c,\$$(__tmp),\$$(c).o)"; \
			printf '\t$$(CXX) $$(CXXFLAGS) $$(LDFLAGS) -o $$(@) $$(^)\n'; \
			printf '%s\n' "all: $${name}"; \
			;; \
		    *"+="*) \
			printf '%s\n' "$${line}"; \
			;; \
		    *) \
			echo "parse error" >&2; \
			exit 1; \
			;; \
		esac; \
	    done; \
	} > $(@)

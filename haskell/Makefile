CABAL ?= cabal

.PHONY: all
all: build

.PHONY: configure
configure:
	$(CABAL) configure --extra-lib-dirs $(PWD)/../out/lib/

.PHONY: build
build:
	$(CABAL) build

MAX_VERSION := 9
PACKAGE_NAME := fs4m
MAX_PACKAGE := "$(HOME)/Documents/Max\ $(MAX_VERSION)/Packages/$(PACKAGE_NAME)"
SCRIPTS := source/scripts
BUILD := build
CONFIG = Release
THIRDPARTY = $(BUILD)/thirdparty
LIB = $(THIRDPARTY)/install/lib
DIST = $(BUILD)/dist/$(PACKAGE_NAME)
ARCH=$(shell uname -m)
DMG=$(PACKAGE_NAME)-$(VERSION)-$(ARCH).dmg
ENTITLEMENTS = source/scripts/entitlements.plist
VERSION=0.1.0

.PHONY: all build universal setup clean reset

all: build

build:
	@mkdir -p build && \
		cd build && \
		cmake -GXcode .. -DENABLE_HOMEBREW=ON && \
		cmake --build . --config '$(CONFIG)' && \
		cmake --install . --config '$(CONFIG)'

clean:
	@rm -rf \
		build \
		externals

reset:
	@rm -rf build externals

setup:
	@git submodule init
	@git submodule update
	@if ! [ -L "$(MAX_PACKAGE)" ]; then \
		ln -s "$(shell pwd)" "$(MAX_PACKAGE)" ; \
		echo "... symlink created" ; \
	else \
		echo "... symlink already exists" ; \
	fi


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

.PHONY: all build static universal thirdparty bundle setup clean reset

all: build

install_sf2:
	@./source/scripts/download_sf2.sh

install_fs:
	@./source/scripts/install_fluidsynth.sh

bundle: reset build install_sf2
	@mkdir -p build && \
		g++ -O3 -o build/bundler source/scripts/bundler.cpp && \
		build/bundler -od -b -x ./externals/fluidsynth~.mxo/Contents/MacOS/fluidsynth~ -d ./externals/fluidsynth~.mxo/Contents/Resources/libs/

build:
	@mkdir -p build && \
		cd build && \
		cmake -GXcode .. -DENABLE_HOMEBREW=ON && \
		cmake --build . --config '$(CONFIG)' && \
		cmake --install . --config '$(CONFIG)'

static: install_fs
	@mkdir -p build && \
		cd build && \
		cmake -GXcode .. -DENABLE_HOMEBREW=ON -DBUILD_STATIC=ON && \
		cmake --build . --config '$(CONFIG)' && \
		cmake --install . --config '$(CONFIG)'

thirdparty:
	@mkdir -p build && \
		cd build && \
		cmake -GXcode .. -DBUILD_THIRDPARTY=ON -DENABLE_HOMEBREW=ON && \
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


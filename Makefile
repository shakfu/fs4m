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
HOMEBREW=$(shell brew --prefix)
HOMEBREW_INCLUDES=$(HOMEBREW)/include
CLANG_TIDY=$(HOMEBREW)/opt/llvm/bin/clang-tidy
MAX_INCLUDES=source/max-sdk-base/c74support/max-includes
MSP_INCLUDES=source/max-sdk-base/c74support/msp-includes



.PHONY: all build static universal thirdparty bundle setup clean reset \
		tidy-fs4m tidy-fs tidy-fsm

all: build

install_sf2:
	@./source/scripts/install_sf2.sh

install_fs:
	@./source/scripts/install_fluidsynth.sh

bundle: reset build install_sf2
	@mkdir -p build && \
		python3 source/scripts/bundler.py -od \
			-d ./externals/fs~.mxo/Contents/libs/ \
			-p @loader_path/../libs/ \
			./externals/fs~.mxo/Contents/MacOS/fs~

build: reset
	@mkdir -p build && \
		cd build && \
		cmake -GXcode .. -DENABLE_HOMEBREW=ON && \
		cmake --build . --config '$(CONFIG)' && \
		cmake --install . --config '$(CONFIG)'

static: reset install_fs
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
	@rm -rf build/CMakeCache.txt externals

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

tidy-fs4m:
	@$(CLANG_TIDY) source/projects/fs4m_tilde/fs4m_tilde.c -- \
		-I $(MAX_INCLUDES) -I $(MSP_INCLUDES) -I $(HOMEBREW_INCLUDES)

tidy-fs:
	@$(CLANG_TIDY) source/projects/fs_tilde/fs_tilde.c -- \
		-I $(MAX_INCLUDES) -I $(MSP_INCLUDES) -I $(HOMEBREW_INCLUDES)

tidy-fsm:
	@$(CLANG_TIDY) source/projects/fs_tilde/fsm_tilde.c -- \
		-I $(MAX_INCLUDES) -I $(MSP_INCLUDES) -I $(HOMEBREW_INCLUDES)

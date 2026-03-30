#
# EZO I2C Probe Library - Makefile
#
# Targets:
#   make                    - native build
#   make debug              - native build with debug symbols, no optimisation
#   make dummy              - native build with fake sensors (no I2C hardware)
#   make arm64              - cross-compile for arm64 (native cross-toolchain)
#   make armhf              - cross-compile for armhf (native cross-toolchain)
#   make release            - build arm64 + armhf inside Docker container
#   make clean              - remove all binaries and object files
#   make clean-build        - remove object files only
#   make install            - install binary to /usr/local/bin
#
# Options (append to any target):
#   WITH_GPIOD=0            - disable GPIO support, removes libgpiod dependency
#   DOCKER_IMAGE=<name>     - override Docker image for release builds
#
# Examples:
#   make WITH_GPIOD=0
#   make arm64 WITH_GPIOD=0
#   make release WITH_GPIOD=0
#   make dummy WITH_GPIOD=0
#
# Docker setup (one time):
#   docker build -f Dockerfile.build -t ezo-buildenv .
#

# ─── Compilers ────────────────────────────────────────────────────────────────

CC        = gcc
CC_ARM64  = aarch64-linux-gnu-gcc
CC_ARMHF  = arm-linux-gnueabihf-gcc

# Docker image used for cross-compilation — override to share with another project:
#   make release DOCKER_IMAGE=aqualinkd-releasebin
DOCKER_IMAGE = ezo-buildenv

# ─── Options ─────────────────────────────────────────────────────────────────
#
# WITH_GPIOD — enable GPIO support via libgpiod
# Default: 1 (enabled)
# Override on command line: make WITH_GPIOD=0
#                           make arm64 WITH_GPIOD=0
#                           make release WITH_GPIOD=0
#
WITH_GPIOD ?= 1

# ─── Flags ────────────────────────────────────────────────────────────────────

# Base libs — always required
LIBS      = -lm

# Conditionally add libgpiod
ifeq ($(WITH_GPIOD), 1)
  LIBS    += -lgpiod
endif

# Standard build: optimised, all warnings
CFLAGS    = -Wall -O2 -std=c11

# Conditionally add -D WITH_GPIOD compile flag
ifeq ($(WITH_GPIOD), 1)
  CFLAGS  += -D WITH_GPIOD
endif

# Debug build: no optimisation, full debug info
DFLAGS    = -Wall -O0 -g -std=c11
ifeq ($(WITH_GPIOD), 1)
  DFLAGS  += -D WITH_GPIOD
endif

# ─── Directories ──────────────────────────────────────────────────────────────

SRC_DIR    = ./source
OBJ_DIR    = ./build
REL_DIR    = ./release

# Per-architecture object directories
OBJ_NATIVE = $(OBJ_DIR)/native
OBJ_ARM64  = $(OBJ_DIR)/arm64
OBJ_ARMHF  = $(OBJ_DIR)/armhf
OBJ_DEBUG  = $(OBJ_DIR)/debug

INCLUDES   = -I$(SRC_DIR)

# Create all build directories upfront at parse time.
# This avoids defining a $(REL_DIR) rule that would conflict with the
# 'release' phony Docker target (Make treats './release' and 'release' as
# the same target and warns about duplicate recipes).
$(shell mkdir -p $(REL_DIR) $(OBJ_NATIVE) $(OBJ_ARM64) $(OBJ_ARMHF) $(OBJ_DEBUG))

# ─── Sources ──────────────────────────────────────────────────────────────────
#
# Wildcard picks up all .c files in source/ automatically.
# As the project grows, just add new .c files to source/ — no Makefile changes needed.
#
# Optional modules are filtered out here when their feature flag is disabled.
#

SRCS      = $(wildcard $(SRC_DIR)/*.c)

ifeq ($(WITH_GPIOD), 0)
  SRCS    := $(filter-out $(SRC_DIR)/gpio.c, $(SRCS))
endif

# ─── Object files per architecture ────────────────────────────────────────────

OBJ_FILES_NATIVE = $(patsubst $(SRC_DIR)/%.c, $(OBJ_NATIVE)/%.o, $(SRCS))
OBJ_FILES_ARM64  = $(patsubst $(SRC_DIR)/%.c, $(OBJ_ARM64)/%.o,  $(SRCS))
OBJ_FILES_ARMHF  = $(patsubst $(SRC_DIR)/%.c, $(OBJ_ARMHF)/%.o,  $(SRCS))
OBJ_FILES_DEBUG  = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DEBUG)/%.o,  $(SRCS))

# ─── Output binaries ──────────────────────────────────────────────────────────

TARGET        = $(REL_DIR)/aquachemd
TARGET_ARM64  = $(REL_DIR)/aquachemd-arm64
TARGET_ARMHF  = $(REL_DIR)/aquachemd-armhf
TARGET_DEBUG  = $(REL_DIR)/aquachemd-debug

# ─── Phony targets ────────────────────────────────────────────────────────────

.PHONY: all debug arm64 armhf release _release_inside_container clean clean-build install

.DEFAULT_GOAL := all

# ─── Docker release target ────────────────────────────────────────────────────

# Build arm64 + armhf binaries inside the ezo-buildenv Docker container.
# The container must be built first:
#   docker build -f Dockerfile.build -t ezo-buildenv .
release:
	sudo docker run -it --mount type=bind,source=./,target=/build $(DOCKER_IMAGE) make _release_inside_container
	@echo "Release binaries built in $(REL_DIR)/"

# Called inside the Docker container — do not invoke directly
_release_inside_container: clean arm64 armhf

# ─── Native build ─────────────────────────────────────────────────────────────

all: $(TARGET)
	@echo "Built: $(TARGET)"

# Dummy sensor build — no I2C hardware required, safe on any machine
# Usage: make dummy && ./release/aquachemd
dummy: CFLAGS += -D DUMMY_SENSORS
dummy: $(TARGET)
	@echo "Built: $(TARGET) (** DUMMY SENSORS **)"

$(TARGET): $(OBJ_FILES_NATIVE)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^ $(LIBS)

$(OBJ_NATIVE)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

# ─── Debug build ──────────────────────────────────────────────────────────────

debug: $(TARGET_DEBUG)
	@echo "Built: $(TARGET_DEBUG) (** DEBUG **)"

$(TARGET_DEBUG): $(OBJ_FILES_DEBUG)
	$(CC) $(DFLAGS) $(INCLUDES) -o $@ $^ $(LIBS)

$(OBJ_DEBUG)/%.o: $(SRC_DIR)/%.c
	$(CC) $(DFLAGS) $(INCLUDES) -c -o $@ $<

# ─── arm64 cross-compile ──────────────────────────────────────────────────────

arm64: CC := $(CC_ARM64)
arm64: $(TARGET_ARM64)
	@echo "Built: $(TARGET_ARM64)"

$(TARGET_ARM64): $(OBJ_FILES_ARM64)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^ $(LIBS)

$(OBJ_ARM64)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

# ─── armhf cross-compile ──────────────────────────────────────────────────────

armhf: CC := $(CC_ARMHF)
armhf: $(TARGET_ARMHF)
	@echo "Built: $(TARGET_ARMHF)"

$(TARGET_ARMHF): $(OBJ_FILES_ARMHF)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^ $(LIBS)

$(OBJ_ARMHF)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

# ─── Install ──────────────────────────────────────────────────────────────────

install: $(TARGET)
	install -m 755 $(TARGET) /usr/local/bin/aquachemd
	@echo "Installed to /usr/local/bin/aquachemd"

# ─── Clean ────────────────────────────────────────────────────────────────────

# Remove object files only — keeps binaries
clean-build:
	$(RM) $(OBJ_FILES_NATIVE) $(OBJ_FILES_ARM64) $(OBJ_FILES_ARMHF) $(OBJ_FILES_DEBUG)

# Remove everything
clean: clean-build
	$(RM) $(TARGET) $(TARGET_ARM64) $(TARGET_ARMHF) $(TARGET_DEBUG)
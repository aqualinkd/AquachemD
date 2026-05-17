#
# AquachemD - Makefile
#
# Targets:
#   make                    - native build
#   make debug              - native build with debug symbols, no optimisation
#   make dummy              - native build with fake sensors (no I2C hardware)
#   make dummy-debug        - native build with fake sensors (no I2C hardware) & debug symbols
#   make arm64              - cross-compile for arm64 (native cross-toolchain)
#   make armhf              - cross-compile for armhf (native cross-toolchain)
#   make release            - build arm64 + armhf inside Docker container
#   make clean              - remove all binaries and object files
#   make clean-build        - remove object files only
#   make distclean          - remove everything (above + library objects)
#   make install            - install binary to /usr/local/bin
#
#  manually override.
#   make debug DFLAGS="-Wall -O0 -g -std=c11 -D USE_SYSTEMD -D DUMMY_SENSORS -D WITH_GPIOD -D MG_TLS=0 -D MG_ENABLE_SSI=0 -include deps/mongoose/aqd_mg_compat.h -I./source -I./deps/cJSON -I./deps/mongoose"
#
# Options (append to any target):
#   WITH_GPIOD=0            - disable GPIO support, removes libgpiod dependency
#   WITH_SYSTEMD=0          - disable systemd
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

# ─── Notes ─────────────────────────────────────────────────────────────────
#
# If we ever compile for container, we need to add this flag for compile.
# -D ACD_CONTAINER_BUILD
#

# ─── Options ─────────────────────────────────────────────────────────────────
#
#
WITH_GPIOD ?= 1
WITH_SYSTEMD ?= 1

# ─── Compilers ────────────────────────────────────────────────────────────────

CC        = gcc
CC_ARM64  = aarch64-linux-gnu-gcc
CC_ARMHF  = arm-linux-gnueabihf-gcc

# Docker image used for cross-compilation — override to share with another project:
#   make release DOCKER_IMAGE=aqualinkd-releasebin
DOCKER_IMAGE = aquachemd-releasebin

# ─── Flags ────────────────────────────────────────────────────────────────────

# Standard build: optimised, all warnings
# Note: CFLAGS already contains the -D flags now because of the += above
CFLAGS    += -Wall -O2 -std=c11
DFLAGS    += -Wall -O0 -g -std=c11

# Base libs — always required
LIBS      = -lpthread -lm

# Conditionally add systemd
ifeq ($(strip $(WITH_SYSTEMD)), 1)
  LIBS    += -lsystemd
  CFLAGS  += -D USE_SYSTEMD
  DFLAGS  += -D USE_SYSTEMD
endif

# Conditionally add libgpiod
ifeq ($(strip $(WITH_GPIOD)), 1)
  LIBS    += -lgpiod
  CFLAGS  += -D WITH_GPIOD
  DFLAGS  += -D WITH_GPIOD
endif

# Mongoose 7.19 flags
#CFLAGS += -D MG_TLS=2 #(2=MG_TLS_OPENSSL. 3=MG_TLS_BUILTIN) --or--  -DMG_TLS=MG_TLS_BUILTIN
CFLAGS += -D MG_TLS=0 -D MG_ENABLE_SSI=0

# Force-include the custom Mongoose additions in every file (ie make sure mongoose.c/.h include this file without modifying the source)
CFLAGS += -include deps/mongoose/aqd_mg_compat.h
DFLAGS += -include deps/mongoose/aqd_mg_compat.h

# ─── Directories ──────────────────────────────────────────────────────────────

SRC_DIR    = ./source
OBJ_DIR    = ./build
REL_DIR    = ./release
DIR_ARM64  = arm64
DIR_ARMHF  = armhf

# Per-architecture object directories
OBJ_NATIVE = $(OBJ_DIR)/native
OBJ_ARM64  = $(OBJ_DIR)/$(DIR_ARM64)
OBJ_ARMHF  = $(OBJ_DIR)/$(DIR_ARMHF)
OBJ_DEBUG  = $(OBJ_DIR)/debug

# Full paths for release binaries
REL_ARM64  = $(REL_DIR)/$(DIR_ARM64)
REL_ARMHF  = $(REL_DIR)/$(DIR_ARMHF)

#INCLUDES   = -I$(SRC_DIR)
INCLUDES  = -I$(SRC_DIR) -I./deps/cJSON -I./deps/mongoose

# Create all build directories upfront at parse time.
# This avoids defining a $(REL_DIR) rule that would conflict with the
# 'release' phony Docker target (Make treats './release' and 'release' as
# the same target and warns about duplicate recipes).
$(shell mkdir -p $(REL_DIR) $(REL_ARM64) $(REL_ARMHF) $(OBJ_NATIVE) $(OBJ_ARM64) $(OBJ_ARMHF) $(OBJ_DEBUG))


# ─── Dependencies & Sources ──────────────────────────────────────────────────

# Add new dependency folders here and everything below updates automatically
DEP_ROOTS = deps/cJSON deps/mongoose

# Automatic VPATH (No spaces after colons)
vpath %.c $(SRC_DIR):$(subst  ,:,$(DEP_ROOTS))

# Finds all .c files in source/ and all .c files in your DEP_ROOTS
SRCS = $(wildcard $(SRC_DIR)/*.c)
SRCS += $(foreach dir,$(DEP_ROOTS),$(wildcard $(dir)/*.c))

# filter out logic for GPIO
ifeq ($(strip $(WITH_GPIOD)), 0)
  SRCS    := $(filter-out $(SRC_DIR)/gpio.c, $(SRCS))
endif

# ─── Object files per architecture ────────────────────────────────────────────

# This uses 'notdir' to handle files coming from different subdirectories 
# and puts them all into a flat build folders.
OBJ_FILES_NATIVE = $(patsubst %.c, $(OBJ_NATIVE)/%.o, $(notdir $(SRCS)))
OBJ_FILES_ARM64  = $(patsubst %.c, $(OBJ_ARM64)/%.o, $(notdir $(SRCS)))
OBJ_FILES_ARMHF  = $(patsubst %.c, $(OBJ_ARMHF)/%.o, $(notdir $(SRCS)))
OBJ_FILES_DEBUG  = $(patsubst %.c, $(OBJ_DEBUG)/%.o, $(notdir $(SRCS)))


# ─── Output binaries ──────────────────────────────────────────────────────────

TARGET        = $(REL_DIR)/aquachemd
TARGET_ARM64  = $(REL_ARM64)/aquachemd
TARGET_ARMHF  = $(REL_ARMHF)/aquachemd
TARGET_DEBUG  = $(REL_DIR)/aquachemd-debug

# ─── Phony targets ────────────────────────────────────────────────────────────

#.PHONY: all debug arm64 armhf release _release_inside_container clean clean-objs distclean install
.PHONY: all debug dummy dummy-debug arm64 armhf release _release_inside_container clean clean-objs distclean install

.DEFAULT_GOAL := all

# ─── Docker release target ────────────────────────────────────────────────────

# Build arm64 + armhf binaries inside the ezo-buildenv Docker container.
# The container must be built first:
#   docker build -f Dockerfile.build -t ezo-buildenv .
release:
	sudo docker run -it --mount type=bind,source=./,target=/build $(DOCKER_IMAGE) make _release_inside_container
	@echo "Release binaries built in $(REL_DIR)/"

# Called inside the Docker container — do not invoke directly
_release_inside_container: distclean arm64 armhf

# ─── Docker release target without clean ───────────────────────────────────────
quick:
	sudo docker run -it --mount type=bind,source=./,target=/build $(DOCKER_IMAGE) make _quick_inside_container
	@echo "Release binaries built in $(REL_DIR)/"

# Called inside the Docker container — do not invoke directly
_quick_inside_container: arm64 armhf


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

$(OBJ_NATIVE)/%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

# ─── Debug build ──────────────────────────────────────────────────────────────

debug: $(TARGET_DEBUG)
	@echo "Built: $(TARGET_DEBUG) (** DEBUG **)"

$(TARGET_DEBUG): $(OBJ_FILES_DEBUG)
	$(CC) $(DFLAGS) $(INCLUDES) -o $@ $^ $(LIBS)

$(OBJ_DEBUG)/%.o: %.c
	$(CC) $(DFLAGS) $(INCLUDES) -c -o $@ $<

# ─── Dummy Sensors & Debug build ───────────────────────────────────────────────

debug: $(TARGET_DEBUG)
	@echo "Built: $(TARGET_DEBUG) (** DEBUG **)"

# NEW: Dummy Debug build
dummy-debug: DFLAGS += -D DUMMY_SENSORS
dummy-debug: $(TARGET_DEBUG)
	@echo "Built: $(TARGET_DEBUG) (** DUMMY SENSORS + DEBUG **)"

$(TARGET_DEBUG): $(OBJ_FILES_DEBUG)
	$(CC) $(DFLAGS) $(INCLUDES) -o $@ $^ $(LIBS)

$(OBJ_DEBUG)/%.o: %.c
	$(CC) $(DFLAGS) $(INCLUDES) -c -o $@ $<


# ─── arm64 cross-compile ──────────────────────────────────────────────────────

arm64: CC := $(CC_ARM64)
arm64: $(TARGET_ARM64)
	@echo "Built: $(TARGET_ARM64)"

$(TARGET_ARM64): $(OBJ_FILES_ARM64)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^ $(LIBS)

$(OBJ_ARM64)/%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

# ─── armhf cross-compile ──────────────────────────────────────────────────────

armhf: CC := $(CC_ARMHF)
armhf: $(TARGET_ARMHF)
	@echo "Built: $(TARGET_ARMHF)"

$(TARGET_ARMHF): $(OBJ_FILES_ARMHF)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^ $(LIBS)

$(OBJ_ARMHF)/%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

# ─── Install ──────────────────────────────────────────────────────────────────

install: $(TARGET)
	install -m 755 $(TARGET) /usr/local/bin/aquachemd
	@echo "Installed to /usr/local/bin/aquachemd"


# ─── Clean Targets ────────────────────────────────────────────────────────────

#.PHONY: clean clean-objs distclean

# List of binaries
ALL_TARGETS = $(TARGET) $(TARGET_ARM64) $(TARGET_ARMHF) $(TARGET_DEBUG)

# #1 Normal Clean: Deletes binaries and standard .o files
# Keeps cJSON.o and mongoose.o
clean:
	@echo "Cleaning binaries and standard objects (keeping heavy deps)..."
	$(RM) $(ALL_TARGETS)
	$(RM) $(filter-out %cJSON.o %mongoose.o, $(OBJ_FILES_NATIVE) $(OBJ_FILES_ARM64) $(OBJ_FILES_ARMHF) $(OBJ_FILES_DEBUG))

# #2 Light Clean: Just the standard objects
clean-objs:
	@echo "Cleaning standard objects only..."
	$(RM) $(filter-out %cJSON.o %mongoose.o, $(OBJ_FILES_NATIVE) $(OBJ_FILES_ARM64) $(OBJ_FILES_ARMHF) $(OBJ_FILES_DEBUG))

# #3 Distclean: The "Nuclear" option for code
# Removes ALL binaries and ALL .o files (including heavy deps)
# Safely leaves scripts and .service files in the release directory
distclean:
	@echo "Deep cleaning all build artifacts..."
	$(RM) $(ALL_TARGETS)
	$(RM) $(OBJ_FILES_NATIVE) $(OBJ_FILES_ARM64) $(OBJ_FILES_ARMHF) $(OBJ_FILES_DEBUG)
	@# Optional: If you want to remove the empty sub-dirs in release, but keep the root:
	@# find $(REL_DIR) -type d -empty -delete
# steelcheck - Makefile (GNU Make)
#
#   make                    debug build of the CLI and the test runner (build/debug/)
#   make test               build and run the tests
#   make BUILD=release      optimized build (build/release/)
#   make BUILD=san test     AddressSanitizer + UBSan build, then run the tests (not on Windows)
#   make CC=clang           choose the compiler
#   make WERROR=1           treat warnings as errors (CI)
#   make clean              remove build/

BUILD ?= debug

#---------------------------------------------------------------------------------------------------
#  Platform
#---------------------------------------------------------------------------------------------------

ifeq ($(OS),Windows_NT)
  EXE := .exe
  # MSYS2 UCRT64 provides gcc but not necessarily cc (make's built-in default).
  ifeq ($(origin CC),default)
    CC := gcc
  endif
else
  EXE :=
endif

#---------------------------------------------------------------------------------------------------
#  Flags
#---------------------------------------------------------------------------------------------------

CSTD := -std=c23

WARNINGS := -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion \
            -Wdouble-promotion -Wformat=2 -Wstrict-prototypes -Wmissing-prototypes \
			-Wmissing-declarations -Wformat-security -Wnull-dereference -Wcast-qual \
			-Wundef -Wvla

ifeq ($(WERROR),1)
  WARNINGS += -Werror
endif

ifeq ($(BUILD),debug)
  OPTFLAGS := -O0 -g3
else ifeq ($(BUILD),release)
  OPTFLAGS := -O2 -DNDEBUG
else ifeq ($(BUILD),san)
  ifeq ($(OS),Windows_NT)
    $(error BUILD=san is not supported on Windows: MinGW has no AddressSanitizer)
  endif
  SANFLAGS := -fsanitize=address,undefined -fno-sanitize-recover=all
  OPTFLAGS := -O1 -g -fno-omit-frame-pointer $(SANFLAGS)
else
  $(error Unknown BUILD '$(BUILD)': use debug, release or san)
endif

# -MMD -MP write a .d file next to each object, listing the headers it includes.
ALL_CPPFLAGS := -Iinclude -MMD -MP $(CPPFLAGS)
ALL_CFLAGS   := $(CSTD) $(WARNINGS) $(OPTFLAGS) $(CFLAGS)
ALL_LDFLAGS  := $(SANFLAGS) $(LDFLAGS)
LDLIBS       := -lm

#---------------------------------------------------------------------------------------------------
#  Files
#---------------------------------------------------------------------------------------------------

BUILD_DIR := build/$(BUILD)

CORE_SRC := $(wildcard src/core/*.c)
CLI_SRC  := $(wildcard src/cli/*.c)
TEST_SRC := $(wildcard tests/*.c)

CORE_OBJ := $(CORE_SRC:%.c=$(BUILD_DIR)/%.o)
CLI_OBJ  := $(CLI_SRC:%.c=$(BUILD_DIR)/%.o)
TEST_OBJ := $(TEST_SRC:%.c=$(BUILD_DIR)/%.o)
DEPS     := $(CORE_OBJ:.o=.d) $(CLI_OBJ:.o=.d) $(TEST_OBJ:.o=.d)

LIB         := $(BUILD_DIR)/libsteelcheck.a
CLI         := $(BUILD_DIR)/steelcheck$(EXE)
TEST_RUNNER := $(BUILD_DIR)/test_runner$(EXE)

#---------------------------------------------------------------------------------------------------
#  Targets
#---------------------------------------------------------------------------------------------------

.PHONY: all test clean

# Delete a target whose recipe failed, so a half-written file is never taken as up to date.
.DELETE_ON_ERROR:

all: $(CLI) $(TEST_RUNNER)

test: $(TEST_RUNNER)
	$(TEST_RUNNER)

# The archive is recreated from scratch, so objects of deleted sources do not linger in it.
$(LIB): $(CORE_OBJ)
	@rm -f $@
	$(AR) rcs $@ $^

# Link order matters: objects first, then the library they use, then -lm.
$(CLI): $(CLI_OBJ) $(LIB)
	$(CC) $(ALL_LDFLAGS) $^ $(LDLIBS) -o $@

$(TEST_RUNNER): $(TEST_OBJ) $(LIB)
	$(CC) $(ALL_LDFLAGS) $^ $(LDLIBS) -o $@

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(ALL_CPPFLAGS) $(ALL_CFLAGS) -c $< -o $@

clean:
	rm -rf build

-include $(DEPS)

# TARGET may be "x86_64" or "x86" or "aarch64"
TARGET     = $(shell uname -m)
OS         = $(shell uname -s)
# BUILD can be "debug" or "release"
BUILD      = release
BUILD_DIR  = .
SRC_DIR    = .
TESTS      = $(SRC_DIR)/tests

PREFIX     = /usr/local

CC         = gcc
BUILD_CC   = gcc
override CFLAGS += -Wall -Wextra -Wno-unused-parameter
override BUILD_CFLAGS += -Wall -Wextra -Wno-unused-parameter
LDFLAGS    = -lm -ldl
LLK        = llk

ifeq (debug, $(BUILD))
 override CFLAGS += -O0 -g -DIR_DEBUG=1
 override BUILD_CFLAGS += -O2 -g -DIR_DEBUG=1
 MINILUA_CFLAGS = -O0 -g
else ifeq (release, $(BUILD))
 override CFLAGS += -O2 -g
 override BUILD_CFLAGS += -O2 -g
 MINILUA_CFLAGS = -O2 -g
else
 $(error Unsupported build type. BUILD must be 'release' or 'debug')
endif

ifneq (, $(filter x86_64 amd64, $(TARGET)))
  override CFLAGS += -m64 -DIR_TARGET_X64
  override BUILD_CFLAGS += -m64 -DIR_TARGET_X64
  TEST_TARGET=x86_64
else ifneq (, $(filter x86 i386, $(TARGET)))
  override CFLAGS += -m32 -DIR_TARGET_X86
  override BUILD_CFLAGS += -m32 -DIR_TARGET_X86
  TEST_TARGET=x86
else ifneq (, $(filter aarch64 arm64, $(TARGET)))
# CC= aarch64-linux-gnu-gcc --sysroot=$(HOME)/php/ARM64
  override CFLAGS += -DIR_TARGET_AARCH64
  override BUILD_CFLAGS += -DIR_TARGET_AARCH64
  TEST_TARGET=aarch64
else
 $(error Unsupported target. TRGET must be 'x86_64', 'x86' or 'aarch64')
endif

ifeq (FreeBSD, $(OS))
  CC=cc
  BUILD_CC=$(CC)
  override CFLAGS += -I/usr/local/include
  override BUILD_CFLAGS += -I/usr/local/include
  LDFLAGS += -L/usr/local/lib
else ifeq (NetBSD, $(OS))
  CC=cc
  BUILD_CC=$(CC)
  override CFLAGS += -I/usr/pkg/include
  override BUILD_CFLAGS += -I/usr/pkg/include
  LDFLAGS = -L/usr/pkg/lib -Wl,-rpath,/usr/pkg/lib -lm
endif

OBJS_RCC = $(BUILD_DIR)/rcc_scanner.o $(BUILD_DIR)/rcc_cpp.o $(BUILD_DIR)/rcc_stdinc.o \
	$(BUILD_DIR)/rcc_parser.o $(BUILD_DIR)/rcc_semantic.o $(BUILD_DIR)/rcc.o

all: $(BUILD_DIR) $(BUILD_DIR)/rcc

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/rcc: $(OBJS_RCC)
	$(CC) $(CFLAGS) -o $@ $^ -lir -lcapstone $(LDFLAGS)

$(BUILD_DIR)/rcc_scanner.o: $(SRC_DIR)/rcc.h
$(BUILD_DIR)/rcc_cpp.o: $(SRC_DIR)/rcc.h
$(BUILD_DIR)/rcc_parser.o: $(SRC_DIR)/rcc.h
$(BUILD_DIR)/rcc_semantic.o: $(SRC_DIR)/rcc.h
$(BUILD_DIR)/rcc_stdinc.o: $(SRC_DIR)/rcc.h
$(BUILD_DIR)/rcc.o: $(SRC_DIR)/rcc.h

$(SRC_DIR)/rcc_parser.c: $(SRC_DIR)/c.g
	$(LLK) -v c.g

$(OBJS_RCC): $(BUILD_DIR)/$(notdir %.o): $(SRC_DIR)/$(notdir %.c)
	$(CC) $(CFLAGS) -I$(BUILD_DIR) -I$(SRC_DIR)/ir -o $@ -c $<

clean:
	rm -rf $(BUILD_DIR)/rcc $(BUILD_DIR)/*.o $(BUILD_DIR)/tester

install: $(BUILD_DIR)/rcc
	install -m a+rx $(BUILD_DIR)/rcc $(PREFIX)/bin

uninstall:
	rm $(PREFIX)/bin/rcc

$(BUILD_DIR)/tester: $(SRC_DIR)/tools/tester.c
	$(CC) $(BUILD_CFLAGS) -o $@ $<

test: $(BUILD_DIR)/rcc $(BUILD_DIR)/tester
	$(BUILD_DIR)/tester \
	--test-cmd $(BUILD_DIR)/rcc \
	--test-extension ".test" \
	--code-extension ".c" \
	--target $(TEST_TARGET) \
	--os $(OS) \
	$(TESTS)

test-ci: $(BUILD_DIR)/rcc $(BUILD_DIR)/tester
	$(BUILD_DIR)/tester \
	--test-cmd $(BUILD_DIR)/rcc \
	--test-extension ".test" \
	--code-extension ".c" \
	--show-diff \
	--target $(TEST_TARGET) \
	--os $(OS) \
	$(TESTS)


bench: $(BUILD_DIR)/rcc
	@if [ ! -x "$(SRC_DIR)/c-benchmarks/run-benchmarks.sh" ]; then \
		echo "Clone the C benchmark suite using one of the following commands, then run 'make bench' once again" ; \
		echo "" ; \
		echo "git clone https://github.com/dstogov/c-benchmarks.git $(SRC_DIR)/c-benchmarks" ; \
		echo "git clone git@github.com:dstogov/c-benchmarks.git $(SRC_DIR)/c-benchmarks" ; \
		echo "" ; \
		exit 1; \
	fi
	(cd $(SRC_DIR)/c-benchmarks/; GCC=gcc RCC=$(realpath $(BUILD_DIR)/rcc) ./run-benchmarks.sh)
	(cd $(SRC_DIR)/c-benchmarks/; GCC=gcc RCC=$(realpath $(BUILD_DIR)/rcc) ./cmp-benchmarks.sh)

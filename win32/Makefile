
!if "$(BUILD)" != "debug"
BUILD=release
!endif

!if "$(BUILD_DIR)" == ""
BUILD_DIR=win32\build_$(VSCMD_ARG_TGT_ARCH)_$(BUILD)
!endif
SRC_DIR=.
IR_SRC_DIR=..\ir
IR_BUILD_DIR=..\ir\win32\build_$(VSCMD_ARG_TGT_ARCH)_$(BUILD)
TESTS=$(SRC_DIR)\tests

CAPSTONE_TAG=5.0-rc2
!if "$(CAPSTONE_DIR)" == ""
CAPSTONE_DIR=..\ir\win32\capstone
!endif

CFLAGS=/nologo /utf-8 /W3 /EHsc /Zi /I$(IR_SRC_DIR)
LDFLAGS=/nologo /NODEFAULTLIB:LIBCMTD.lib
!if "$(BUILD)" == "release"
CFLAGS=$(CFLAGS) /MD /Ox /D_CRT_SECURE_NO_DEPRECATE /D_CRT_STDIO_INLINE=__declspec(dllexport)__inline
LDFLAGS=$(LDFLAGS) /LTCG
!else
CFLAGS=$(CFLAGS) /MDd /Od /DEBUG /D_DEBUG /DIR_DEBUG=1 /D_CRT_SECURE_NO_DEPRECATE /D_CRT_STDIO_INLINE=__declspec(dllexport)__inline

LDFLAGS=$(LDFLAGS) /DEBUG
!endif

!if "$(CC)" == ""
CC=cl.exe
!endif

!if "$(LD)" == ""
LD=link.exe
!endif

!if "$(PHP)" == ""
PHP=php.exe
!endif

!if "$(TARGET)" == ""
TARGET = $(VSCMD_ARG_TGT_ARCH)
!endif
!if "$(TARGET)" == "x64"
CFLAGS=$(CFLAGS) /DIR_TARGET_X64
TEST_TARGET=Windows-x86_64
!endif
!if "$(TARGET)" == "x86"
CFLAGS=$(CFLAGS) /DIR_TARGET_X86
TEST_TARGET=x86
!endif

!if "$(VSCMD_ARG_TGT_ARCH)" == "x64"
LDFLAGS=$(LDFLAGS) /libpath:$(IR_BUILD_DIR) /libpath:$(CAPSTONE_DIR)\msvc\$(VSCMD_ARG_TGT_ARCH)\$(BUILD)
!else
LDFLAGS=$(LDFLAGS) /libpath:$(IR_BUILD_DIR) /libpath:$(CAPSTONE_DIR)\msvc\$(BUILD)
!endif

LIBS=ir.lib psapi.lib capstone.lib

OBJS=$(BUILD_DIR)\rcc.obj $(BUILD_DIR)\rcc_scanner.obj $(BUILD_DIR)\rcc_cpp.obj \
	$(BUILD_DIR)\rcc_parser.obj $(BUILD_DIR)\rcc_semantic.obj $(BUILD_DIR)\rcc_stdinc.obj

all: $(BUILD_DIR) $(BUILD_DIR)\rcc.exe

$(BUILD_DIR)\rcc.exe: $(OBJS)
	"$(LD)" $(LDFLAGS) $(OBJS) $(LIBS) /out:$@

$(OBJS): $(SRC_DIR)\rcc.h $(IR_SRC_DIR)\ir.h $(IR_SRC_DIR)\ir_private.h

{$(SRC_DIR)}.c{$(BUILD_DIR)}.obj:
	"$(CC)" $(CFLAGS) /Fo$@ /c $<

$(BUILD_DIR):
	md "$(BUILD_DIR)"

$(BUILD_DIR)\tester.exe: $(SRC_DIR)/tools/tester.c
	"$(CC)" $(CFLAGS) $(SRC_DIR)\tools\tester.c /Fe:$@

test: $(BUILD_DIR)\rcc.exe $(BUILD_DIR)\tester.exe
	$(BUILD_DIR)\tester.exe --test-cmd $(BUILD_DIR)\rcc.exe --target $(TEST_TARGET) \
		--additional-args "--no-abort-fault" \
		--diff-cmd "diff --strip-trailing-cr -u" \
		--test-extension ".test" --code-extension ".c" $(TESTS)

test-ci: $(BUILD_DIR)\rcc.exe $(BUILD_DIR)\tester.exe
	$(BUILD_DIR)\tester.exe --test-cmd $(BUILD_DIR)\rcc.exe --target $(TEST_TARGET) \
		--additional-args "--no-abort-fault" \
		--diff-cmd "diff --strip-trailing-cr -u" \
		--test-extension ".test" --code-extension ".c" --show-diff $(TESTS)

clean:
	del /f /q $(BUILD_DIR)\*.obj $(BUILD_DIR)\*.exe $(BUILD_DIR)\*.pdb $(BUILD_DIR)\*.ilk


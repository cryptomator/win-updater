# NMAKE makefile. Expects a Visual Studio developer environment, so prefer build.cmd over calling this directly, e.g. `build.cmd x64 0.1.0` or `build.cmd x64 0.1.0 test`.
ARCH = x64
VERSION_MAJOR = 0
VERSION_MINOR = 0
VERSION_PATCH = 0

# Branding of the version resource (FileDescription is what Task Manager and antivirus alerts show), taken from the environment variables
# WIN_UPDATER_PRODUCT and WIN_UPDATER_COMPANY, which NMAKE reads as macros. build.cmd checks that they contain no characters that would break a C string.
!IF "$(WIN_UPDATER_PRODUCT)" == ""
PRODUCT_NAME = win-updater
FILE_DESCRIPTION = App Updater
!ELSE
PRODUCT_NAME = $(WIN_UPDATER_PRODUCT)
FILE_DESCRIPTION = $(WIN_UPDATER_PRODUCT) Updater
!ENDIF
COMPANY_NAME = $(WIN_UPDATER_COMPANY)

OBJ_DIR = target\$(ARCH)
TEST_DIR = target\test
UPDATER = target\win-updater-$(ARCH).exe

SOURCES = src\log.cpp src\options.cpp src\process.cpp src\updater.cpp src\verify.cpp
CFLAGS = /nologo /std:c++17 /EHsc /W4 /WX /sdl /guard:cf /MT /O2 /DUNICODE /D_UNICODE
# Linker flag /ManifestUAC with 'asInvoker' is needed because Windows' installer detection demands elevation for any EXE whose name contains "update"
LINK_FLAGS = /SUBSYSTEM:WINDOWS /GUARD:CF /DYNAMICBASE /NXCOMPAT /HIGHENTROPYVA /DEPENDENTLOADFLAG:0x800 /MANIFEST:EMBED /MANIFESTUAC:"level='asInvoker' uiAccess='false'"
!IF "$(ARCH)" == "x64"
LINK_FLAGS = $(LINK_FLAGS) /CETCOMPAT
!ENDIF
LIBS = kernel32.lib wintrust.lib crypt32.lib bcrypt.lib shell32.lib ole32.lib

########

all: updater

updater:
	if not exist "$(OBJ_DIR)" mkdir "$(OBJ_DIR)"
	echo #define PRODUCT_NAME "$(PRODUCT_NAME)"> "$(OBJ_DIR)\branding.h"
	echo #define FILE_DESCRIPTION "$(FILE_DESCRIPTION)">> "$(OBJ_DIR)\branding.h"
	echo #define COMPANY_NAME "$(COMPANY_NAME)">> "$(OBJ_DIR)\branding.h"
	rc /nologo /i "$(OBJ_DIR)" /d VERSION_MAJOR=$(VERSION_MAJOR) /d VERSION_MINOR=$(VERSION_MINOR) /d VERSION_PATCH=$(VERSION_PATCH) /fo "$(OBJ_DIR)\version.res" src\version.rc
	cl $(CFLAGS) /Fo"$(OBJ_DIR)/" /Fe"$(UPDATER)" $(SOURCES) src\main.cpp "$(OBJ_DIR)\version.res" /link $(LINK_FLAGS) $(LIBS)

# Tests run on the build machine, so only for x64. The test build of the updater replaces the signature verification with a stub, starts the bundle
# without UAC prompt and waits 3 s instead of 60 s.
test: updater
!IF "$(ARCH)" != "x64"
	@echo Tests are only supported for ARCH=x64 1>&2
	@exit 1
!ENDIF
	if not exist "$(TEST_DIR)\updater" mkdir "$(TEST_DIR)\updater"
	if not exist "$(TEST_DIR)\fake" mkdir "$(TEST_DIR)\fake"
	if not exist "$(TEST_DIR)\tests" mkdir "$(TEST_DIR)\tests"
	cl $(CFLAGS) /DWIN_UPDATER_TEST /DWIN_UPDATER_WAIT_TIMEOUT_MS=3000 /Fo"$(TEST_DIR)\updater/" /Fe"$(TEST_DIR)\win-updater-test.exe" $(SOURCES) src\main.cpp "$(OBJ_DIR)\version.res" /link $(LINK_FLAGS) $(LIBS)
	cl $(CFLAGS) /Fo"$(TEST_DIR)\fake/" /Fe"$(TEST_DIR)\fake.exe" test\fake.cpp /link $(LINK_FLAGS) shell32.lib
	cl $(CFLAGS) /Fo"$(TEST_DIR)\tests/" /Fe"$(TEST_DIR)\tests.exe" test\main.cpp test\testing.cpp test\unit_tests.cpp test\integration_tests.cpp src\log.cpp src\options.cpp src\process.cpp src\verify.cpp /link /SUBSYSTEM:CONSOLE $(LIBS)
	"$(TEST_DIR)\tests.exe" "$(UPDATER)" "$(TEST_DIR)\win-updater-test.exe" "$(TEST_DIR)\fake.exe" test\fixtures\skymatic_signed.dll

clean:
	if exist target rmdir /s /q target

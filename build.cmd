@echo off
rem Builds the updater for one architecture: locates Visual Studio, enters its developer environment and runs nmake.
rem Usage: build.cmd <x64|arm64> <version> [nmake target], e.g. `build.cmd x64 0.1.0` or `build.cmd x64 0.1.0 test`.
rem Optional environment variables WIN_UPDATER_PRODUCT and WIN_UPDATER_COMPANY brand the version resource, e.g. "Cryptomator" and "Skymatic GmbH".
rem
rem Quoting as in integrations-win's build.cmd: every assignment uses `set "VAR=..."` and every expansion sits inside double quotes, so that a stray
rem &, | or > in an inherited variable such as TEMP or VSINSTALLDIR stays data. Arguments that reach a command line unquoted are validated first.
setlocal

if "%~1"=="" goto :usage
if "%~2"=="" goto :usage

set "ARCH=%~1"
set "VERSION=%~2"
set "TARGET=%~3"
if not defined TARGET set "TARGET=all"

set "VCVARS_ARCH="
if /i "%ARCH%"=="x64" set "VCVARS_ARCH=x64"
if /i "%ARCH%"=="arm64" set "VCVARS_ARCH=x64_arm64"
if not defined VCVARS_ARCH goto :bad_arch

rem Accept lowercase letters only; any other character yields a token and jumps to the error label.
for /f "delims=abcdefghijklmnopqrstuvwxyz" %%c in ("%TARGET%") do goto :bad_target

rem The version must be three numbers separated by dots.
for /f "delims=0123456789." %%c in ("%VERSION%") do goto :bad_version
set "VERSION_MAJOR="
set "VERSION_MINOR="
set "VERSION_PATCH="
for /f "tokens=1-4 delims=." %%a in ("%VERSION%") do (
  set "VERSION_MAJOR=%%a"
  set "VERSION_MINOR=%%b"
  set "VERSION_PATCH=%%c"
  if not "%%d"=="" goto :bad_version
)
if not defined VERSION_PATCH goto :bad_version

rem Optional branding of the version resource (see Makefile): letters, digits, spaces, dots and hyphens only, as the values end up in a C string.
rem Delayed expansion, so that a quote or & in the value can't break these lines; the values reach nmake through the environment.
rem The space must be the last delimiter, as for /f splits its options at spaces otherwise.
setlocal EnableDelayedExpansion
for /f "delims=ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789.- " %%c in ("!WIN_UPDATER_PRODUCT!") do goto :bad_branding
for /f "delims=ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789.- " %%c in ("!WIN_UPDATER_COMPANY!") do goto :bad_branding
endlocal

rem An ambient developer environment (e.g. a Developer Command Prompt) wins over detection.
set "VS_DIR=%VSINSTALLDIR%"
if defined VS_DIR goto :vcvars

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" goto :no_vswhere
set "VSWHERE_OUT=%TEMP%\win-updater-vsdir.txt"
if /i "%ARCH%"=="arm64" goto :vswhere_arm64
"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath > "%VSWHERE_OUT%"
goto :vswhere_done
:vswhere_arm64
"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -requires Microsoft.VisualStudio.Component.VC.Tools.ARM64 -property installationPath > "%VSWHERE_OUT%"
:vswhere_done
set /p "VS_DIR="<"%VSWHERE_OUT%"
del "%VSWHERE_OUT%"
if not defined VS_DIR goto :no_vs

:vcvars
if not exist "%VS_DIR%\VC\Auxiliary\Build\vcvarsall.bat" goto :no_vcvarsall
call "%VS_DIR%\VC\Auxiliary\Build\vcvarsall.bat" %VCVARS_ARCH% || exit /b 1
nmake /nologo %TARGET% ARCH=%ARCH% VERSION_MAJOR=%VERSION_MAJOR% VERSION_MINOR=%VERSION_MINOR% VERSION_PATCH=%VERSION_PATCH% || exit /b 1
exit /b 0

:bad_arch
echo [build.cmd] unsupported architecture "%ARCH%", expected x64 or arm64 1>&2
exit /b 2

:bad_version
echo [build.cmd] invalid version "%VERSION%", expected three numbers separated by dots, e.g. 0.1.0 1>&2
exit /b 2

:bad_branding
echo [build.cmd] WIN_UPDATER_PRODUCT and WIN_UPDATER_COMPANY may only contain letters, digits, spaces, dots and hyphens 1>&2
exit /b 2

:bad_target
echo [build.cmd] invalid nmake target "%TARGET%", expected lowercase letters only, e.g. test 1>&2
exit /b 2

:no_vswhere
echo [build.cmd] vswhere.exe not found at "%VSWHERE%" 1>&2
echo [build.cmd] set VSINSTALLDIR to the Visual Studio installation root to skip detection 1>&2
exit /b 1

:no_vs
echo [build.cmd] no Visual Studio with the C++ toolset for %ARCH% found 1>&2
exit /b 1

:no_vcvarsall
echo [build.cmd] no VC\Auxiliary\Build\vcvarsall.bat below "%VS_DIR%" 1>&2
echo [build.cmd] VSINSTALLDIR must point at a Visual Studio installation root 1>&2
exit /b 1

:usage
echo Usage: build.cmd ^<x64^|arm64^> ^<version^> [nmake target] 1>&2
echo Example: build.cmd x64 0.1.0 test 1>&2
exit /b 2

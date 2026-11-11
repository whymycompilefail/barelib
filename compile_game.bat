@echo off
setlocal enabledelayedexpansion

REM ============================================================
REM   compile_game.bat - Compile AI-authored games with raylib
REM
REM   Usage:
REM     compile_game.bat game.c                        Compile and run
REM     compile_game.bat game.c build                 Compile only (no run)
REM     compile_game.bat a.c b.c c.c build            Multiple source files
REM     compile_game.bat a.c b.c -o mygame.exe        Explicit output name
REM
REM   By default the output name is the first source file with .exe suffix.
REM   "build" and "-o name.exe" may appear anywhere in the argument list.
REM
REM   Prerequisites:
REM     - Visual Studio 2022/2019 (with C++ build tools)
REM     - raylib built via CMake (run: cmake -B build -G "Visual Studio 17 2022"
REM       then: cmake --build build --target raylib --config Debug)
REM
REM   VCVARS can be set in the environment to override detection (useful for
REM   testing the diagnostic path). If vcvars64.bat is missing or fails,
REM   vswhere.exe + the Windows SDK are used to assemble INCLUDE/LIB by hand.
REM ============================================================

REM --- Validate input ---
if "%~1"=="" (
    echo Usage: compile_game.bat ^<source.c^> [source.c ...] [-o name.exe] [build]
    echo   source.c    - One or more game source files
    echo   -o name.exe - Explicit output executable name
    echo   build       - Optional: compile only, do not run
    exit /b 1
)

REM --- Locate this script's directory (project root) ---
set "PROJECT_ROOT=%~dp0"
set "PROJECT_ROOT=%PROJECT_ROOT:~0,-1%"

REM --- Parse arguments (sources, optional -o, optional build) ---
set "SOURCES="
set "FIRST_SRC="
set "EXE="
set "RUN_AFTER=1"
set "EXPECT_OUT=0"

:parse_args
if "%~1"=="" goto :args_done
if /i "%~1"=="build" (
    set "RUN_AFTER=0"
    shift
    goto :parse_args
)
if /i "%~1"=="-o" (
    set "EXPECT_OUT=1"
    shift
    goto :parse_args
)
if "%EXPECT_OUT%"=="1" (
    for %%F in ("%~f1") do set "EXE=%%~fF"
    set "EXPECT_OUT=0"
    shift
    goto :parse_args
)
if not defined FIRST_SRC set "FIRST_SRC=%~f1"
set SOURCES=!SOURCES! "%~f1"
shift
goto :parse_args

:args_done
if "%EXPECT_OUT%"=="1" (
    echo ERROR: -o requires an output file name.
    exit /b 1
)
if not defined SOURCES (
    echo ERROR: no source files given.
    exit /b 1
)
if not defined EXE (
    for %%F in ("%FIRST_SRC%") do set "EXE=%%~dpnF.exe"
)
for %%F in ("%EXE%") do set "EXEDIR=%%~dpF"
set "EXEDIR=%EXEDIR:~0,-1%"

REM --- Set Visual Studio environment (probe common install paths) ---
REM Only Windows is supported for authoring/using the engine (spec N3); VS 2022
REM or 2019 (Community/Professional/Enterprise/Build Tools) are all acceptable.
if defined VCVARS goto :vcvars_resolved
set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" set "VCVARS=C:\Program Files\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" set "VCVARS=C:\Program Files\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" set "VCVARS=C:\Program Files\Microsoft Visual Studio\2019\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" set "VCVARS=C:\Program Files\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" (
    echo WARNING: Visual Studio 2022/2019 vcvars64.bat not found in common paths.
    echo Tried Community/Professional/Enterprise/Build Tools, 2022/2019, 32/64-bit dirs.
    goto :try_fallback_start
)
:vcvars_resolved

if exist "%VCVARS%" call "%VCVARS%" >nul 2>&1

call :check_toolchain
if "%TOOLCHAIN_OK%"=="1" goto :toolchain_ready

echo.
echo [compile_game] WARNING: vcvars did not produce a usable toolchain.
call :diagnose

:try_fallback_start
call :try_fallback
call :check_toolchain
if "%TOOLCHAIN_OK%"=="1" goto :toolchain_ready

echo.
echo [compile_game] ERROR: vcvars is unavailable - INCLUDE/LIB could not be resolved.
echo   This is usually caused by reg.exe being blocked by a security policy
echo   (vcvars64.bat calls reg.exe internally). It is NOT a missing C runtime:
echo   "fatal error C1083: cannot open stdio.h" is the downstream symptom.
echo   Use tools/build.sh or cmake instead, or fix the policy and retry.
echo.
call :diagnose
exit /b 1

:toolchain_ready

REM --- Set raylib paths ---
set "RAYLIB_SRC=%PROJECT_ROOT%\src"
set "RAYLIB_LIB=%PROJECT_ROOT%\bin\Debug"

if not exist "%RAYLIB_LIB%\raylib.lib" (
    echo ERROR: raylib.lib not found at %RAYLIB_LIB%
    echo Build raylib first:
    echo   cmake -B build -G "Visual Studio 17 2022" -DBUILD_EXAMPLES=OFF
    echo   cmake --build build --target raylib --config Debug
    exit /b 1
)

REM --- Fix for non-ASCII TEMP path issue (cl.exe D8018 error) ---
REM cl.exe creates response files in TEMP; if the username contains
REM non-ASCII characters (e.g. Chinese), this fails. Redirect to ASCII path.
set "TEMP=%PROJECT_ROOT%\temp"
set "TMP=%PROJECT_ROOT%\temp"
if not exist "%TEMP%" mkdir "%TEMP%"

REM --- Clean previous build ---
if exist "%EXE%" del /q "%EXE%" 2>nul

REM --- Compile ---
echo.
echo [compile_game] Sources: %SOURCES%
echo [compile_game] Output:  %EXE%
echo.

REM /MD: match the CRT mode used to build raylib.lib (CMake default /MD dynamic
REM CRT). Without it cl defaults to /MT, which cannot resolve the __imp_* CRT
REM import symbols referenced by raylib (LNK2019 / LNK1120).
cl.exe /nologo /W3 /utf-8 /MD /Fe:"%EXE%" %SOURCES% ^
    /I"%RAYLIB_SRC%" ^
    /link /LIBPATH:"%RAYLIB_LIB%" ^
    raylib.lib opengl32.lib gdi32.lib winmm.lib user32.lib shell32.lib

REM --- Check compilation result ---
if not exist "%EXE%" (
    echo.
    echo [compile_game] ERROR: Compilation failed.
    exit /b 1
)

echo.
echo [compile_game] Compilation succeeded.

REM --- Run the game (unless "build" mode) ---
if "%RUN_AFTER%"=="1" (
    echo.
    echo [compile_game] Running...
    echo ------------------------------------------------------------
    pushd "%EXEDIR%"
    "%EXE%"
    popd
)

exit /b 0

REM ============================================================
REM   Subroutines
REM ============================================================

:check_toolchain
set "TOOLCHAIN_OK=0"
if not defined INCLUDE exit /b 0
if not defined LIB exit /b 0
where cl >nul 2>&1
if errorlevel 1 exit /b 0
REM vcvars64.bat calls reg.exe. If reg.exe is blocked, INCLUDE and cl can
REM still be set while the Windows SDK Lib\<ver>\um\x64 path is missing ->
REM LNK1181 "cannot open opengl32.lib" with no readable root cause. Require
REM the SDK import-lib directory before trusting vcvars.
set "LIB_NO_UM=!LIB:um\x64=!"
if "!LIB_NO_UM!"=="!LIB!" exit /b 0
set "TOOLCHAIN_OK=1"
exit /b 0

:diagnose
echo [compile_game] Resolved toolchain values:
echo   VCVARS  = "%VCVARS%"
echo   INCLUDE = "%INCLUDE%"
echo   LIB     = "%LIB%"
echo   VSROOT  = "%VSROOT%"
echo   MSVCROOT= "%MSVCROOT%"
echo   SDKVER  = "%SDKVER%"
echo   CL_BIN  = "%CL_BIN%"
where cl >nul 2>&1
if errorlevel 1 echo   where cl = NOT FOUND
exit /b 0

:try_fallback
echo [compile_game] Trying vswhere.exe + Windows SDK fallback...
set "PFX86=%ProgramFiles(x86)%"
set "VSWHERE=%PFX86%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VSROOT="
if exist "%VSWHERE%" (
    for /f "usebackq tokens=* delims=" %%i in (`"%VSWHERE%" -latest -products * -property installationPath`) do set "VSROOT=%%i"
)
if not defined VSROOT (
    echo [compile_game] vswhere.exe did not return an installation path.
    exit /b 0
)

set "MSVCROOT="
for /d %%d in ("%VSROOT%\VC\Tools\MSVC\*") do set "MSVCROOT=%%~fd"
if not defined MSVCROOT (
    echo [compile_game] No VC\Tools\MSVC version directory under "%VSROOT%".
    exit /b 0
)

set "SDKROOT=%PFX86%\Windows Kits\10"
set "SDKVER="
for /d %%d in ("%SDKROOT%\Include\*") do set "SDKVER=%%~nxd"
if not defined SDKVER (
    echo [compile_game] No Windows 10 SDK Include directory under "%SDKROOT%".
    exit /b 0
)
if not exist "%SDKROOT%\Include\%SDKVER%\ucrt" (
    echo [compile_game] SDK version "%SDKVER%" has no ucrt headers.
    exit /b 0
)

set "CL_BIN=%MSVCROOT%\bin\Hostx64\x64"
if not exist "%CL_BIN%\cl.exe" (
    echo [compile_game] cl.exe not found at "%CL_BIN%".
    exit /b 0
)

set "INCLUDE=%MSVCROOT%\include;%SDKROOT%\Include\%SDKVER%\ucrt;%SDKROOT%\Include\%SDKVER%\um;%SDKROOT%\Include\%SDKVER%\shared;%SDKROOT%\Include\%SDKVER%\winrt"
set "LIB=%MSVCROOT%\lib\x64;%SDKROOT%\Lib\%SDKVER%\ucrt\x64;%SDKROOT%\Lib\%SDKVER%\um\x64"
set "PATH=%CL_BIN%;%PATH%"
echo [compile_game] Fallback toolchain assembled from vswhere + SDK.
exit /b 0

@echo off
setlocal enabledelayedexpansion

REM ============================================================
REM   compile_game.bat - Compile AI-authored games with raylib
REM
REM   Usage:
REM     compile_game.bat game.c              Compile and run
REM     compile_game.bat game.c build        Compile only (no run)
REM
REM   Prerequisites:
REM     - Visual Studio 2022 (with C++ build tools)
REM     - raylib built via CMake (run: cmake -B build -G "Visual Studio 17 2022"
REM       then: cmake --build build --target raylib --config Debug)
REM ============================================================

REM --- Validate input ---
if "%~1"=="" (
    echo Usage: compile_game.bat ^<source.c^> [build]
    echo   source.c   - Game source file to compile
    echo   build      - Optional: compile only, do not run
    exit /b 1
)

REM --- Locate this script's directory (project root) ---
set PROJECT_ROOT=%~dp0
set PROJECT_ROOT=%PROJECT_ROOT:~0,-1%

REM --- Set Visual Studio environment (probe common install paths) ---
REM Only Windows is supported for authoring/using the engine (spec N3); VS 2022
REM or 2019 (Community/Professional/Enterprise/Build Tools) are all acceptable.
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
    echo ERROR: Visual Studio 2022/2019 vcvars64.bat not found.
    echo Tried common VS install paths - Community/Professional/Enterprise/Build Tools, 2022/2019, 32/64-bit dirs.
    echo Install "Desktop development with C++" workload from VS Installer.
    exit /b 1
)
call "%VCVARS%" >nul 2>&1

REM --- Set raylib paths ---
set RAYLIB_SRC=%PROJECT_ROOT%\src
set RAYLIB_LIB=%PROJECT_ROOT%\bin\Debug

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
set TEMP=%PROJECT_ROOT%\temp
set TMP=%PROJECT_ROOT%\temp
if not exist "%TEMP%" mkdir "%TEMP%"

REM --- Determine source and output paths ---
set SRC=%~f1
set EXE=%~dpn1.exe
set EXEDIR=%~dp1
set EXEDIR=%EXEDIR:~0,-1%

REM --- Determine mode (run by default, "build" = compile only) ---
set RUN_AFTER=1
if /i "%~2"=="build" set RUN_AFTER=0

REM --- Clean previous build ---
if exist "%EXE%" del /q "%EXE%" 2>nul

REM --- Compile ---
echo.
echo [compile_game] Compiling: %SRC%
echo [compile_game] Output:    %EXE%
echo.

REM /MD: match the CRT mode used to build raylib.lib (CMake default /MD dynamic
REM CRT). Without it cl defaults to /MT, which cannot resolve the __imp_* CRT
REM import symbols referenced by raylib (LNK2019 / LNK1120).
cl.exe /nologo /W3 /utf-8 /MD /Fe:"%EXE%" "%SRC%" ^
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

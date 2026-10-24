@echo off
rem Usage: build.bat [release^|debug]   (default: release)
rem Output: build\release\moba_trainer.exe , build\release\moba_tests.exe
setlocal

set CONFIG=%~1
if "%CONFIG%"=="" set CONFIG=release
if /i "%CONFIG%"=="debug" (set CMAKE_CONFIG=Debug) else (set CONFIG=release& set CMAKE_CONFIG=Release)

set VSWHERE="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%i in (`%VSWHERE% -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set VSINSTALL=%%i
if not defined VSINSTALL (
  echo [build] Visual Studio Build Tools with C++ was not found.
  exit /b 1
)

call "%VSINSTALL%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1 || exit /b 1
cd /d "%~dp0"

cmake -S . -B build\%CONFIG% -G Ninja -DCMAKE_BUILD_TYPE=%CMAKE_CONFIG% || exit /b 1
cmake --build build\%CONFIG% || exit /b 1

echo.
echo [build] OK: build\%CONFIG%\moba_trainer.exe

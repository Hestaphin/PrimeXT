@echo off
setlocal
cd /d "%~dp0"
if not exist "build\Release\pxwe.exe" if not exist "build\pxwe.exe" (
  cmake -S . -B build
  if errorlevel 1 exit /b 1
  cmake --build build --config Release --target pxwe
  if errorlevel 1 exit /b 1
)
if exist "build\Release\pxwe.exe" (
  "build\Release\pxwe.exe" %*
) else (
  "build\pxwe.exe" %*
)

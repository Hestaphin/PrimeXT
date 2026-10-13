@echo off
setlocal
if "%~1"=="" (
  echo Usage: build_pxwe.bat "C:\Games\Xash3D\primext"
  exit /b 2
)
cmake -S "%~dp0." -B "%~dp0build-native" -A x64
if errorlevel 1 exit /b 1
cmake --build "%~dp0build-native" --config Release --target pxwe
if errorlevel 1 exit /b 1
cmake --install "%~dp0build-native" --config Release --prefix "%~f1"
if errorlevel 1 exit /b 1
echo Installed "%~f1\devkit\pxwe.exe"

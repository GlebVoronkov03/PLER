@echo off
setlocal
cd /d "%~dp0"
call "%ProgramFiles(x86)%\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvars64.bat" || (
  echo Could not find VS 2017 vcvars64.bat
  exit /b 1
)
if exist "%CUDA_PATH%\bin\nvcc.exe" set "PATH=%CUDA_PATH%\bin;%PATH%"
if exist "C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.2\bin\nvcc.exe" set "PATH=C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.2\bin;%PATH%"
if exist "C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v11.8\bin\nvcc.exe" set "PATH=C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v11.8\bin;%PATH%"

cmake -B build -G "Visual Studio 15 2017" -A x64 -DPLER_CUDA=ON -DPLER_BUILD_GUI=ON
if errorlevel 1 exit /b 1
cmake --build build --config Release
echo.
echo Built: build\pler_cli\Release\pler.exe
echo        build\pler_gui\Release\pler_gui.exe

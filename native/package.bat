@echo off
setlocal EnableDelayedExpansion
REM Build install tree and zip for distribution (Phase 0)
set ROOT=%~dp0..
set BUILD=%~dp0build
set PREFIX=%ROOT%\dist\pler-2.0
set ZIP=%ROOT%\dist\pler-2.0-win64.zip

if not exist "%BUILD%\pler_cli\Release\pler.exe" (
  echo Build Release first: native\build.bat
  exit /b 1
)

if exist "%PREFIX%" rmdir /s /q "%PREFIX%"
mkdir "%PREFIX%\bin"
mkdir "%PREFIX%\config"
mkdir "%PREFIX%\docs"
mkdir "%PREFIX%\samples"
mkdir "%PREFIX%\include"

copy /y "%BUILD%\pler_cli\Release\pler.exe" "%PREFIX%\bin\" >nul
if exist "%BUILD%\pler_gui\Release\pler_gui.exe" copy /y "%BUILD%\pler_gui\Release\pler_gui.exe" "%PREFIX%\bin\" >nul
if exist "%BUILD%\pler_c\Release\pler.dll" copy /y "%BUILD%\pler_c\Release\pler.dll" "%PREFIX%\bin\" >nul
if exist "%BUILD%\pler_c\Release\pler.lib" copy /y "%BUILD%\pler_c\Release\pler.lib" "%PREFIX%\bin\" >nul
copy /y "%~dp0pler_c\include\pler.h" "%PREFIX%\include\" >nul

copy /y "%ROOT%\config\pler.yaml" "%PREFIX%\config\" >nul
copy /y "%ROOT%\docs\*.md" "%PREFIX%\docs\" >nul
copy /y "%ROOT%\README.md" "%PREFIX%\docs\" >nul
copy /y "%ROOT%\LICENSE" "%PREFIX%\docs\" >nul
copy /y "%ROOT%\CITATION.cff" "%PREFIX%\docs\" >nul
if exist "%ROOT%\samples" xcopy /y /q "%ROOT%\samples\*.obj" "%PREFIX%\samples\" >nul

REM CUDA runtime DLLs if present (optional; CPU fallback at runtime)
set "CUDABIN="
if defined CUDA_PATH if exist "%CUDA_PATH%\bin" set "CUDABIN=%CUDA_PATH%\bin"
if not defined CUDABIN if exist "C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.2\bin" set "CUDABIN=C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.2\bin"
if not defined CUDABIN if exist "C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.6\bin" set "CUDABIN=C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.6\bin"
if not defined CUDABIN if exist "C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v11.8\bin" set "CUDABIN=C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v11.8\bin"
if defined CUDABIN (
  for %%F in ("%CUDABIN%\cudart64_*.dll") do copy /y "%%F" "%PREFIX%\bin\" >nul
  echo Bundled CUDA runtime from !CUDABIN!
) else (
  echo No CUDA toolkit bin found; packaging CPU-capable zip only
)

(
echo PLER 2.0.0
echo.
echo Run:  bin\pler.exe --version
echo       bin\pler.exe selftest
echo       bin\pler.exe samples\ref_unit_sphere.obj samples\test_unit_sphere_lod.obj
echo       bin\pler_gui.exe
echo.
echo CUDA is optional. If cudart DLLs are present, the GPU BVH path may be used;
echo otherwise PLER falls back to the CPU BVH automatically.
echo.
echo License: MIT  ^(see docs\LICENSE^)
echo Author: Gleb Alekseevich Voronkov  glebvoronkov03@gmail.com
) > "%PREFIX%\README.txt"

echo Packaged to %PREFIX%
where tar >nul 2>nul
if %ERRORLEVEL%==0 (
  if exist "%ZIP%" del "%ZIP%"
  tar -a -c -f "%ZIP%" -C "%ROOT%\dist" pler-2.0
  echo Zip: %ZIP%
) else (
  echo tar not found; folder ready at %PREFIX%
)
endlocal

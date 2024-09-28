::
:: Run:
:: > .\build.bat
:: > .\build.bat release
:: > .\build.bat publish
::

@echo off

:: Setup
set project_root=%~dp0%
set exe_name=myspace.exe
set publish_path=%project_root%..\nickav.github.io

set build_hash=0
FOR /F "tokens=*" %%g IN ('more %project_root%\.git\refs\heads\master') do (set build_hash=%%g)

set commit_message=0
FOR /F "tokens=*" %%g IN ('more %project_root%\.git\COMMIT_EDITMSG') do (set commit_message=%%g)

:: Args
set release=0
set publish=0
for %%a in (%*) do set "%%a=1"

if %publish%==1 set release=1
echo ---
echo release=%release%
echo publish=%publish%
echo ---

pushd %project_root%
  if not exist build (mkdir build)

  pushd build
    cl /MD -DDEBUG=1 /Od -nologo -Zo -Z7 ..\src\main.cpp /link -subsystem:console -incremental:no -opt:ref -OUT:%exe_name%

    IF %errorlevel% NEQ 0 (popd && goto end)

    rmdir /s /q bin

    if %release%==0 (
      .\%exe_name% ..\data bin --serve
    )
    if %release%==1 (
      .\%exe_name% ..\data bin --open
    )
  popd

  if %publish%==1 (
    xcopy /s /i /y %project_root%\build\bin %publish_path%

    git add .
    git commit -m "%commit_message%"

    git push
  )
popd

:end

exit /B %errorlevel%

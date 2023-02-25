:: Exact same script as `build.bat` with different arguments to exe
@echo off

:: Setup
set project_root=%~dp0%
set exe_name=myspace.exe

pushd %project_root%
  if not exist build (mkdir build)

  pushd build
    cl /MD -DDEBUG=1 /Od -nologo -Zo -Z7 ..\src\main.cpp /link -subsystem:console -incremental:no -opt:ref -OUT:%exe_name%

    IF %errorlevel% NEQ 0 (popd && goto end)

    rmdir /s /q bin

    .\%exe_name% ..\data bin --open
  popd
popd

:end

exit /B %errorlevel%

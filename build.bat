@echo off

:: Setup
set script_path=%~dp0%
set project_root=%script_path%
set exe_name=myspace.exe

pushd %project_root%
  if not exist build (mkdir build)

  pushd build
    cl /MD -DDEBUG=1 /Od -nologo -Zo -Z7 ..\src\main.cpp /link -subsystem:console -incremental:no -opt:ref -OUT:%exe_name%

    IF %errorlevel% NEQ 0 (popd && goto end)

    rmdir /s /q bin
    mkdir bin
    mkdir bin\r

    xcopy /s /i /q /y ..\public bin\

    .\%exe_name%
  popd
popd

:end

exit /B %errorlevel%

@echo off

:: Setup
set project_root=%~dp0%
set publish_path=%project_root%..\nickav.github.io

echo From: %project_root%
echo To: %publish_path%


call %project_root%\dist.bat

pushd %project_root%

    FOR /F "tokens=*" %%g IN ('more %project_root%\.git\refs\heads\master') do (set build_hash=%%g)

    echo %build_hash%

    FOR /F "tokens=*" %%g IN ('more %project_root%\.git\COMMIT_EDITMSG') do (set commit_message=%%g)

    echo %commit_message%
popd

pushd %publish_path%
    xcopy /s /i /y %project_root%\build\bin %publish_path%

    git commit -a -m "%commit_message%"

     git push
popd

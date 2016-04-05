@echo off
SET LOCAL_TARGET_DIR=c:\files\rbin
SET BUILD32=builds\vs2015-32\Release\clc32.exe
SET BUILD64=builds\vs2015-64\Release\clc64.exe

IF NOT EXIST "%BUILD32%" (
    echo [install_local] cannot find %BUILD32%
    goto :FAIL
)
copy /y %BUILD32% %LOCAL_TARGET_DIR% > NUL
IF NOT %ERRORLEVEL%==0 (echo [install_local] failed to copy clc32) & (goto :FAIL)


IF NOT EXIST "%BUILD64%" (
    echo [install_local] cannot find %BUILD64%
    goto :FAIL
)
copy /y %BUILD64% %LOCAL_TARGET_DIR% > NUL
IF NOT %ERRORLEVEL%==0 (echo [install_local] failed to copy clc64) & (goto :FAIL)

echo DONE!
EXIT /B 0

:FAIL
EXIT /B 1

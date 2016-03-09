@echo off

IF "%1"=="/h" goto :HELP
IF "%1"=="/H" goto :HELP
IF "%1"=="/?" goto :HELP
IF "%1"=="-h" goto :HELP
IF "%1"=="--help" goto :HELP


SET PLATFORM=""
IF "%1"=="" (SET PLATFORM=32) & (goto BUILD_DIR)
IF "%1"=="32" (SET PLATFORM=32)
IF "%1"=="64" (SET PLATFORM=64)
IF %PLATFORM%=="" goto :HELP


:BUILD_DIR
set BUILD_DIR=platform-builds\vs2013-%PLATFORM%
IF EXIST "%BUILD_DIR%" (
    echo [config_vs2013] clobbering existing %BUILD_DIR%
    rd /s /q %BUILD_DIR%
    IF NOT %ERRORLEVEL%==0 (echo ) & (goto :FAIL)
)
md %BUILD_DIR%

pushd %BUILD_DIR%
echo [cmake -G "Visual Studio 12" ..\..]
cmake -G "Visual Studio 12" ..\..
popd

:SUCCESS
EXIT /b 0

:HELP
@echo usage: config_vs2013.bat [32^|64]
@echo [32^|64]                      corresponds to a 32-bit or 64-bit project files
@echo                               *** WARNING: this dirctory gets clobbered first! ***
EXIT /b 0

:FAIL
ECHO [build_all] SCRIPT FAILED
EXIT /b 1
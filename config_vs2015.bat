@echo off
set BUILD_ROOT=builds\vs2015-

IF "%1"=="/h" goto :HELP
IF "%1"=="/H" goto :HELP
IF "%1"=="/?" goto :HELP
IF "%1"=="-h" goto :HELP
IF "%1"=="--help" goto :HELP


set BUILD_DIR=%BUILD_ROOT%32
IF EXIST "%BUILD_DIR%" (
    echo [config_vs2015] clobbering existing %BUILD_DIR%
    rd /s /q %BUILD_DIR%
    IF NOT %ERRORLEVEL%==0 (echo ) & (goto :FAIL)
)
md %BUILD_DIR%
pushd %BUILD_DIR%
echo [cmake -G "Visual Studio 14" ..\..]
cmake -G "Visual Studio 14" ..\..
popd

set BUILD_DIR=%BUILD_ROOT%64
IF EXIST "%BUILD_DIR%" (
    echo [config_vs2015] clobbering existing %BUILD_DIR%
    rd /s /q %BUILD_DIR%
    IF NOT %ERRORLEVEL%==0 (echo ) & (goto :FAIL)
)
md %BUILD_DIR%
pushd %BUILD_DIR%
echo [cmake -G "Visual Studio 14 Win64" ..\..]
cmake -G "Visual Studio 14 Win64" ..\..
popd

:SUCCESS
EXIT /b 0

:HELP
@echo usage: config_vs2015.bat
EXIT /b 0

:FAIL
ECHO [build_all] SCRIPT FAILED
EXIT /b 1
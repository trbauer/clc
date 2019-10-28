@echo off
md builds

md builds\vs2019-64
cd builds\vs2019-64
cmake -G "Visual Studio 16" -A x64 ..\..
cd ..\..

md builds\vs2019-32
cd builds\vs2019-32
cmake -G "Visual Studio 16" -A x32 ..\..
cd ..\..

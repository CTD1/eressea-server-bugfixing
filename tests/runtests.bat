@ECHO OFF
SET BUILD=..\build-vs12\eressea\Debug\
SET SERVER=%BUILD%\eressea.exe
%BUILD%\test_eressea.exe
%SERVER% ..\scripts\run-tests.lua
%SERVER% ..\scripts\run-tests-e2.lua
%SERVER% ..\scripts\run-tests-e3.lua
%SERVER% ..\scripts\run-tests-e4.lua
PAUSE
RMDIR /s /q reports

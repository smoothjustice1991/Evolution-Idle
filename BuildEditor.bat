@echo off
rem Compiles the EvolutionIdle editor module without opening Unreal.
rem Output goes to Saved\BuildLog.txt. Close the Unreal editor first.
setlocal
set "UE=C:\Program Files\Epic Games\UE_5.8"
set "PROJDIR=%~dp0"
if not exist "%PROJDIR%Saved" mkdir "%PROJDIR%Saved"
echo Building EvolutionIdleEditor... > "%PROJDIR%Saved\BuildLog.txt"
call "%UE%\Engine\Build\BatchFiles\Build.bat" EvolutionIdleEditor Win64 Development -Project="%PROJDIR%EvolutionIdle.uproject" -WaitMutex -NoHotReloadFromIDE >> "%PROJDIR%Saved\BuildLog.txt" 2>&1
echo BUILD_EXIT_CODE=%ERRORLEVEL% >> "%PROJDIR%Saved\BuildLog.txt"
endlocal

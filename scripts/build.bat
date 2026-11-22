@echo off
setlocal EnableExtensions
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo error: vswhere.exe not found
    exit /b 1
)
set "VSINSTALL="
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -property installationPath`) do set "VSINSTALL=%%i"
if "%VSINSTALL%"=="" (
    echo error: Visual Studio installation not found
    exit /b 1
)
echo Using Visual Studio: %VSINSTALL%
call "%VSINSTALL%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
if not exist build mkdir build
cl /nologo /W4 /WX /O2 /I. /Iinclude src\main.c src\mcp.c src\tools.c src\config.c src\obs_ws.c src\obs_process.c vendor\cJSON\cJSON.c /Fe:build\obs-mcp.exe /link ws2_32.lib psapi.lib
if errorlevel 1 exit /b 1
cl /nologo /W4 /WX /O2 /I. /Iinclude tests\test_obs_mcp.c src\mcp.c src\tools.c src\config.c src\obs_ws.c src\obs_process.c vendor\cJSON\cJSON.c /Fe:build\test_obs_mcp.exe /link ws2_32.lib psapi.lib
if errorlevel 1 exit /b 1
build\test_obs_mcp.exe
exit /b %errorlevel%

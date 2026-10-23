@echo off
setlocal
cd /d "%~dp0"
if not exist .venv\Scripts\python.exe py -3 -m venv .venv
if errorlevel 1 goto fail
.venv\Scripts\python.exe -m pip install -r requirements.txt
if errorlevel 1 goto fail
if not exist runtime\bin\mk64-extract.exe .venv\Scripts\python.exe build.py
if errorlevel 1 goto fail
.venv\Scripts\pythonw.exe app.py
exit /b
:fail
echo Setup failed. Python 3.11+, CMake, Git and a C++20 compiler are required.
pause
exit /b 1

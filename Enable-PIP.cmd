@echo off
"%~dp0build\loader\PIP.Loader.exe" --enable "%~dp0."
if errorlevel 1 pause

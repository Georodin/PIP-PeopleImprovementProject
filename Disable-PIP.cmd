@echo off
"%~dp0build\loader\PIP.Loader.exe" --disable "%~dp0."
if errorlevel 1 pause

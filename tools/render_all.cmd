@echo off
title War Wind HD - cutscene remaster
cd /d "%~dp0"
"%~dp0..\..\..\.venv\Scripts\python.exe" video_pipeline.py run %*
pause

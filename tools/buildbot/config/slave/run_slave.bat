@echo off
setlocal
title Build slave
set PATH=%~dp0..\depot_tools\release\python_24\;%PATH%
python.exe %~dp0\run_slave.py --no_save -y buildbot.tac

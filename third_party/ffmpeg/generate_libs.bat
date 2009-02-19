@echo off

setlocal
set OUTDIR=%1

lib /nologo /machine:X86 /def:avcodec-52.def  /out:%OUTDIR%\lib\avcodec-52.lib
lib /nologo /machine:X86 /def:avformat-52.def /out:%OUTDIR%\lib\avformat-52.lib
lib /nologo /machine:X86 /def:avutil-49.def   /out:%OUTDIR%\lib\avutil-49.lib


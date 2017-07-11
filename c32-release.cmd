@echo off
call iclvars.bat ia32 vs2015
cd sndrender
nmake all SSE2=1 RELEASE=1
cd ..

cd z80
nmake all SSE2=1 RELEASE=1
cd ..

nmake all SSE2=1 RELEASE=1

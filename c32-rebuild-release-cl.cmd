@echo off
cd sndrender
nmake clean
nmake all SSE2=1 RELEASE=1 USE_CL=1
cd ..

cd z80
nmake clean
nmake all SSE2=1 RELEASE=1 USE_CL=1
cd ..

nmake clean
nmake all SSE2=1 RELEASE=1 USE_CL=1

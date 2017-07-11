!ifdef USE_CL
CXX=cl -c
ICL_FLAGS_COMMON=
ICL_FLAGS_RELEASE=
ICL_IA32=
CL_FLAGS_RELEASE=-Ox -GL
LIB=lib
LIBFLAGS=-LTCG
!else
CXX=icl -c
ICL_FLAGS_COMMON=-Wcheck -Qms0 -Qopt-report-embed- -notraceback
ICL_FLAGS_RELEASE=-O3 -Qipo
ICL_IA32=-arch:IA32
CL_FLAGS_RELEASE=
LIB=xilib
LIBFLAGS=-qipo
!endif

CFLAGS_COMMON=-nologo -W3 -EHa- -GR- -Zi -MP -Oi -Zc:threadSafeInit- $(ICL_FLAGS_COMMON) \
         -D_CRT_SECURE_NO_DEPRECATE -DUSE_SND_EXTERNAL_BUFFER -D_PREFIX_ -D_USING_V110_SDK71_

!ifdef SSE1
CFLAGS_COMMON=$(CFLAGS_COMMON) -QxK
!elseifdef SSE2
CFLAGS_COMMON=$(CFLAGS_COMMON) -arch:SSE2 -D_M_IX86_FP=2
!elseifdef SSE42
CFLAGS_COMMON=$(CFLAGS_COMMON) -QxSSE4.2 -D_M_IX86_FP=2
!else
CFLAGS_COMMON=$(CFLAGS_COMMON) $(ICL_IA32)
!endif

!ifdef DEBUG
CFLAGS_DEBUG=-Od -MTd -DDEBUG -D_DEBUG
!else
CFLAGS_RELEASE=-DNDEBUG -MT $(CL_FLAGS_RELEASE) $(ICL_FLAGS_RELEASE)
!endif

CXXFLAGS=$(CFLAGS_COMMON) $(CFLAGS_DEBUG) $(CFLAGS_RELEASE) -Zc:forScope,wchar_t
CFLAGS=$(CFLAGS_COMMON) $(CFLAGS_DEBUG) $(CFLAGS_RELEASE) -Zc:wchar_t

all: z80.lib

.c.obj::
	$(CXX) $(CFLAGS) $<

.cpp.obj::
	$(CXX) $(CXXFLAGS) $<

z80.lib: daa_tabs.obj op_cb.obj op_dd.obj op_ddcb.obj op_ed.obj \
	op_fd.obj op_noprefix.obj tables.obj
	$(LIB) $(LIBFLAGS) -out:$@ $**
clean:
	-del *.obj *.lib

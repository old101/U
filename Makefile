.SUFFIXES : .dep .cpp

!ifdef X64
OUT_DIR=x64
LIBPATH=lib64
LFLAGS=-subsystem:console,5.02
!else
OUT_DIR=x32
LIBPATH=lib32
LFLAGS=-subsystem:console,5.01
!endif

LFLAGS=$(LFLAGS) -osversion:5.0 -pdbaltpath:%_PDB%

!ifdef USE_CL
!ifndef DEBUG
LFLAGS=-LTCG
!endif #DEBUG

CXX=cl -c
ICL_FLAGS_COMMON=
ICL_FLAGS_RELEASE=
ICL_IA32=
CL_FLAGS_RELEASE=-Ox -GL
LINK=link
LFLAGS=$(LFLAGS) #-pdbpath:none
!else #USE_CL
CXX=icl -c
ICL_FLAGS_COMMON=-Wcheck -Qms0 -debug:inline-debug-info -Qopt-report-embed- -notraceback
ICL_FLAGS_RELEASE=-O3 -Qip -Qipo
ICL_IA32=-arch:IA32
CL_FLAGS_RELEASE=
LINK=xilink
LFLAGS=$(LFLAGS) -qipo #-pdbpath:none
!endif #USE_CL

#-RTCsu -Qtrapuv

CFLAGS_COMMON=-nologo -W3 -EHa- -GR- -Zi -MP -Oi -Zc:threadSafeInit- $(ICL_FLAGS_COMMON) \
         -D_CRT_SECURE_NO_DEPRECATE -DUSE_SND_EXTERNAL_BUFFER -D_PREFIX_ -D_USING_V110_SDK71_

!ifdef VGEMUL
CFLAGS_COMMON=$(CFLAGS_COMMON) -DVG_EMUL
!endif

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

LFLAGS=$(LFLAGS) -debug -fixed:no -release -libpath:$(LIBPATH)

RCFLAGS=-D_USING_V110_SDK71_

LIBS=$(LIBS) sndrender/snd.lib z80/z80.lib

SRCS=emul.cpp std.cpp atm.cpp cheat.cpp config.cpp dbgbpx.cpp dbgcmd.cpp dbglabls.cpp \
    dbgmem.cpp dbgoth.cpp dbgpaint.cpp dbgreg.cpp dbgrwdlg.cpp dbgtrace.cpp \
        debug.cpp draw.cpp drawnomc.cpp draw_384.cpp dx.cpp dxerr.cpp dxovr.cpp \
        dxrcopy.cpp dxrend.cpp dxrendch.cpp dxrframe.cpp dxr_4bpp.cpp dxr_512.cpp \
        dxr_advm.cpp dxr_atm.cpp dxr_atm0.cpp dxr_atm2.cpp dxr_atm4.cpp dxr_atm6.cpp \
        dxr_atm7.cpp \
        dxr_atmf.cpp dxr_prof.cpp dxr_rsm.cpp dxr_text.cpp dxr_vd.cpp \
        emulkeys.cpp emul_2203.cpp fntsrch.cpp font.cpp font14.cpp font16.cpp \
        font8.cpp fontatm2.cpp fontdata.cpp gs.cpp gshlbass.cpp gshle.cpp \
        gsz80.cpp gui.cpp hdd.cpp hddio.cpp iehelp.cpp init.cpp \
        input.cpp inputpc.cpp io.cpp keydefs.cpp leds.cpp mainloop.cpp \
        memory.cpp modem.cpp opendlg.cpp savesnd.cpp sdcard.cpp snapshot.cpp \
        snd_bass.cpp sound.cpp sshot_png.cpp tape.cpp util.cpp vars.cpp \
        vs1001.cpp wd93cmd.cpp wd93crc.cpp wd93dat.cpp wd93trk.cpp \
        wldr_fdi.cpp wldr_isd.cpp wldr_pro.cpp wldr_td0.cpp wldr_trd.cpp wldr_udi.cpp \
        z80.cpp z80asm.cpp zc.cpp savevid.cpp

OBJS=$(SRCS:.cpp=.obj)

all: $(OUT_DIR)/emul.exe

dep: mk.dep

mk.dep: $(SRCS)
    $(CXX) -QMM $** >mk.dep

.c.obj::
    $(CXX) $(CFLAGS) $<

std.obj: std.cpp
    $(CXX) $(CXXFLAGS) -Yc"std.h" std.cpp

.cpp.obj::
    $(CXX) $(CXXFLAGS) -Yu"std.h" $<

.rc.res:
    $(RC) $(RCFLAGS) $<

$(OUT_DIR)/emul.exe: $(OBJS) $(LIBS) settings.res
    $(LINK) $(LFLAGS) -out:$@ -pdb:$*.pdb -map:$*.map $** $(LIBS)

clean:
    -del *.obj *.res *.map *.pdb *.pch *.pchi

!if exist(mk.dep)
!include mk.dep
!endif

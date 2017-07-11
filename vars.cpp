#include "std.h"

#include "emul.h"
#include "vars.h"
#include "debug.h"
#include "memory.h"
#include "gsz80.h"
#include "z80.h"
#include "util.h"

namespace z80fast
{
#include "z80_main.h"
}

#include "sndrender/sndcounter.h"

int fmsoundon0=4; //Alone Coder
int tfmstatuson0=2; //Alone Coder
char pressedit = 0; //Alone Coder

void __fastcall wmdbg(u32 addr, u8 val);
u8 __fastcall rmdbg(u32 addr);
u8 *__fastcall MemDbg(u32 addr);
void __cdecl BankNames(int i, char *Name);
namespace z80dbg
{
void Z80FAST step();
__int64 __cdecl delta();
void __cdecl SetLastT();
}

#pragma pack(8)

void out(unsigned port, unsigned char val);
unsigned char in(unsigned port);

Z80INLINE unsigned char m1_cycle(Z80 *cpu)
{
   cpu->r_low++; cpu->t += 4;
   return cpu->MemIf->xm(cpu->pc++);
}

/*
unsigned char TMainZ80::rm(unsigned addr) { return z80fast::rm(addr); }

unsigned char TMainZ80::dbgrm(unsigned addr) { return ::rmdbg(addr); }

void TMainZ80::wm(unsigned addr, unsigned char val) { z80fast::wm(addr, val); }

void TMainZ80::dbgwm(unsigned addr, unsigned char val) { ::wmdbg(addr, val); }
*/
u8 *TMainZ80::DirectMem(unsigned addr) const
{
    return am_r(addr);
}

unsigned char TMainZ80::m1_cycle() { return ::m1_cycle(this); }

unsigned char TMainZ80::in(unsigned port) { return ::in(port); }

void TMainZ80::out(unsigned port, unsigned char val) { ::out(port, val); }

u8 TMainZ80::IntVec()
{
    return (comp.flags & CF_Z80FBUS)? u8(rdtsc() & 0xFF) : 0xFF;
}

void TMainZ80::CheckNextFrame()
{
   if(t >= conf.frame)
   {
       comp.t_states += conf.frame;
       t -= conf.frame;
       eipos -= conf.frame;
       comp.frame_counter++;
       int_pend = true;
   }
}

void TMainZ80::retn()
{
    nmi_in_progress = false;
    set_banks();
}

static const TMemIf FastMemIf = { Xm, Rm, Wm };
static const TMemIf DbgMemIf = { DbgXm, DbgRm, DbgWm };


TMainZ80 cpu(0, BankNames, z80dbg::step, z80dbg::delta, z80dbg::SetLastT, membits, &FastMemIf, &DbgMemIf);


#ifdef MOD_GSZ80
namespace z80gs
{
    u8 __fastcall dbgrm(u32 addr);
    void __fastcall dbgwm(u32 addr, u8 val);
    u8 *__fastcall MemDbg(u32 addr);
    void __cdecl BankNames(int i, char *Name);
    void Z80FAST step();
    __int64 __cdecl delta();
    void __cdecl SetLastT();
    u8 membits[0x10000];

    SNDRENDER sound;
    static const TMemIf FastMemIf = { z80gs::Xm, z80gs::Rm, z80gs::Wm };
    static const TMemIf DbgMemIf = { z80gs::DbgXm, z80gs::DbgRm, z80gs::DbgWm };

}

u8 TGsZ80::IntVec()
{
    return 0xFF;
}

void TGsZ80::CheckNextFrame()
{
   if(t >= z80gs::GSCPUINT)
   {
      t -= z80gs::GSCPUINT;
      eipos -= z80gs::GSCPUINT;
      z80gs::gs_t_states += z80gs::GSCPUINT;
      int_pend = true;
   }
}

TGsZ80 gscpu(1, z80gs::BankNames, z80gs::step, z80gs::delta,
    z80gs::SetLastT, z80gs::membits, &z80gs::FastMemIf, &z80gs::DbgMemIf);
#endif

TCpuMgr CpuMgr;

void TCpuMgr::SwitchCpu()
{
    CurrentCpu++;
    CurrentCpu %= Count;
}

Z80 *TCpuMgr::Cpus[] =
{
  &cpu,
#ifdef MOD_GSZ80
  &gscpu
#endif
};


const unsigned TCpuMgr::Count = _countof(Cpus);
TZ80State TCpuMgr::PrevCpus[TCpuMgr::Count];
unsigned TCpuMgr::CurrentCpu = 0;

#ifdef MOD_GSBASS
GSHLE gs;
#endif

unsigned char dbgbreak = 0;


CONFIG conf;
COMPUTER comp;
TEMP temp;
ATA_PORT hdd;   // not in `comp' - not cleared in reset()
K_INPUT input;
ISA_MODEM modem;

SNDRENDER sound;
SNDCHIP ay[2];
SNDCOUNTER sndcounter;


unsigned char *base_sos_rom, *base_dos_rom, *base_128_rom, *base_sys_rom;

#ifdef CACHE_ALIGNED
ATTR_ALIGN(4096)
unsigned char memory[PAGE*MAX_PAGES];
#else // __declspec(align) not available, force QWORD align with old method
__int64 memory__[PAGE*MAX_PAGES/sizeof(__int64)];
unsigned char * const memory = (unsigned char*)memory__;
#endif

#ifdef MOD_VID_VD
CACHE_ALIGNED unsigned char vdmem[4][0x2000];
#endif

unsigned char membits[0x10000];
unsigned char *bankr[4];
unsigned char *bankw[4];
unsigned char cmos[0x100];
unsigned char nvram[0x800];

unsigned sndplaybuf[PLAYBUFSIZE];
unsigned spbsize;

FILE *savesnd;
unsigned char savesndtype; // 0-none,1-wave,2-vtx
unsigned char *vtxbuf; unsigned vtxbufsize, vtxbuffilled;

unsigned char trdos_load, trdos_save, trdos_format, trdos_seek; // for leds
unsigned char needclr; // clear screenbuffer before rendering

HWND wnd; HINSTANCE hIn;

char droppedFile[512];

const TMemModel mem_model[N_MM_MODELS] =
{
    { "PENTAGON", "PENTAGON",                MM_PENTAGON, 128,  RAM_128 | RAM_256 | RAM_512 | RAM_1024 },
    { "ZS SCORPION", "SCORPION",             MM_SCORP, 256,  RAM_256 | RAM_1024 },
    { "ZS SCORPION + PROF ROM", "PROFSCORP", MM_PROFSCORP, 256,  RAM_256 | RAM_1024 },
    { "PROFI", "PROFI",                      MM_PROFI, 1024, RAM_1024 },
    { "ATM-TURBO v4.50", "ATM450",           MM_ATM450, 512,  RAM_512 | RAM_1024 },
    { "ATM-TURBO 2+ v7.10", "ATM710",        MM_ATM710, 1024, RAM_128 | RAM_256 | RAM_512 | RAM_1024 },
    { "ATM-TURBO 3", "ATM3",                 MM_ATM3, 4096, RAM_4096 },
    { "Nemo's KAY", "KAY",                   MM_KAY, 256,  RAM_256 | RAM_1024 },
    { "ZX-Spectrum +3", "PLUS3",             MM_PLUS3, 128,  RAM_128 },
    { "QUORUM", "QUORUM",                    MM_QUORUM, 1024, RAM_128 | RAM_1024 },
};

unsigned char kbdpc[VK_MAX]; // add cells for mouse & joystick
unsigned char kbdpcEX[6]; //Dexus
keyports inports[VK_MAX];

char statusline[128];
unsigned statcnt;

char arcbuffer[0x2000]; // extensions and command lines for archivers
char skiparc[0x400]; // ignore this files in archive

unsigned char exitflag = 0; // avoid call exit() twice

// beta128 vars
unsigned trd_toload = 0; // drive to load
unsigned DefaultDrive = -1; // ��᪮��� �� 㬮�砭�� � ����� ������� ��ࠧ� ��᪮� �� ����

char trd_loaded[4]; // used to get first free drive with no account of autoloaded images
char ininame[0x200];
char helpname[0x200];
unsigned snapsize;

// conditional breakpoints support
unsigned brk_port_in, brk_port_out;
unsigned char brk_port_val;

virtkeyt pckeys[] =
{
   { "ESC", DIK_ESCAPE },
   { "F1", DIK_F1 }, { "F2", DIK_F2 }, { "F3", DIK_F3 },
   { "F4", DIK_F4 }, { "F5", DIK_F5 }, { "F6", DIK_F6 },
   { "F7", DIK_F7 }, { "F8", DIK_F8 }, { "F9", DIK_F9 },
   { "F10", DIK_F10 }, { "F11", DIK_F11 }, { "F12", DIK_F12 },
   { "PRSCR", DIK_SYSRQ }, { "SCLOCK", DIK_SCROLL }, { "PAUSE", DIK_PAUSE },

   { "1", DIK_1 }, { "2", DIK_2 }, { "3", DIK_3 }, { "4", DIK_4 }, { "5", DIK_5 },
   { "6", DIK_6 }, { "7", DIK_7 }, { "8", DIK_8 }, { "9", DIK_9 }, { "0", DIK_0 },

   { "Q", DIK_Q }, { "W", DIK_W }, { "E", DIK_E }, { "R", DIK_R }, { "T", DIK_T },
   { "Y", DIK_Y }, { "U", DIK_U }, { "I", DIK_I }, { "O", DIK_O }, { "P", DIK_P },
   { "A", DIK_A }, { "S", DIK_S }, { "D", DIK_D }, { "F", DIK_F }, { "G", DIK_G },
   { "H", DIK_H }, { "J", DIK_J }, { "K", DIK_K }, { "L", DIK_L },
   { "Z", DIK_Z }, { "X", DIK_X }, { "C", DIK_C }, { "V", DIK_V }, { "B", DIK_B },
   { "N", DIK_N }, { "M", DIK_M },

   { "MINUS", DIK_MINUS }, { "PLUS", DIK_EQUALS }, { "BACK", DIK_BACK },
   { "TAB", DIK_TAB }, { "LB", DIK_LBRACKET }, { "RB", DIK_RBRACKET },
   { "CAPS", DIK_CAPITAL }, { "TIL", DIK_GRAVE }, { "SPACE", DIK_SPACE },
   { "COL", DIK_SEMICOLON }, { "QUOTE", DIK_APOSTROPHE }, { "ENTER", DIK_RETURN },
   { "COMMA", DIK_COMMA }, { "POINT", DIK_PERIOD }, { "SLASH", DIK_SLASH }, { "BACKSL", DIK_BACKSLASH },
   { "SHIFT", DIK_SHIFT }, { "ALT", DIK_MENU }, { "CONTROL", DIK_CONTROL },
   { "LSHIFT", DIK_LSHIFT }, { "LALT", DIK_LMENU }, { "LCONTROL", DIK_LCONTROL },
   { "RSHIFT", DIK_RSHIFT }, { "RALT", DIK_RMENU }, { "RCONTROL", DIK_RCONTROL },

   { "INS", DIK_INSERT }, { "HOME", DIK_HOME }, { "PGUP", DIK_PRIOR },
   { "DEL", DIK_DELETE }, { "END", DIK_END },   { "PGDN", DIK_NEXT },

   { "UP", DIK_UP }, { "DOWN", DIK_DOWN }, { "LEFT", DIK_LEFT }, { "RIGHT", DIK_RIGHT },

   { "NUMLOCK", DIK_NUMLOCK }, { "GRDIV", DIK_DIVIDE },
   { "GRMUL", DIK_MULTIPLY }, { "GRSUB", DIK_SUBTRACT }, { "GRADD", DIK_ADD },
   { "GRENTER", DIK_NUMPADENTER },

   { "N0", DIK_NUMPAD0 }, { "N1", DIK_NUMPAD1 }, { "N2", DIK_NUMPAD2 },
   { "N3", DIK_NUMPAD3 }, { "N4", DIK_NUMPAD4 }, { "N5", DIK_NUMPAD5 },
   { "N6", DIK_NUMPAD6 }, { "N7", DIK_NUMPAD7 }, { "N8", DIK_NUMPAD8 },
   { "N9", DIK_NUMPAD9 }, { "NP", DIK_DECIMAL },

   { "LMB", VK_LMB }, { "RMB", VK_RMB }, { "MMB", VK_MMB },
   { "MWU", VK_MWU }, { "MWD", VK_MWD },

   { "JLEFT", VK_JLEFT }, { "JRIGHT", VK_JRIGHT },
   { "JUP", VK_JUP }, { "JDOWN", VK_JDOWN }, { "JFIRE", VK_JFIRE },
   { "JB0", VK_JB0 }, 
   { "JB1", VK_JB1 }, 
   { "JB2", VK_JB2 }, 
   { "JB3", VK_JB3 }, 
   { "JB4", VK_JB4 }, 
   { "JB5", VK_JB5 }, 
   { "JB6", VK_JB6 }, 
   { "JB7", VK_JB7 }, 
   { "JB8", VK_JB8 }, 
   { "JB9", VK_JB9 }, 
   { "JB10", VK_JB10 }, 
   { "JB11", VK_JB11 }, 
   { "JB12", VK_JB12 }, 
   { "JB13", VK_JB13 }, 
   { "JB14", VK_JB14 }, 
   { "JB15", VK_JB15 }, 
   { "JB16", VK_JB16 }, 
   { "JB17", VK_JB17 }, 
   { "JB18", VK_JB18 }, 
   { "JB19", VK_JB19 }, 
   { "JB20", VK_JB20 }, 
   { "JB21", VK_JB21 }, 
   { "JB22", VK_JB22 }, 
   { "JB23", VK_JB23 }, 
   { "JB24", VK_JB24 }, 
   { "JB25", VK_JB25 }, 
   { "JB26", VK_JB26 }, 
   { "JB27", VK_JB27 }, 
   { "JB28", VK_JB28 }, 
   { "JB29", VK_JB29 }, 
   { "JB30", VK_JB30 }, 
   { "JB31", VK_JB31 }, 
};
const size_t pckeys_count = _countof(pckeys);

static zxkey zxk_default[] =
{
   { "KRIGHT", &input.kjoy, u8(~1U) },
   { "KLEFT",  &input.kjoy, u8(~2U) },
   { "KDOWN",  &input.kjoy, u8(~4U) },
   { "KUP",    &input.kjoy, u8(~8U) },
   { "KFIRE",  &input.kjoy, u8(~16U)},

   { "ENT", input.kbd+6, u8(~0x01U) },
   { "SPC", input.kbd+7, u8(~0x01U) },
   { "SYM", input.kbd+7, u8(~0x02U) },

   { "CAP", input.kbd+0, u8(~0x01U) },
   { "Z",   input.kbd+0, u8(~0x02U) },
   { "X",   input.kbd+0, u8(~0x04U) },
   { "C",   input.kbd+0, u8(~0x08U) },
   { "V",   input.kbd+0, u8(~0x10U) },

   { "A",   input.kbd+1, u8(~0x01U) },
   { "S",   input.kbd+1, u8(~0x02U) },
   { "D",   input.kbd+1, u8(~0x04U) },
   { "F",   input.kbd+1, u8(~0x08U) },
   { "G",   input.kbd+1, u8(~0x10U) },

   { "Q",   input.kbd+2, u8(~0x01U) },
   { "W",   input.kbd+2, u8(~0x02U) },
   { "E",   input.kbd+2, u8(~0x04U) },
   { "R",   input.kbd+2, u8(~0x08U) },
   { "T",   input.kbd+2, u8(~0x10U) },

   { "1",   input.kbd+3, u8(~0x01U) },
   { "2",   input.kbd+3, u8(~0x02U) },
   { "3",   input.kbd+3, u8(~0x04U) },
   { "4",   input.kbd+3, u8(~0x08U) },
   { "5",   input.kbd+3, u8(~0x10U) },

   { "0",   input.kbd+4, u8(~0x01U) },
   { "9",   input.kbd+4, u8(~0x02U) },
   { "8",   input.kbd+4, u8(~0x04U) },
   { "7",   input.kbd+4, u8(~0x08U) },
   { "6",   input.kbd+4, u8(~0x10U) },

   { "P",   input.kbd+5, u8(~0x01U) },
   { "O",   input.kbd+5, u8(~0x02U) },
   { "I",   input.kbd+5, u8(~0x04U) },
   { "U",   input.kbd+5, u8(~0x08U) },
   { "Y",   input.kbd+5, u8(~0x10U) },

   { "L",   input.kbd+6, u8(~0x02U) },
   { "K",   input.kbd+6, u8(~0x04U) },
   { "J",   input.kbd+6, u8(~0x08U) },
   { "H",   input.kbd+6, u8(~0x10U) },

   { "M",   input.kbd+7, u8(~0x04U) },
   { "N",   input.kbd+7, u8(~0x08U) },
   { "B",   input.kbd+7, u8(~0x10U) },
};

static zxkey zxk_bk08[] =
{
   { "KRIGHT", &input.kjoy, u8(~1U) },
   { "KLEFT",  &input.kjoy, u8(~2U) },
   { "KDOWN",  &input.kjoy, u8(~4U) },
   { "KUP",    &input.kjoy, u8(~8U) },
   { "KFIRE",  &input.kjoy, u8(~16U)},

   { "ALT", input.kbd+0, u8(~0x01U) },
   { "Z",   input.kbd+0, u8(~0x02U) },
   { "X",   input.kbd+0, u8(~0x04U) },
   { "C",   input.kbd+0, u8(~0x08U) },
   { "V",   input.kbd+0, u8(~0x10U) },
   { "RUS", input.kbd+0, u8(~0x20U) },
   { "SHF", input.kbd+0, u8( 0x7FU) },

   { "A",   input.kbd+1, u8(~0x01U) },
   { "S",   input.kbd+1, u8(~0x02U) },
   { "D",   input.kbd+1, u8(~0x04U) },
   { "F",   input.kbd+1, u8(~0x08U) },
   { "G",   input.kbd+1, u8(~0x10U) },
   { "BSL", input.kbd+1, u8(~0x20U) },
   { "SL",  input.kbd+1, u8( 0x7FU) },

   { "Q",   input.kbd+2, u8(~0x01U) },
   { "W",   input.kbd+2, u8(~0x02U) },
   { "E",   input.kbd+2, u8(~0x04U) },
   { "R",   input.kbd+2, u8(~0x08U) },
   { "T",   input.kbd+2, u8(~0x10U) },
   { "CMA", input.kbd+2, u8(~0x20U) },
   { "PNT", input.kbd+2, u8( 0x7FU) },

   { "1",   input.kbd+3, u8(~0x01U) },
   { "2",   input.kbd+3, u8(~0x02U) },
   { "3",   input.kbd+3, u8(~0x04U) },
   { "4",   input.kbd+3, u8(~0x08U) },
   { "5",   input.kbd+3, u8(~0x10U) },
   { "TIL", input.kbd+3, u8(~0x20U) },
   { "TAB", input.kbd+3, u8( 0x7FU) },

   { "0",   input.kbd+4, u8(~0x01U) },
   { "9",   input.kbd+4, u8(~0x02U) },
   { "8",   input.kbd+4, u8(~0x04U) },
   { "7",   input.kbd+4, u8(~0x08U) },
   { "6",   input.kbd+4, u8(~0x10U) },
   { "MNS", input.kbd+4, u8(~0x20U) },
   { "PLS", input.kbd+4, u8( 0x7FU) },

   { "P",   input.kbd+5, u8(~0x01U) },
   { "O",   input.kbd+5, u8(~0x02U) },
   { "I",   input.kbd+5, u8(~0x04U) },
   { "U",   input.kbd+5, u8(~0x08U) },
   { "Y",   input.kbd+5, u8(~0x10U) },
   { "LB",  input.kbd+5, u8(~0x20U) },
   { "RB",  input.kbd+5, u8( 0x7FU) },

   { "ENT", input.kbd+6, u8(~0x01U) },
   { "L",   input.kbd+6, u8(~0x02U) },
   { "K",   input.kbd+6, u8(~0x04U) },
   { "J",   input.kbd+6, u8(~0x08U) },
   { "H",   input.kbd+6, u8(~0x10U) },
   { "COL", input.kbd+6, u8(~0x20U) },
   { "QUO", input.kbd+6, u8( 0x7FU) },

   { "SPC", input.kbd+7, u8(~0x01U) },
   { "CTL", input.kbd+7, u8(~0x02U) },
   { "M",   input.kbd+7, u8(~0x04U) },
   { "N",   input.kbd+7, u8(~0x08U) },
   { "B",   input.kbd+7, u8(~0x10U) },
   { "R/A", input.kbd+7, u8(~0x20U) },
   { "CPS", input.kbd+7, u8( 0x7FU) }
};

static zxkey zxk_quorum[] =
{
   { "KRIGHT", &input.kjoy, u8(~1U) },
   { "KLEFT",  &input.kjoy, u8(~2U) },
   { "KDOWN",  &input.kjoy, u8(~4U) },
   { "KUP",    &input.kjoy, u8(~8U) },
   { "KFIRE",  &input.kjoy, u8(~16U)},

   { "ENT", input.kbd+6, u8(~0x01U) },
   { "SPC", input.kbd+7, u8(~0x01U) },
   { "SYM", input.kbd+7, u8(~0x02U) },

   { "CAP", input.kbd+0, u8(~0x01U) },
   { "Z",   input.kbd+0, u8(~0x02U) },
   { "X",   input.kbd+0, u8(~0x04U) },
   { "C",   input.kbd+0, u8(~0x08U) },
   { "V",   input.kbd+0, u8(~0x10U) },

   { "A",   input.kbd+1, u8(~0x01U) },
   { "S",   input.kbd+1, u8(~0x02U) },
   { "D",   input.kbd+1, u8(~0x04U) },
   { "F",   input.kbd+1, u8(~0x08U) },
   { "G",   input.kbd+1, u8(~0x10U) },

   { "Q",   input.kbd+2, u8(~0x01U) },
   { "W",   input.kbd+2, u8(~0x02U) },
   { "E",   input.kbd+2, u8(~0x04U) },
   { "R",   input.kbd+2, u8(~0x08U) },
   { "T",   input.kbd+2, u8(~0x10U) },

   { "1",   input.kbd+3, u8(~0x01U) },
   { "2",   input.kbd+3, u8(~0x02U) },
   { "3",   input.kbd+3, u8(~0x04U) },
   { "4",   input.kbd+3, u8(~0x08U) },
   { "5",   input.kbd+3, u8(~0x10U) },

   { "0",   input.kbd+4, u8(~0x01U) },
   { "9",   input.kbd+4, u8(~0x02U) },
   { "8",   input.kbd+4, u8(~0x04U) },
   { "7",   input.kbd+4, u8(~0x08U) },
   { "6",   input.kbd+4, u8(~0x10U) },

   { "P",   input.kbd+5, u8(~0x01U) },
   { "O",   input.kbd+5, u8(~0x02U) },
   { "I",   input.kbd+5, u8(~0x04U) },
   { "U",   input.kbd+5, u8(~0x08U) },
   { "Y",   input.kbd+5, u8(~0x10U) },

   { "L",   input.kbd+6, u8(~0x02U) },
   { "K",   input.kbd+6, u8(~0x04U) },
   { "J",   input.kbd+6, u8(~0x08U) },
   { "H",   input.kbd+6, u8(~0x10U) },

   { "M",   input.kbd+7, u8(~0x04U) },
   { "N",   input.kbd+7, u8(~0x08U) },
   { "B",   input.kbd+7, u8(~0x10U) },

   // quorum additional keys
   { "RUS",    input.kbd+8, u8(~0x01U) },
   { "LAT",    input.kbd+8, u8(~0x02U) },
   { "N1",     input.kbd+8, u8(~0x08U) },
   { "N2",     input.kbd+8, u8(~0x10U) },
   { ".",      input.kbd+8, u8(~0x20U) },

   { "CAPS",   input.kbd+9, u8(~0x01U) },
   { "F2",     input.kbd+9, u8(~0x02U) },
   { "TILDA",  input.kbd+9, u8(~0x04U) },
   { "N4",     input.kbd+9, u8(~0x08U) },
   { "QUOTE",  input.kbd+9, u8(~0x10U) },
   { "N6",     input.kbd+9, u8(~0x20U) },

   { "TAB",    input.kbd+10, u8(~0x01U) },
   { "F4",     input.kbd+10, u8(~0x02U) },
   { "N7",     input.kbd+10, u8(~0x08U) },
   { "N5",     input.kbd+10, u8(~0x10U) },
   { "N9",     input.kbd+10, u8(~0x20U) },

   { "EBOX",   input.kbd+11, u8(~0x01U) },
   { "F5",     input.kbd+11, u8(~0x02U) },
   { "BS",     input.kbd+11, u8(~0x04U) },
   { "NSLASH", input.kbd+11, u8(~0x08U) },
   { "N8",     input.kbd+11, u8(~0x10U) },
   { "NMINUS", input.kbd+11, u8(~0x20U) },

   { "-",      input.kbd+12, u8(~0x01U) },
   { "+",      input.kbd+12, u8(~0x04U) },
   { "DEL",    input.kbd+12, u8(~0x08U) },
   { "NSTAR",  input.kbd+12, u8(~0x10U) },
   { "GBOX",   input.kbd+12, u8(~0x20U) },

   { "COLON",  input.kbd+13, u8(~0x01U) },
   { "F3",     input.kbd+13, u8(~0x02U) },
   { "\\",     input.kbd+13, u8(~0x04U) },
   { "]",      input.kbd+13, u8(~0x10U) },
   { "[",      input.kbd+13, u8(~0x20U) },

   { ",",      input.kbd+14, u8(~0x01U) },
   { "/",      input.kbd+14, u8(~0x10U) },
   { "N3",     input.kbd+14, u8(~0x20U) },

   { "F1",     input.kbd+15, u8(~0x02U) },
   { "N0",     input.kbd+15, u8(~0x08U) },
   { "NPOINT", input.kbd+15, u8(~0x10U) },
   { "NPLUS",  input.kbd+15, u8(~0x20U) },
};

zxkeymap zxk_maps[] =
{
   { "default", zxk_default, _countof(zxk_default) },
   { "BK08", zxk_bk08, _countof(zxk_bk08) },
   { "quorum", zxk_quorum, _countof(zxk_quorum) },
};

const size_t zxk_maps_count = _countof(zxk_maps);

PALETTEENTRY syspalette[0x100];

GDIBMP gdibmp = { { { sizeof(BITMAPINFOHEADER), 320, -240, 1, 8, BI_RGB, 0 } } };

PALETTE_OPTIONS pals[32] = {{"default",0x00,0x80,0xC0,0xE0,0xFF,0xC8,0xFF,0x00,0x00,0x00,0xFF,0x00,0x00,0x00,0xFF}};

#pragma pack()

unsigned char snbuf[4*1048576]; // large temporary buffer (for reading snapshots)
unsigned char gdibuf[2*1048576];

// on-screen watches block
uintptr_t watch_script[4][64];
unsigned char watch_enabled[4];
unsigned char used_banks[MAX_PAGES];
unsigned char trace_rom=1, trace_ram=1;

DWORD WinVerMajor;
DWORD WinVerMinor;
HWND dlg; // used in setcheck/getcheck: gui settings, monitor dialogs

HBITMAP hbm;  // bitmap for repaint background
DWORD bm_dx, bm_dy;
DWORD mousepos;  // left-clicked point in monitor
unsigned nowait; // don't close console window after error if started from GUI
bool normal_exit = false; // true on correct exit, false on failure exit

char *ayvols[64]; unsigned num_ayvols;
char *aystereo[64]; unsigned num_aystereo;
char *ulapreset[64]; unsigned num_ula;
char presetbuf[0x4000], *setptr = presetbuf;

/*
#include "fontdata.cpp"
#include "font8.cpp"
#include "font14.cpp"
#include "font16.cpp"
#include "fontatm2.cpp"
*/

const char * const ay_schemes[] = { "no soundchip", "single chip", "pseudo-turbo", "quadro-AY", "turbo-AY // POS", "turbo-sound // NedoPC" };

#include "std.h"

#include "emul.h"
#include "vars.h"
#include "draw.h"
#include "memory.h"
#include "tape.h"
#include "debug.h"
#include "sound.h"
#include "atm.h"
#include "gs.h"
#include "emulkeys.h"
#include "z80/op_system.h"
#include "z80/op_noprefix.h"
#include "fontatm2.h"
#include "sdcard.h"
#include "zc.h"

#include "util.h"

namespace z80gs
{
void flush_gs_z80();
}

#ifdef MOD_FASTCORE
   namespace z80fast
   {
      #include "z80_main.inl"
   }
#else
   #define z80fast z80dbg
#endif

#ifdef MOD_DEBUGCORE
   namespace z80dbg
   {
      #define Z80_DBG
      #include "z80_main.inl"
      #undef Z80_DBG
   }
#else
   #define z80dbg z80fast
#endif

void out(unsigned port, unsigned char val);

u8 __fastcall Xm(u32 addr)
{
    return z80fast::xm(addr);
}

u8 __fastcall Rm(u32 addr)
{
    return z80fast::rm(addr);
}

void __fastcall Wm(u32 addr, u8 val)
{
    z80fast::wm(addr, val);
}

u8 __fastcall DbgXm(u32 addr)
{
    return z80dbg::xm(addr);
}

u8 __fastcall DbgRm(u32 addr)
{
    return z80dbg::rm(addr);
}

void __fastcall DbgWm(u32 addr, u8 val)
{
    z80dbg::wm(addr, val);
}

void reset(ROM_MODE mode)
{
   comp.pEFF7 &= conf.EFF7_mask;
   comp.pEFF7 |= EFF7_GIGASCREEN; // [vv] disable turbo
   {
                conf.frame = frametime;
                cpu.SetTpi(conf.frame);
//                if ((conf.mem_model == MM_PENTAGON)&&(comp.pEFF7 & EFF7_GIGASCREEN))conf.frame = 71680; //removed 0.37
                apply_sound(); 
   } //Alone Coder 0.36.4
   comp.t_states = 0; comp.frame_counter = 0;
   comp.p7FFD = comp.pDFFD = comp.pFDFD = comp.p1FFD = 0;
   comp.p7EFD = 0;

   comp.p00 = comp.p80FD = 0; // quorum

   comp.pBF = 0; // ATM3
   comp.pBE = 0; // ATM3

   if (conf.mem_model == MM_ATM710 || conf.mem_model == MM_ATM3)
   {
       switch(mode)
       {
       case RM_DOS:
           // ������ �������, ������ cpm, ��������� ���������� ������
           // ��������� ������������ ����������, ���������� �������� ����������
           set_atm_FF77(0x4000 | 0x200 | 0x100, 0x80 | 0x40 | 0x20 | 3);
           comp.pFFF7[0] = 0x100 | 1; // trdos
           comp.pFFF7[1] = 0x200 | 5; // ram 5
           comp.pFFF7[2] = 0x200 | 2; // ram 2
           comp.pFFF7[3] = 0x200;     // ram 0

           comp.pFFF7[4] = 0x100 | 1; // trdos
           comp.pFFF7[5] = 0x200 | 5; // ram 5
           comp.pFFF7[6] = 0x200 | 2; // ram 2
           comp.pFFF7[7] = 0x200;     // ram 0
       break;
       default:
           set_atm_FF77(0,0);
       }
   }

   if (conf.mem_model == MM_ATM450)
   {
       switch(mode)
       {
       case RM_DOS:
           set_atm_aFE(0x80|0x60);
           comp.aFB = 0;
       break;
       default:
           set_atm_aFE(0x80);
           comp.aFB = 0x80;
       }
   }

   comp.flags = 0;
   comp.active_ay = 0;

   cpu.reset();
   reset_tape();
   ay[0].reset();
   ay[1].reset();
   Saa1099.reset();

   if (conf.sound.ay_scheme == AY_SCHEME_CHRV)
   {
        out(0xfffd,0xff); //0.36.7
        //printf("tfmstatuson0=%d\n",tfmstatuson0);
   };//Alone Coder

   #ifdef MOD_GS
   if (conf.sound.gsreset)
       reset_gs();
   #endif

   #ifdef MOD_VID_VD
   comp.vdbase = 0; comp.pVD = 0;
   #endif

   if (conf.mem_model == MM_ATM450 ||
       conf.mem_model == MM_ATM710 ||
       conf.mem_model == MM_ATM3 ||
       conf.mem_model == MM_PROFI)
       load_spec_colors();

   comp.ide_hi_byte_r = 0;
   comp.ide_hi_byte_w = 0;
   comp.ide_hi_byte_w1 = 0;
   hdd.reset();
   input.atm51.reset();
   Zc.Reset();

   if ((!conf.trdos_present && mode == RM_DOS) ||
       (!conf.cache && mode == RM_CACHE))
       mode = RM_SOS;

   set_mode(mode);
}

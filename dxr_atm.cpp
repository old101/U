#include "std.h"

#include "emul.h"
#include "vars.h"
#include "draw.h"
#include "dxrend.h"
#include "dxr_atm0.h"
#include "dxr_atm2.h"
#include "dxr_atm4.h"
#include "dxr_atm6.h"
#include "dxr_atm7.h"
#include "dxr_atmf.h"

typedef void (*TAtmRendFunc)(unsigned char *dst, unsigned pitch);

static void rend_atm_frame_small(unsigned char *dst, unsigned pitch)
{
     static const TAtmRendFunc AtmRendFunc[] =
     { rend_atmframe8, rend_atmframe16, 0, rend_atmframe32 };

     AtmRendFunc[temp.obpp/8-1](dst, pitch);
}

static void rend_atm_frame(unsigned char *dst, unsigned pitch)
{
    if (conf.fast_sl) {
        switch(temp.obpp)
        {
        case 8:
            rend_atmframe_8d1(dst, pitch);
            break;
        case 16:
            rend_atmframe_16d1(dst, pitch);
            break;
        case 32:
            rend_atmframe_32d1(dst, pitch);
            break;
        }
    } else {
        switch(temp.obpp)
        {
        case 8:
            rend_atmframe_8d(dst, pitch);
            break;
        case 16:
            rend_atmframe_16d(dst, pitch);
            break;
        case 32:
            rend_atmframe_32d(dst, pitch);
            break;
        }
    }

}

// atm2, atm3
void rend_atm_2_small(unsigned char *dst, unsigned pitch)
{
    if (temp.comp_pal_changed) {
        pixel_tables();
        temp.comp_pal_changed = 0;
    }

    if ( 3 == (comp.pFF77 & 7) ) //< Sinclair VideoMode
    {
        rend_small(dst, pitch);
        return;
    }

    if ( 7 == (comp.pFF77 & 7) ) //< Undocumented Sinclair Textmode VideoMode
    {
//        rend_atm7_small(dst, pitch);
        return;
    }

    if (temp.oy > temp.scy && conf.fast_sl) 
        pitch *= 2;
    rend_atm_frame_small(dst, pitch);

    for (int y=0; y<200; y++)
    {
        const AtmVideoController::ScanLine& Scanline = AtmVideoCtrl.Scanlines[y+56];
        switch (Scanline.VideoMode)
        {
        case 0: //< EGA 320x200
            rend_atm0_small(dst, pitch, y, Scanline.Offset);
            break;
/* 640x200
        case 2: // Hardware Multicolor
            rend_atm2(dst, pitch, y, Scanline.Offset);
            break;
        case 6: //< Textmode
            rend_atm6(dst, pitch, y, Scanline.Offset);
            break;
*/
        }
    }
}

// atm2, atm3 dbl
void rend_atm_2(unsigned char *dst, unsigned pitch)
{
    if (temp.comp_pal_changed) {
        pixel_tables();
        temp.comp_pal_changed = 0;
    }

    if ( 3 == (comp.pFF77 & 7) ) //< Sinclair VideoMode
    {
        rend_dbl(dst, pitch);
        return;
    }

    if ( 7 == (comp.pFF77 & 7) ) //< Undocumented Sinclair Textmode VideoMode
    {
        rend_atm7(dst, pitch);
        return;
    }
    
    if (temp.oy > temp.scy && conf.fast_sl) 
        pitch *= 2;
    rend_atm_frame(dst, pitch);

    for (int y=0; y<200; y++)
    {
        const AtmVideoController::ScanLine& Scanline = AtmVideoCtrl.Scanlines[y+56];
        switch (Scanline.VideoMode)
        {
        case 0: //< EGA 320x200
            rend_atm0(dst, pitch, y, Scanline.Offset);
            break;
        case 2: // Hardware Multicolor
            rend_atm2(dst, pitch, y, Scanline.Offset);
            break;
        case 4: // Textmode (vga like)
            if(conf.mem_model == MM_ATM3)
                rend_atm4(dst, pitch, y);
            break;
        case 6: //< Textmode
            rend_atm6(dst, pitch, y, Scanline.Offset);
            break;
        }
    }
}

// atm1 small
void rend_atm_1_small(unsigned char *dst, unsigned pitch)
{
   if (temp.comp_pal_changed) {
      pixel_tables();
      temp.comp_pal_changed = 0;
   }

   int VideoMode = (comp.aFE >> 5) & 3;
   if ( 3 == VideoMode ) //< Sinclair VideoMode
   {
       rend_small(dst, pitch);
       return;
   }

   if (temp.oy > temp.scy && conf.fast_sl) 
       pitch *= 2;
   rend_atm_frame_small(dst, pitch);

   for (int y=0; y<200; y++)
   {
       const AtmVideoController::ScanLine& Scanline = AtmVideoCtrl.Scanlines[y+56];
       switch (Scanline.VideoMode)
       {
       case 0: //< EGA 320x200
           rend_atm0_small(dst, pitch, y, Scanline.Offset);
           break;
/*
       case 1: // Hardware Multicolor
           rend_atm2(dst, pitch, y, Scanline.Offset);
           break;
*/
       }
   }
}

// atm1 dbl
void rend_atm_1(unsigned char *dst, unsigned pitch)
{
   if (temp.comp_pal_changed) {
      pixel_tables();
      temp.comp_pal_changed = 0;
   }

   int VideoMode = (comp.aFE >> 5) & 3;
   if ( 3 == VideoMode ) //< Sinclair VideoMode
   {
       rend_dbl(dst, pitch);
       return;
   }

   if (temp.oy > temp.scy && conf.fast_sl) 
       pitch *= 2;
   rend_atm_frame(dst, pitch);

   for (int y=0; y<200; y++)
   {
       const AtmVideoController::ScanLine& Scanline = AtmVideoCtrl.Scanlines[y+56];
       switch (Scanline.VideoMode)
       {
       case 0: //< EGA 320x200
           rend_atm0(dst, pitch, y, Scanline.Offset);
           break;
       case 1: // Hardware Multicolor
           rend_atm2(dst, pitch, y, Scanline.Offset);
           break;
       }
   }
}

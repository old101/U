#include "std.h"

#include "emul.h"
#include "vars.h"
#include "draw.h"
#include "dxrframe.h"
#include "dxr_4bpp.h"

// AlCo 4bpp mode
static const int p4bpp_ofs[] =
{
  0x00000000 - int(PAGE), 0x00004000 - int(PAGE), 0x00002000 - int(PAGE), 0x00006000 - int(PAGE),
  0x00000001 - int(PAGE), 0x00004001 - int(PAGE), 0x00002001 - int(PAGE), 0x00006001 - int(PAGE),
  0x00000002 - int(PAGE), 0x00004002 - int(PAGE), 0x00002002 - int(PAGE), 0x00006002 - int(PAGE),
  0x00000003 - int(PAGE), 0x00004003 - int(PAGE), 0x00002003 - int(PAGE), 0x00006003 - int(PAGE),
  0x00000004 - int(PAGE), 0x00004004 - int(PAGE), 0x00002004 - int(PAGE), 0x00006004 - int(PAGE),
  0x00000005 - int(PAGE), 0x00004005 - int(PAGE), 0x00002005 - int(PAGE), 0x00006005 - int(PAGE),
  0x00000006 - int(PAGE), 0x00004006 - int(PAGE), 0x00002006 - int(PAGE), 0x00006006 - int(PAGE),
  0x00000007 - int(PAGE), 0x00004007 - int(PAGE), 0x00002007 - int(PAGE), 0x00006007 - int(PAGE),
  0x00000008 - int(PAGE), 0x00004008 - int(PAGE), 0x00002008 - int(PAGE), 0x00006008 - int(PAGE),
  0x00000009 - int(PAGE), 0x00004009 - int(PAGE), 0x00002009 - int(PAGE), 0x00006009 - int(PAGE),
  0x0000000A - int(PAGE), 0x0000400A - int(PAGE), 0x0000200A - int(PAGE), 0x0000600A - int(PAGE),
  0x0000000B - int(PAGE), 0x0000400B - int(PAGE), 0x0000200B - int(PAGE), 0x0000600B - int(PAGE),
  0x0000000C - int(PAGE), 0x0000400C - int(PAGE), 0x0000200C - int(PAGE), 0x0000600C - int(PAGE),
  0x0000000D - int(PAGE), 0x0000400D - int(PAGE), 0x0000200D - int(PAGE), 0x0000600D - int(PAGE),
  0x0000000E - int(PAGE), 0x0000400E - int(PAGE), 0x0000200E - int(PAGE), 0x0000600E - int(PAGE),
  0x0000000F - int(PAGE), 0x0000400F - int(PAGE), 0x0000200F - int(PAGE), 0x0000600F - int(PAGE),
  0x00000010 - int(PAGE), 0x00004010 - int(PAGE), 0x00002010 - int(PAGE), 0x00006010 - int(PAGE),
  0x00000011 - int(PAGE), 0x00004011 - int(PAGE), 0x00002011 - int(PAGE), 0x00006011 - int(PAGE),
  0x00000012 - int(PAGE), 0x00004012 - int(PAGE), 0x00002012 - int(PAGE), 0x00006012 - int(PAGE),
  0x00000013 - int(PAGE), 0x00004013 - int(PAGE), 0x00002013 - int(PAGE), 0x00006013 - int(PAGE),
  0x00000014 - int(PAGE), 0x00004014 - int(PAGE), 0x00002014 - int(PAGE), 0x00006014 - int(PAGE),
  0x00000015 - int(PAGE), 0x00004015 - int(PAGE), 0x00002015 - int(PAGE), 0x00006015 - int(PAGE),
  0x00000016 - int(PAGE), 0x00004016 - int(PAGE), 0x00002016 - int(PAGE), 0x00006016 - int(PAGE),
  0x00000017 - int(PAGE), 0x00004017 - int(PAGE), 0x00002017 - int(PAGE), 0x00006017 - int(PAGE),
  0x00000018 - int(PAGE), 0x00004018 - int(PAGE), 0x00002018 - int(PAGE), 0x00006018 - int(PAGE),
  0x00000019 - int(PAGE), 0x00004019 - int(PAGE), 0x00002019 - int(PAGE), 0x00006019 - int(PAGE),
  0x0000001A - int(PAGE), 0x0000401A - int(PAGE), 0x0000201A - int(PAGE), 0x0000601A - int(PAGE),
  0x0000001B - int(PAGE), 0x0000401B - int(PAGE), 0x0000201B - int(PAGE), 0x0000601B - int(PAGE),
  0x0000001C - int(PAGE), 0x0000401C - int(PAGE), 0x0000201C - int(PAGE), 0x0000601C - int(PAGE),
  0x0000001D - int(PAGE), 0x0000401D - int(PAGE), 0x0000201D - int(PAGE), 0x0000601D - int(PAGE),
  0x0000001E - int(PAGE), 0x0000401E - int(PAGE), 0x0000201E - int(PAGE), 0x0000601E - int(PAGE),
  0x0000001F - int(PAGE), 0x0000401F - int(PAGE), 0x0000201F - int(PAGE), 0x0000601F - int(PAGE)
};

#define p4bpp8_nf p4bpp8

static int buf4bpp_shift = 0;

static void line_p4bpp_8(unsigned char *dst, unsigned char *src, unsigned *tab)
{
   u8 *d = (u8 *)dst;
   for (unsigned x = 0, i = 0; x < 256; x += 2, i++)
   {
       unsigned tmp = tab[src[p4bpp_ofs[(i+temp.offset_hscroll) & 0x7f]]];
       d[x]   = tmp;
       d[x+1] = tmp >> 16;
   }
}

static void line_p4bpp_8d(unsigned char *dst, unsigned char *src, unsigned *tab)
{
   for (unsigned x = 0; x < 128; x++)
   {
       *(unsigned*)(dst) = tab[src[p4bpp_ofs[(x+temp.offset_hscroll) & 0x7f]]];
       dst+=4;
   }
}

static void line_p4bpp_8d_nf(unsigned char *dst, unsigned char *src1, unsigned char *src2, unsigned *tab)
{
   for (unsigned x = 0; x < 128; x++)
   {
       *(unsigned*)(dst) =
        (tab[src1[p4bpp_ofs[(x+temp.offset_hscroll     ) & 0x7f]]] & 0x0F0F0F0F) +
        (tab[src2[p4bpp_ofs[(x+temp.offset_hscroll_prev) & 0x7f]]] & 0xF0F0F0F0);
       dst+=4;
   }
}

static void line_p4bpp_16(unsigned char *dst, unsigned char *src, unsigned *tab)
{
   u16 *d = (u16 *)dst;
   for (unsigned x = 0, i = 0; x < 256; x +=2, i++)
   {
       unsigned tmp = 2*src[p4bpp_ofs[(i+temp.offset_hscroll) & 0x7f]];
       d[x]   = tab[0+tmp];
       d[x+1] = tab[1+tmp];
   }
}

static void line_p4bpp_16d(unsigned char *dst, unsigned char *src, unsigned *tab)
{
   unsigned tmp;
   for (unsigned x = 0; x < 128; x++)
   {
       tmp = 2*src[p4bpp_ofs[(x+temp.offset_hscroll) & 0x7f]];
       *(unsigned*)dst = tab[0+tmp];
       dst+=4;
       *(unsigned*)dst = tab[1+tmp];
       dst+=4;
   }
}

static void line_p4bpp_16d_nf(unsigned char *dst, unsigned char *src1, unsigned char *src2, unsigned *tab)
{
   unsigned tmp1;
   unsigned tmp2;
   for (unsigned x = 0; x < 128; x++)
   {
       tmp1 = p4bpp_ofs[(x+temp.offset_hscroll     ) & 0x7f];
       tmp2 = p4bpp_ofs[(x+temp.offset_hscroll_prev) & 0x7f];
       *(unsigned*)dst = tab[0+2*src1[tmp1]] + tab[0+2*src2[tmp2]];
       dst+=4;
       *(unsigned*)dst = tab[1+2*src1[tmp1]] + tab[1+2*src2[tmp2]];
       dst+=4;
   }
}

static void line_p4bpp_32(unsigned char *dst, unsigned char *src, unsigned *tab)
{
   u32 *d = (u32 *)dst;
   for (unsigned x = 0, i = 0; x < 256; x += 2, i++)
   {
       unsigned tmp = 2*src[p4bpp_ofs[(i+temp.offset_hscroll) & 0x7f]];
       d[x]   = tab[0+tmp];
       d[x+1] = tab[1+tmp];
   }
}

static void line_p4bpp_32d(unsigned char *dst, unsigned char *src, unsigned *tab)
{
   unsigned tmp;
   for (unsigned x = 0; x < 128; x++)
   {
       tmp = 2*src[p4bpp_ofs[(x+temp.offset_hscroll) & 0x7f]];
       *(unsigned*)dst = *(unsigned*)(dst+4) = tab[0+tmp];
       dst+=8;
       *(unsigned*)dst = *(unsigned*)(dst+4) = tab[1+tmp];
       dst+=8;
   }
}

static void line_p4bpp_32d_nf(unsigned char *dst, unsigned char *src1, unsigned char *src2, unsigned *tab)
{
   unsigned tmp1;
   unsigned tmp2;
   for (unsigned x = 0; x < 128; x++)
   {
       tmp1 = p4bpp_ofs[(x+temp.offset_hscroll     ) & 0x7f];
       tmp2 = p4bpp_ofs[(x+temp.offset_hscroll_prev) & 0x7f];
       *(unsigned*)dst = *(unsigned*)(dst+4) = tab[0+2*src1[tmp1]] + tab[0+2*src2[tmp2]];
       dst+=8;
       *(unsigned*)dst = *(unsigned*)(dst+4) = tab[1+2*src1[tmp1]] + tab[1+2*src2[tmp2]];
       dst+=8;
   }
}

static void r_p4bpp_8(unsigned char *dst, unsigned pitch)
{
    for (unsigned y = 0; y < 192; y++)
    {
       unsigned char *src = temp.base + t.scrtab[(unsigned char)(y+temp.offset_vscroll)];
       line_p4bpp_8(dst, src, t.p4bpp8[0]); dst += pitch;
    }
}

static void r_p4bpp_8d1(unsigned char *dst, unsigned pitch)
{
   if (conf.noflic)
   {
      for (unsigned y = 0; y < 192; y++)
      {
         unsigned char *src1 = temp.base                 + t.scrtab[(unsigned char)(y+temp.offset_vscroll)];
         unsigned char *src2 = temp.base + buf4bpp_shift + t.scrtab[(unsigned char)(y+temp.offset_vscroll_prev)];
         line_p4bpp_8d_nf(dst, src1, src2, t.p4bpp8_nf[0]); dst += pitch;
      }

   }
   else
   {
      for (unsigned y = 0; y < 192; y++)
      {
         unsigned char *src = temp.base + t.scrtab[(unsigned char)(y+temp.offset_vscroll)];
         line_p4bpp_8d(dst, src, t.p4bpp8[0]); dst += pitch;
      }
   }
}

static void r_p4bpp_8d(unsigned char *dst, unsigned pitch)
{
   if (conf.noflic)
   {

      for (unsigned y = 0; y < 192; y++)
      {
         unsigned char *src1 = temp.base                 + t.scrtab[(unsigned char)(y+temp.offset_vscroll)];
         unsigned char *src2 = temp.base + buf4bpp_shift + t.scrtab[(unsigned char)(y+temp.offset_vscroll_prev)];
         line_p4bpp_8d_nf(dst, src1, src2, t.p4bpp8_nf[0]); dst += pitch;
         line_p4bpp_8d_nf(dst, src1, src2, t.p4bpp8_nf[1]); dst += pitch;
      }

   }
   else
   {

      for (unsigned y = 0; y < 192; y++)
      {
         unsigned char *src = temp.base + t.scrtab[(unsigned char)(y+temp.offset_vscroll)];
         line_p4bpp_8d(dst, src, t.p4bpp8[0]); dst += pitch;
         line_p4bpp_8d(dst, src, t.p4bpp8[1]); dst += pitch;
      }

   }
}

static void r_p4bpp_16(unsigned char *dst, unsigned pitch)
{
    for (unsigned y = 0; y < 192; y++)
    {
       unsigned char *src = temp.base + t.scrtab[(unsigned char)(y+temp.offset_vscroll)];
       line_p4bpp_16(dst, src, t.p4bpp16[0]);
       dst += pitch;
    }
}

static void r_p4bpp_16d1(unsigned char *dst, unsigned pitch)
{
   if (conf.noflic)
   {
      for (unsigned y = 0; y < 192; y++)
      {
         unsigned char *src1 = temp.base                 + t.scrtab[(unsigned char)(y+temp.offset_vscroll)];
         unsigned char *src2 = temp.base + buf4bpp_shift + t.scrtab[(unsigned char)(y+temp.offset_vscroll_prev)];
         line_p4bpp_16d_nf(dst, src1, src2, t.p4bpp16_nf[0]); dst += pitch;
      }

   }
   else
   {
      for (unsigned y = 0; y < 192; y++)
      {
         unsigned char *src = temp.base + t.scrtab[(unsigned char)(y+temp.offset_vscroll)];
         line_p4bpp_16d(dst, src, t.p4bpp16[0]); dst += pitch;
      }

   }
}

static void r_p4bpp_16d(unsigned char *dst, unsigned pitch)
{
   if (conf.noflic)
   {
      for (unsigned y = 0; y < 192; y++)
      {
         unsigned char *src1 = temp.base                 + t.scrtab[(unsigned char)(y+temp.offset_vscroll)];
         unsigned char *src2 = temp.base + buf4bpp_shift + t.scrtab[(unsigned char)(y+temp.offset_vscroll_prev)];
         line_p4bpp_16d_nf(dst, src1, src2, t.p4bpp16_nf[0]); dst += pitch;
         line_p4bpp_16d_nf(dst, src1, src2, t.p4bpp16_nf[1]); dst += pitch;
      }

   }
   else
   {
      for (unsigned y = 0; y < 192; y++)
      {
         unsigned char *src = temp.base + t.scrtab[(unsigned char)(y+temp.offset_vscroll)];
         line_p4bpp_16d(dst, src, t.p4bpp16[0]); dst += pitch;
         line_p4bpp_16d(dst, src, t.p4bpp16[1]); dst += pitch;
      }
   }
}

static void r_p4bpp_32(unsigned char *dst, unsigned pitch)
{
    for (unsigned y = 0; y < 192; y++)
    {
       unsigned char *src = temp.base + t.scrtab[(unsigned char)(y+temp.offset_vscroll)];
       line_p4bpp_32(dst, src, t.p4bpp32[0]);
       dst += pitch;
    }
}

static void r_p4bpp_32d1(unsigned char *dst, unsigned pitch)
{
   if (conf.noflic)
   {
      for (unsigned y = 0; y < 192; y++)
      {
         unsigned char *src1 = temp.base                 + t.scrtab[(unsigned char)(y+temp.offset_vscroll)];
         unsigned char *src2 = temp.base + buf4bpp_shift + t.scrtab[(unsigned char)(y+temp.offset_vscroll_prev)];
         line_p4bpp_32d_nf(dst, src1, src2, t.p4bpp32_nf[0]); dst += pitch;
      }

   }
   else
   {
      for (unsigned y = 0; y < 192; y++)
      {
         unsigned char *src = temp.base + t.scrtab[(unsigned char)(y+temp.offset_vscroll)];
         line_p4bpp_32d(dst, src, t.p4bpp32[0]); dst += pitch;
      }
   }
}

static void r_p4bpp_32d(unsigned char *dst, unsigned pitch)
{
   if (conf.noflic)
   {
      for (unsigned y = 0; y < 192; y++)
      {
         unsigned char *src1 = temp.base                 + t.scrtab[(unsigned char)(y+temp.offset_vscroll)];
         unsigned char *src2 = temp.base + buf4bpp_shift + t.scrtab[(unsigned char)(y+temp.offset_vscroll_prev)];
         line_p4bpp_32d_nf(dst, src1, src2, t.p4bpp32_nf[0]); dst += pitch;
         line_p4bpp_32d_nf(dst, src1, src2, t.p4bpp32_nf[1]); dst += pitch;
      }
   }
   else
   {
      for (unsigned y = 0; y < 192; y++)
      {
         unsigned char *src = temp.base + t.scrtab[(unsigned char)(y+temp.offset_vscroll)];
         line_p4bpp_32d(dst, src, t.p4bpp32[0]); dst += pitch;
         line_p4bpp_32d(dst, src, t.p4bpp32[1]); dst += pitch;
      }
   }
}

void rend_p4bpp_small(unsigned char *dst, unsigned pitch)
{
   unsigned char *dst2 = dst + (temp.ox-256)*temp.obpp/16;
   dst2 += temp.b_top * pitch * ((temp.oy > temp.scy)?2:1);

   if (temp.oy > temp.scy && conf.fast_sl)
       pitch *= 2;

   if (conf.noflic)
       buf4bpp_shift = (rbuf_s+PAGE) - temp.base;

   if (temp.obpp == 8) 
   {
       rend_frame8(dst, pitch);
       r_p4bpp_8(dst2, pitch);
   }
   if (temp.obpp == 16)
   {
       rend_frame16(dst, pitch);
       r_p4bpp_16(dst2, pitch);
   }
   if (temp.obpp == 32)
   {
       rend_frame32(dst, pitch);
       r_p4bpp_32(dst2, pitch);
   }

   if (conf.noflic)
       memcpy(rbuf_s, temp.base-PAGE, 2*PAGE);

   temp.offset_vscroll_prev = temp.offset_vscroll;
   temp.offset_vscroll = 0;
   temp.offset_hscroll_prev = temp.offset_hscroll;
   temp.offset_hscroll = 0;
}

void rend_p4bpp(unsigned char *dst, unsigned pitch)
{
   unsigned char *dst2 = dst + (temp.ox-512)*temp.obpp/16;
   dst2 += temp.b_top * pitch * ((temp.oy > temp.scy)?2:1);

   if (temp.oy > temp.scy && conf.fast_sl) pitch *= 2;

   if (conf.noflic)
       buf4bpp_shift = (rbuf_s+PAGE) - temp.base;

   if (temp.obpp == 8) 
   {
       if (conf.fast_sl)
       {
           rend_frame_8d1(dst, pitch);
           r_p4bpp_8d1(dst2, pitch);
       }
       else
       {
           rend_frame_8d(dst, pitch);
           r_p4bpp_8d(dst2, pitch);
       }
   }
   if (temp.obpp == 16)
   {
       if (conf.fast_sl)
       {
           rend_frame_16d1(dst, pitch);
           r_p4bpp_16d1(dst2, pitch);
       }
       else
       {
           rend_frame_16d(dst, pitch);
           r_p4bpp_16d(dst2, pitch);
       }
   }
   if (temp.obpp == 32)
   {
       if (conf.fast_sl)
       {
           rend_frame_32d1(dst, pitch);
           r_p4bpp_32d1(dst2, pitch);
       }
       else
       {
           rend_frame_32d(dst, pitch);
           r_p4bpp_32d(dst2, pitch);
       }
   }

   if (conf.noflic)
       memcpy(rbuf_s, temp.base-PAGE, 2*PAGE);

   temp.offset_vscroll_prev = temp.offset_vscroll;
   temp.offset_vscroll = 0;
   temp.offset_hscroll_prev = temp.offset_hscroll;
   temp.offset_hscroll = 0;
}

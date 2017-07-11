#include "std.h"

#include "emul.h"
#include "vars.h"
#include "draw.h"
#include "drawnomc.h"

void draw_border()
{
   unsigned br = comp.border_attr * 0x11001100;
   for (unsigned i = 0; i < temp.scx*temp.scy/4; i+=4)
      *(unsigned*)(rbuf + i) = br;
}

void draw_alco();
void draw_gigascreen_no_border();

void draw_screen()
{
/* [vv] Отключен, т.к. этот бит используется для DDp scroll
   if (comp.pEFF7 & EFF7_384)
   {
       draw_alco();
       return;
   }
*/

   if (conf.nopaper)
   {
       draw_border();
       return;
   }
//   if (comp.pEFF7 & EFF7_GIGASCREEN) { draw_border(); draw_gigascreen_no_border(); return; } //Alone Coder

   unsigned char *dst = rbuf;
   unsigned br = comp.border_attr * 0x11001100; // Размножение атрибута 0xbb00bb00, где b - цвет бордюра

   unsigned i;
   // Верхний бордюр (предполагается, что temp.scx кратно 16 точкам)
   for (i = temp.b_top * temp.scx / 16; i; i--)
   {
      *(unsigned*)dst = br; // Обработка сразу двух знакомест (пиксели/pc-атрибуты)
      dst += 4;
   }

   // Экран
   for (int y = 0; y < 192; y++)
   {
      unsigned x;
	  // Левый бордюр (в пределах 192 строк экрана)
	  for (x = temp.b_left; x; x -= 8) // Цикл идет по знакоместам
      {
         *(u16 *)dst = br; // Запись точек (на бордюре их нет) и атрибутов (цвет бордюра) (0xbb00)
         dst += sizeof(u16);
      }

	  // Экранная область (центральная часть)
      for (x = 0; x < 32; x++) // Цикл идет по знакоместам
      {
         *dst++ = temp.base[t.scrtab[y] + x]; // Точки (8штук)
         *dst++ = colortab[temp.base[atrtab[y] + x]]; // pc-атрибуты
      }

      // Правый бордюр (в пределах 192 строк экрана)
	  for (x = temp.b_right; x; x -= 8) // Цикл идет по знакоместам
      {
         *(u16 *)dst = br; // Запись точек (на бордюре их нет) и атрибутов (цвет бордюра) (0xbb00)
         dst += sizeof(u16);
      }
   }

   // Нижний бордюр (предполагается, что temp.scx кратно 16 точкам)
   for (i = temp.b_bottom*temp.scx / 16; i; i--)
   {
      *(unsigned*)dst = br; // Обработка сразу двух знакомест (пиксели/pc-атрибуты)
      dst += 4;
   }
}

void draw_gigascreen_no_border()
{
   unsigned char *dst = rbuf + (temp.b_top * temp.scx + temp.b_left) / 4;
   unsigned char * const screen = RAM_BASE_M + 5*PAGE;
   unsigned offset = (comp.frame_counter & 1)? 0 : 2*PAGE;
   for (int y = 0; y < 192; y++) {
      *(volatile unsigned char*)dst;
      for (unsigned x = 0; x < 32; x++) {
         dst[2*x+0] = screen[t.scrtab[y] + x + offset];
         dst[2*x+1] = colortab[screen[atrtab[y] + x + offset]];
      }
      offset ^= 2*PAGE;
      dst += temp.scx / 4;
   }
}

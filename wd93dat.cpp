#include "std.h"

#include "emul.h"
#include "vars.h"
#include "snapshot.h"

#include "util.h"

int FDD::index()
{
   return this - comp.wd.fdd;
}

// return: 0 - CANCEL, 1 - YES/SAVED, 2 - NOT SAVED
char FDD::test()
{
   if (!optype) return 1;

   static char changed[] = "Disk A: changed. Save ?"; changed[5] = index()+'A';
   int r = MessageBox(GetForegroundWindow(), changed, "Save disk", exitflag? MB_ICONQUESTION | MB_YESNO : MB_ICONQUESTION | MB_YESNOCANCEL);
   if (r == IDCANCEL) return 0;
   if (r == IDNO) return 2;
   // r == IDYES
   unsigned char *image = (unsigned char*)malloc(sizeof snbuf);
   memcpy(image, snbuf, sizeof snbuf);
   savesnap(index());
   memcpy(snbuf, image, sizeof snbuf);
   ::free(image);

   if (*(volatile char*)&optype) return 0;
   return 1;
}

void FDD::free()
{
   if (rawdata)
       VirtualFree(rawdata, 0, MEM_RELEASE);

   motor = 0;
   track = 0;

   rawdata = 0;
   rawsize = 0;
   cyls = 0;
   sides = 0;
   name[0] = 0;
   dsc[0] = 0;
   memset(trklen, 0,  sizeof(trklen));
   memset(trkd, 0,  sizeof(trkd));
   memset(trki, 0,  sizeof(trki));
   memset(trkwp, 0,  sizeof(trkwp));
   optype = 0;
   snaptype = 0;
   conf.trdos_wp[index()] = 0;
   /*comp.wd.trkcache.clear(); [vv]*/
   t.clear();
}

void FDD::newdisk(unsigned cyls, unsigned sides)
{
   free();

   FDD::cyls = cyls; FDD::sides = sides;
   unsigned len = MAX_TRACK_LEN;
   unsigned bitmap_len = unsigned(len+7U) >> 3;
   unsigned len2 = len + bitmap_len*2;
   rawsize = align_by(cyls * sides * len2, 4096);
   rawdata = (unsigned char*)VirtualAlloc(0, rawsize, MEM_COMMIT, PAGE_READWRITE);
   // ZeroMemory(rawdata, rawsize); // already done by VirtualAlloc

   for (unsigned c = 0; c < cyls; c++)
   {
      for (unsigned h = 0; h < sides; h++)
      {
         trklen[c][h] = len;
         trkd[c][h] = rawdata + len2*(c*sides + h);
         trki[c][h] = trkd[c][h] + len;
         trkwp[c][h] = trkd[c][h] + len + bitmap_len;
      }
   }
   // comp.wd.trkcache.clear(); // already done in free()
}

int FDD::read(unsigned char type)
{
   int ok = 0;
   switch(type)
   {
   case snTRD: ok = read_trd(); break;
   case snUDI: ok = read_udi(); break;
   case snHOB: ok = read_hob(); break;
   case snSCL: ok = read_scl(); break;
   case snFDI: ok = read_fdi(); break;
   case snTD0: ok = read_td0(); break;
   case snISD: ok = read_isd(); break;
   case snPRO: ok = read_pro(); break;
   }
   return ok;
}

bool done_fdd(bool Cancelable)
{
   for(int i = 0; i < 4; i++)
      if(!comp.wd.fdd[i].test() && Cancelable)
          return false;
   return true;
}

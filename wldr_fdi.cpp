#include "std.h"

#include "emul.h"
#include "vars.h"

#include "util.h"
#include "wd93crc.h "

#pragma pack(push, 1)
struct TFdiSecHdr
{
    enum
    {
        FL_DELETED_DATA = 0x80,
        FL_NO_DATA = 0x40,
        FL_GOOD_CRC_4096 = 0x20,
        FL_GOOD_CRC_2048 = 0x10,
        FL_GOOD_CRC_1024 = 0x8,
        FL_GOOD_CRC_512 = 0x4,
        FL_GOOD_CRC_256 = 0x2,
        FL_GOOD_CRC_128 = 0x1
    };

    u8 c;
    u8 h;
    u8 r;
    u8 n;
    // flags:
    // bit 7 - 1 = deleted data (F8) / 0 = normal data (FB)
    // bit 6 - 1 - sector with no data
    // bits 0..5 - 1 = good crc for sector size (128, 256, 512, 1024, 2048, 4096)
    u8 fl;
    u16 DataOffset;
};

struct TFdiTrkHdr
{
    u32 TrkOffset;
    u16 Res1;
    u8  Spt;
    TFdiSecHdr Sec[];
};

struct TFdiHdr
{
    char Sig[3];
    u8 Rw;
    u16 c;
    u16 h;
    u16 TextOffset;
    u16 DataOffset;
    u16 AddLen;
    u8 AddData[]; // AddLen -> TFdiAddInfo
//   TFdiTrkHdr Trk[c*h];
};

struct TFdiAddInfo
{
    enum { BAD_BYTES = 1 };
    enum { FDI_2 = 2 };
    u16 Ver; // 2 - FDI 2
    u16 AddInfoType; // 1 - bad bytes info
    u32 TrkAddInfoOffset; // -> TFdiTrkAddInfo
    u32 DataOffset;
};

struct TFdiSecAddInfo
{
    u8 Flags; // 1 - Массив сбойных байтов присутствует
    // Смещение битового массива сбойных байтов внутри трэка
    // Число битов определяется размером сектора
    // Один бит соответствует одному сбойному байту
    u16 DataOffset;
};

struct TFdiTrkAddInfo
{
    u32 TrkOffset; // Смещение массива сбойных байтов для трэка относительно TFdiAddInfo->DataOffset,
                   // 0xFFFFFFFF - Массив описателей параметров секторов отсутствует
    TFdiSecAddInfo Sec[]; // Spt
};
#pragma pack(pop)


int FDD::read_fdi()
{
   const TFdiHdr *FdiHdr = (const TFdiHdr *)snbuf;
   unsigned cyls = FdiHdr->c;
   unsigned sides = FdiHdr->h;
   unsigned AddLen = FdiHdr->AddLen;

   if(cyls > MAX_CYLS)
   {
       err_printf("cylinders (%u) > MAX_CYLS(%d)", cyls, MAX_CYLS);
       return 0;
   }

   if(sides > 2)
   {
       err_printf("sides (%u) > 2", sides);
       return 0;
   }

   newdisk(cyls, sides);

   const TFdiAddInfo *FdiAddInfo = 0;
   const TFdiTrkAddInfo *FdiTrkAddInfo = 0;
   if(AddLen >= sizeof(TFdiAddInfo))
   {
       // Проверить параметры FdiAddInfo (версию, тип и т.д.)
       FdiAddInfo = (const TFdiAddInfo *)FdiHdr->AddData;
       if(FdiAddInfo->Ver >= TFdiAddInfo::FDI_2 && FdiAddInfo->AddInfoType == TFdiAddInfo::BAD_BYTES)
       {
           FdiTrkAddInfo = (const TFdiTrkAddInfo *)(snbuf + FdiAddInfo->TrkAddInfoOffset);
       }
   }

   strncpy(dsc, (const char *)&snbuf[FdiHdr->TextOffset], sizeof(dsc));
   dsc[sizeof(dsc) - 1] = 0;

   int res = 1;
   const TFdiTrkHdr *FdiTrkHdr = (const TFdiTrkHdr *)&snbuf[sizeof(TFdiHdr) + FdiHdr->AddLen];
   u8 *dat = snbuf + FdiHdr->DataOffset;

   for (unsigned c = 0; c < cyls; c++)
   {
      for (unsigned s = 0; s < sides; s++)
      {
         t.seek(this, c,s, JUST_SEEK);

         u8 *t0 = dat + FdiTrkHdr->TrkOffset;
         unsigned ns = FdiTrkHdr->Spt;
         u8 *wp0 = 0;
         if(FdiTrkAddInfo && FdiTrkAddInfo->TrkOffset != UINT_MAX)
         {
             wp0 = snbuf + FdiAddInfo->DataOffset + FdiTrkAddInfo->TrkOffset;
             if(wp0 >  snbuf + snapsize)
             {
                 err_printf("bad bytes data is beyond disk image end");
                 return 0;
             }
         }

         for (unsigned sec = 0; sec < ns; sec++)
         {
            t.hdr[sec].c = FdiTrkHdr->Sec[sec].c;
            t.hdr[sec].s = FdiTrkHdr->Sec[sec].h;
            t.hdr[sec].n = FdiTrkHdr->Sec[sec].r;
            t.hdr[sec].l = FdiTrkHdr->Sec[sec].n;
            t.hdr[sec].c1 = 0;
            t.hdr[sec].wp = 0;

            if (FdiTrkHdr->Sec[sec].fl & TFdiSecHdr::FL_NO_DATA)
                t.hdr[sec].data = 0;
            else
            {
               if (t0 + FdiTrkHdr->Sec[sec].DataOffset > snbuf + snapsize)
               {
                   err_printf("sector data is beyond disk image end");
                   return 0;
               }
               t.hdr[sec].data = t0 + FdiTrkHdr->Sec[sec].DataOffset;

               if(FdiTrkAddInfo && FdiTrkAddInfo->TrkOffset != UINT_MAX)
               {
                   t.hdr[sec].wp = ((FdiTrkAddInfo->Sec[sec].Flags & 1) ? (wp0 + FdiTrkAddInfo->Sec[sec].DataOffset) : 0);
               }
#if 0
               if(FdiTrkHdr->Sec[sec].n > 3)
               {
                   u8 buf[1+1024];
                   buf[0] = (FdiTrkHdr->Sec[sec].fl & TFdiSecHdr::FL_DELETED_DATA) ? 0xF8 : 0xFB;
                   memcpy(buf+1, t.hdr[sec].data, 128U << (FdiTrkHdr->Sec[sec].n & 3));

                   u16 crc_calc = wd93_crc(buf, (128U << (FdiTrkHdr->Sec[sec].n & 3)) + 1);
                   u16 crc_from_hdr = *(u16*)(t.hdr[sec].data+(128U << (FdiTrkHdr->Sec[sec].n & 3)));
                   printf("phys: c=%-2u, h=%u, s=%u | hdr: c=0x%02X, h=0x%02X, r=0x%02X, n=%02X(%u) | crc1=0x%04X, crc2=0x%04X\n",
                       c, s, sec,
                       FdiTrkHdr->Sec[sec].c, FdiTrkHdr->Sec[sec].h, FdiTrkHdr->Sec[sec].r, FdiTrkHdr->Sec[sec].n, (FdiTrkHdr->Sec[sec].n & 3),
                       crc_calc, crc_from_hdr);

                   if(crc_calc == crc_from_hdr)
                   {
                       TFdiTrkHdr *FdiTrkHdrRW = const_cast<TFdiTrkHdr *>(FdiTrkHdr);
                       FdiTrkHdrRW->Sec[sec].fl |= (1<<(FdiTrkHdr->Sec[sec].n & 3));

                   }
               }
#endif
               t.hdr[sec].c2 = (FdiTrkHdr->Sec[sec].fl & (1<<(FdiTrkHdr->Sec[sec].n & 3))) ? 0:2; // [vv]
            }
/* [vv]
            if (t.hdr[sec].l>5)
            {
                t.hdr[sec].data = 0;
                if (!(trk[4] & 0x40))
                    res = 0;
            }
*/
         } // sec
         t.s = ns;
         t.format();

         if(FdiTrkAddInfo)
         {
             FdiTrkAddInfo = (const TFdiTrkAddInfo *)(((const u8 *)FdiTrkAddInfo) + sizeof(TFdiTrkAddInfo) +
              ((FdiTrkAddInfo->TrkOffset != UINT_MAX) ? FdiTrkHdr->Spt * sizeof(TFdiSecAddInfo) : 0));
         }

         FdiTrkHdr = (const TFdiTrkHdr *)(((const u8 *)FdiTrkHdr) + sizeof(TFdiTrkHdr) + FdiTrkHdr->Spt * sizeof(TFdiSecHdr));
      } // s
   }
   return res;
}

int FDD::write_fdi(FILE *ff)
{
   unsigned b, c, s, se, total_s = 0;
   unsigned sectors_wp = 0; // Общее число секторов для которых пишутся заголовки с дополнительной информацией
   unsigned total_size = 0; // Общий размер данных занимаемый секторами

   // Подсчет общего числа секторов на диске
   for (c = 0; c < cyls; c++)
   {
      for (s = 0; s < sides; s++)
      {
         t.seek(this, c, s, LOAD_SECTORS);
         for(se = 0; se < t.s; se++)
         {
             total_size += (t.hdr[se].data ? t.hdr[se].datlen : 0);
         }
         for(se = 0; se < t.s; se++)
         {
             if(t.hdr[se].wp_start)
             {
                 sectors_wp += t.s;
                 break;
             }
         }
         total_s += t.s;
      }
   }

   unsigned AddLen = sectors_wp ? sizeof(TFdiAddInfo) : 0;
   unsigned tlen = strlen(dsc)+1;
   unsigned hsize = sizeof(TFdiHdr) + AddLen + cyls * sides * sizeof(TFdiTrkHdr) + total_s * sizeof(TFdiSecHdr);
   unsigned AddHdrsSize = cyls * sides * sizeof(TFdiTrkAddInfo) + sectors_wp * sizeof(TFdiSecAddInfo);

   // Формирование FDI заголовка
   TFdiHdr *FdiHdr = (TFdiHdr *)snbuf;
   memcpy(FdiHdr->Sig, "FDI", 3);
   FdiHdr->Rw = 0;
   FdiHdr->c = cyls;
   FdiHdr->h = sides;
   FdiHdr->TextOffset = hsize;
   FdiHdr->DataOffset = FdiHdr->TextOffset + tlen;
   FdiHdr->AddLen = AddLen;

   TFdiAddInfo *FdiAddInfo = (TFdiAddInfo *)FdiHdr->AddData;
   if(AddLen)
   {
       FdiAddInfo->Ver = TFdiAddInfo::FDI_2; // FDI ver 2
       FdiAddInfo->AddInfoType = TFdiAddInfo::BAD_BYTES; // Информация о сбойных байтах
       FdiAddInfo->TrkAddInfoOffset = FdiHdr->DataOffset + total_size;
       FdiAddInfo->DataOffset = FdiAddInfo->TrkAddInfoOffset + AddHdrsSize;
   }

   // Запись FDI заголовка с дополнительными данными
   if(fwrite(FdiHdr, sizeof(TFdiHdr) + AddLen, 1, ff) != 1)
       return 0;

   unsigned trkoffs = 0;
   for (c = 0; c < cyls; c++)
   {
      for (s = 0; s < sides; s++)
      {
         t.seek(this, c, s, LOAD_SECTORS);

         // Формирование заголовка трэка
         TFdiTrkHdr FdiTrkHdr;
         FdiTrkHdr.TrkOffset = trkoffs;
         FdiTrkHdr.Res1 = 0;
         FdiTrkHdr.Spt = t.s;

         // Запись заголовка трэка
         if(fwrite(&FdiTrkHdr, sizeof(FdiTrkHdr), 1, ff) != 1)
             return 0;

         unsigned secoffs = 0;
         for (se = 0; se < t.s; se++)
         {
           // Формирование заголовка сектора
           TFdiSecHdr FdiSecHdr;
           FdiSecHdr.c = t.hdr[se].c;
           FdiSecHdr.h = t.hdr[se].s;
           FdiSecHdr.r = t.hdr[se].n;
           FdiSecHdr.n = t.hdr[se].l;
           FdiSecHdr.fl = 0;

           if(t.hdr[se].data)
           {
               if(t.hdr[se].data[-1] == 0xF8)
                  FdiSecHdr.fl |= TFdiSecHdr::FL_DELETED_DATA;
               else
                  FdiSecHdr.fl |= (t.hdr[se].c2 ? (1<<(t.hdr[se].l & 3)) : 0); // [vv]
           }
           else
           {
               FdiSecHdr.fl |= TFdiSecHdr::FL_NO_DATA;
           }

           FdiSecHdr.DataOffset = secoffs;


            // Запись заголовка сектора
            if(fwrite(&FdiSecHdr, sizeof(FdiSecHdr), 1, ff) != 1)
                return 0;
            secoffs += t.hdr[se].datlen;
         }
         trkoffs += secoffs;
      }
   }

   // Запись комментария
   fseek(ff, FdiHdr->TextOffset, SEEK_SET);
   if(fwrite(dsc, tlen, 1, ff) != 1)
       return 0;

   // Запись зон данных трэков
   for (c = 0; c < cyls; c++)
   {
      for (s = 0; s < sides; s++)
      {
         t.seek(this, c, s, LOAD_SECTORS);
         for (unsigned se = 0; se < t.s; se++)
         {
            if (t.hdr[se].data)
            {
               if (fwrite(t.hdr[se].data, t.hdr[se].datlen, 1, ff) != 1)
                   return 0;
            }
         }
      }
   }

   // Запись дополниетльной информации (информации о сбойных байтах)
   if(AddLen)
   {
       trkoffs = 0;
       for (c = 0; c < cyls; c++)
       {
          for (s = 0; s < sides; s++)
          {
             t.seek(this, c, s, LOAD_SECTORS);

             // Формирование заголовка трэка
             TFdiTrkAddInfo FdiTrkAddInfo;
             FdiTrkAddInfo.TrkOffset = UINT_MAX;
             for(b = 0; b < t.trklen; b++)
             {
                 if(t.test_wp(b))
                 {
                     FdiTrkAddInfo.TrkOffset = trkoffs;
                     break;
                 }
             }

             // Запись заголовка трэка
             if(fwrite(&FdiTrkAddInfo, sizeof(FdiTrkAddInfo), 1, ff) != 1)
                 return 0;

             unsigned secoffs = 0;
             if(FdiTrkAddInfo.TrkOffset != UINT_MAX)
             {
                 for (se = 0; se < t.s; se++)
                 {
                   // Формирование заголовка сектора
                   TFdiSecAddInfo FdiSecAddInfo;
                   FdiSecAddInfo.Flags = 0;
                   FdiSecAddInfo.DataOffset = 0;

                   if(t.hdr[se].wp_start)
                   {
                       FdiSecAddInfo.Flags |= 1;
                       FdiSecAddInfo.DataOffset = secoffs;
                   }

                    // Запись заголовка сектора
                    if(fwrite(&FdiSecAddInfo, sizeof(FdiSecAddInfo), 1, ff) != 1)
                        return 0;
                    secoffs += (t.hdr[se].wp_start ? ((t.hdr[se].datlen + 7) >> 3) : 0);
                 }
             }
             trkoffs += secoffs;
          }
       }

       // Запись зон сбойных байтов
       for (c = 0; c < cyls; c++)
       {
          for (s = 0; s < sides; s++)
          {
             t.seek(this, c, s, LOAD_SECTORS);
             for (unsigned se = 0; se < t.s; se++)
             {
                if (t.hdr[se].wp_start)
                {
                   unsigned nbits = t.hdr[se].datlen;

                   u8 wp_bits[1024U >> 3U] = { 0 };
                   for(b = 0; b < nbits; b++)
                   {
                       if(t.test_wp(t.hdr[se].wp_start + b))
                       {
                           set_bit(wp_bits, b);
                       }
                   }
                   if (fwrite(wp_bits, (nbits + 7) >> 3, 1, ff) != 1)
                       return 0;
                }
             }
          }
       }
   }

   return 1;
}

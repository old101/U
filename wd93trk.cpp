#include "std.h"

#include "emul.h"
#include "vars.h"
#include "wd93crc.h"

#include "util.h"

void TRKCACHE::seek(FDD *d, unsigned cyl, unsigned side, SEEK_MODE fs)
{
   if ((d == drive) && (sf == fs) && (cyl == TRKCACHE::cyl) && (side == TRKCACHE::side))
       return;

   drive = d; sf = fs; s = 0;
   TRKCACHE::cyl = cyl; TRKCACHE::side = side;
   if (cyl >= d->cyls || !d->rawdata)
   {
       trkd = 0;
       return;
   }

   assert(cyl < MAX_CYLS);
   trkd = d->trkd[cyl][side];
   trki = d->trki[cyl][side];
   trkwp = d->trkwp[cyl][side];
   trklen = d->trklen[cyl][side];
   if (!trklen)
   {
       trkd = 0;
       return;
   }

   ts_byte = Z80FQ / (trklen * FDD_RPS);
   if (fs == JUST_SEEK)
       return; // else find sectors

   for (unsigned i = 0; i < trklen - 8; i++)
   {
      if (trkd[i] != 0xA1 || trkd[i+1] != 0xFE || !test_i(i)) // ����� idam
          continue;

      if (s == MAX_SEC)
          errexit("too many sectors");

      SECHDR *h = &hdr[s++]; // ���������� ���������
      h->id = trkd + i + 2; // ��������� �� ��������� �������
      h->c = h->id[0];
      h->s = h->id[1];
      h->n = h->id[2];
      h->l = h->id[3];
      h->crc = *(unsigned short*)(trkd+i+6);
      h->c1 = (wd93_crc(trkd+i+1, 5) == h->crc);
      h->data = 0;
      h->datlen = 0;
      h->wp_start = 0;
//      if (h->l > 5) continue; [vv]

      unsigned end = min(trklen - 8, i + 8 + 43); // 43-DD, 30-SD

      // ������������ ��������� �� ���� ������ �������
      for (unsigned j = i + 8; j < end; j++)
      {
         if (trkd[j] != 0xA1 || !test_i(j) || test_i(j+1))
             continue;

         if (trkd[j+1] == 0xF8 || trkd[j+1] == 0xFB) // ������ data am
         {
            h->datlen = 128 << (h->l & 3); // [vv] FD1793 use only 2 lsb of sector size code
            h->data = trkd + j + 2;
            h->c2 = (wd93_crc(h->data-1, h->datlen+1) == *(unsigned short*)(h->data+h->datlen));

            if(trkwp)
            {
                for(unsigned b = 0; b < h->datlen; b++)
                {
                    if(test_wp(j + 2 + b))
                    {
                        h->wp_start = j + 2; // ���� ������ ���� ������� ����
                        break;
                    }
                }
            }
         }
         break;
      }
   }
}

void TRKCACHE::format()
{
   memset(trkd, 0, trklen);
   memset(trki, 0, unsigned(trklen + 7U) >> 3);
   memset(trkwp, 0, unsigned(trklen + 7U) >> 3);

   unsigned char *dst = trkd;

   unsigned i;

   //6250-6144=106
   //gap4a(80)+sync0(12)+iam(3)+1+s*(gap1(50)+sync1(12)+idam(3)+1+4+2+gap2(22)+sync2(12)+data_am(3)+1+2)
   unsigned gap4a = 80;
   unsigned sync0 = 12;
   unsigned i_am = 3;
   unsigned gap1 = 40;
   unsigned sync1 = 12;
   unsigned id_am = 3;
   unsigned gap2 = 22;
   unsigned sync2 = 12;
   unsigned data_am = 3;

   unsigned data_sz = 0;
   for (unsigned is = 0; is < s; is++)
   {
      SECHDR *sechdr = hdr + is;
      data_sz += (128 << (sechdr->l & 3)); // n
   }

   if((gap4a+sync0+i_am+1+data_sz+s*(gap1+sync1+id_am+1+4+2+gap2+sync2+data_am+1+2)) >= MAX_TRACK_LEN)
   { // ���������� ����������� ����� �������, ��������� ��������� �� �����������
       gap4a = 1;
       sync0 = 1;
       i_am = 1;
       gap1 = 1;
       sync1 = 1;
       id_am = 1;
       gap2 = 1;
       sync2 = 1;
       data_am = 1;
   }

   memset(dst, 0x4E, gap4a); dst += gap4a; // gap4a
   memset(dst, 0, sync0); dst += sync0; //sync

   for (i = 0; i < i_am; i++) // iam
       write(dst++ - trkd, 0xC2, 1);
   *dst++ = 0xFC;

   for (unsigned is = 0; is < s; is++)
   {
      memset(dst, 0x4E, gap1); dst += gap1; // gap1 // 50 [vv] // fixme: recalculate gap1 only for non standard formats
      memset(dst, 0, sync1); dst += sync1; //sync
      for (i = 0; i < id_am; i++) // idam
          write(dst++ - trkd, 0xA1, 1);
      *dst++ = 0xFE;

      SECHDR *sechdr = hdr + is;
      *dst++ = sechdr->c; // c
      *dst++ = sechdr->s; // h
      *dst++ = sechdr->n; // s
      *dst++ = sechdr->l; // n

      unsigned crc = wd93_crc(dst-5, 5); // crc
      if (sechdr->c1 == 1)
          crc = sechdr->crc;
      if (sechdr->c1 == 2)
          crc ^= 0xFFFF;
      *(unsigned*)dst = crc;
      dst += 2;

      if (sechdr->data)
      {
         memset(dst, 0x4E, gap2); dst += gap2; // gap2
         memset(dst, 0, sync2); dst += sync2; //sync
         for (i = 0; i < data_am; i++) // data am
             write(dst++ - trkd, 0xA1, 1);
         *dst++ = 0xFB;

//         if (sechdr->l > 5) errexit("strange sector"); // [vv]
         unsigned len = 128 << (sechdr->l & 3); // data
         if (sechdr->data != (unsigned char*)1)
         {
             memcpy(dst, sechdr->data, len);
             if(sechdr->wp) // ����������� ������� ����� ������� ������
             {
                 unsigned wp_start = dst - trkd;
                 sechdr->wp_start = wp_start;
                 for(unsigned b = 0; b < len; b++)
                 {
                     if(test_bit(sechdr->wp, b))
                     {
                         set_wp(wp_start + b);
                     }
                 }
             }
         }
         else
             memset(dst, 0, len);

         crc = wd93_crc(dst-1, len+1); // crc
         if (sechdr->c2 == 1)
             crc = sechdr->crcd;
         if (sechdr->c2 == 2)
             crc ^= 0xFFFF;
         *(unsigned*)(dst+len) = crc;
             dst += len+2;
      }
   }
   if (dst > trklen + trkd)
   {
       printf("cyl=%u, h=%u, additional len=%u\n", cyl, side, dst - (trklen + trkd));
       errexit("track too long");
   }
   while (dst < trkd + trklen)
       *dst++ = 0x4E;
}

#if 1
void TRKCACHE::dump()
{
   printf("\n%d/%d:", cyl, side);
   if (!trkd) { printf("<e>"); return; }
   if (!sf) { printf("<n>"); return; }
   for (unsigned i = 0; i < s; i++)
      printf("%c%02X-%02X-%02X-%02X,%c%c%c", i?' ':'<', hdr[i].c,hdr[i].s,hdr[i].n,hdr[i].l, hdr[i].c1?'+':'-', hdr[i].c2?'+':'-', hdr[i].data?'d':'h');
   printf(">");
}
#endif

int TRKCACHE::write_sector(unsigned sec, unsigned char *data)
{
   const SECHDR *h = get_sector(sec);
   if (!h || !h->data)
       return 0;
   unsigned sz = h->datlen;
   if(h->data != data)
      memcpy(h->data, data, sz);
   *(unsigned short*)(h->data+sz) = (unsigned short)wd93_crc(h->data-1, sz+1);
   return sz;
}

const SECHDR *TRKCACHE::get_sector(unsigned sec) const
{
   unsigned i;
   for (i = 0; i < s; i++)
   {
      if (hdr[i].n == sec)
          break;
   }
   if (i == s)
       return 0;

//   dump();

   if (/*(hdr[i].l & 3) != 1 ||*/ hdr[i].c != cyl) // [vv]
       return 0;
   return &hdr[i];
}


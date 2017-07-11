#include "std.h"

#include "emul.h"
#include "vars.h"
#include "memory.h"
#include "draw.h"

void atm_memswap()
{
   if (!conf.atm.mem_swap) return;
   // swap memory address bits A5-A7 and A8-A10
   for (unsigned start_page = 0; start_page < conf.ramsize*1024; start_page += 2048) {
      unsigned char buffer[2048], *bank = memory + start_page;
      for (unsigned addr = 0; addr < 2048; addr++)
         buffer[addr] = bank[(addr & 0x1F) + ((addr >> 3) & 0xE0) + ((addr << 3) & 0x700)];
      memcpy(bank, buffer, 2048);
   }
}

void AtmApplySideEffectsWhenChangeVideomode(unsigned char val)
{
    int NewVideoMode = (val & 7);
    int OldVideoMode = (comp.pFF77 & 7);

    // ��������� ����� ������ �����, ������ ��� ����� �������� ���2 ��� �� ��������.
    const int tScanlineWidth = 224;
    const int tScreenWidth = 320/2;
    const int tEachBorderWidth = (tScanlineWidth - tScreenWidth)/2;
    const int iLinesAboveBorder = 56;

    int iRayLine = cpu.t / tScanlineWidth;
    int iRayOffset = cpu.t % tScanlineWidth;
/*    
    static int iLastLine = 0;
    if ( iLastLine > iRayLine )
    {
        printf("\nNew Frame Begin\n");
        __debugbreak();
    }
    iLastLine = iRayLine;
    printf("%d->%d %d %d\n", OldVideoMode, NewVideoMode, iRayLine, iRayOffset);
*/

    if (OldVideoMode == 3 || NewVideoMode == 3)
    {
        // ������������ ��/� sinclair ����� �� ����� �������� �������� ��������
        // (�� ������ AlCo ���� ������ ���������)
        for(unsigned y = 0; y < 200; y++)
        {
            AtmVideoCtrl.Scanlines[y+56].VideoMode = NewVideoMode;
            AtmVideoCtrl.Scanlines[y+56].Offset = ((y & ~7) << 3) + 0x01C0;
        }
        return;
    }

    if (OldVideoMode != 6 && NewVideoMode != 6)
    {
        // ��� ���������� ������ ������������ ����� ��������� ������� � ������������ ������������.
        // ���������� ������ ��� �� ��, �� ����� ������������. 
        // �������������, ��� � �������� ��������.
        
        // �������������� ����� ���������� �� �������������� ���������
        if (iRayOffset >= tEachBorderWidth)
            ++iRayLine;

        while (iRayLine < 256)
        {
            AtmVideoCtrl.Scanlines[iRayLine++].VideoMode = NewVideoMode;
        }
//        printf("%d->%d SKIPPED!\n",  OldVideoMode, NewVideoMode);
        return;
    }

    //
    // ������������ � ���������:
    // ����� �������� 312 ���������, �� 224����� (noturbo) � ������.
    //
    //  "������" - ������� 3 ���� ������ �������������� ����������������� (�����) ���������
    //  "�����"  - ������� ����� � �����������, � �������� �������� ����� ���������������
    //             ����� �������� � �������������� +8 ��� +64
    //  "�����"  - ��������� ��������. �� ���� ������ �������� "��������".
    //             ������� �������: 
    //          ������ � ����� �� ������: 56 ���������
    //              ����� � ������ �� ������: 32 ����� (64�������).
    // 
    // +64 ����������, ����� CROW 1->0, �� ����:
    //  ���� � ��������� ��� �������� �� ������ 7 �� ������ 0,
    //  ���� ��� ������������ ��������->������� ��� ������ A5=0,
    //  ���� ��� ������������ �������->�������� ��� ������ A5=1 � ������ 0..3
    //
    // +8 ���������� �� ������ � ����� 64-����� �������� (������ 32�����) ���������� �� ������
    // (�����: +8 �� ���������� ������ A3..A5 � ���������� ������� �������� � �������.)
    //
    // ����� A3..A5 (����������� +8) ����������, ����� RCOL 1->0, �� ����:
    //  ���� � ��������� ��� �������� � ������ �� ������,
    //  ���� �� ������� ��� ������������ �������->��������
    //


    if (iRayLine >= 256)
    {
        return;
    }

    // ������� ������ ������� ��������� (���������� ���������� � ������)
    int Offset = AtmVideoCtrl.Scanlines[iRayLine].Offset;

    // �������� �������� ����������, � ������ ����������� ��� ��������� ������.
    // ����� ���������, ���� ��������� +64 ���������� ��� ������������ �����������:
    //  - ���� ��� �� ������� ������ �� ������ ��� �� ������� ����� �� ������ - �������� ������ ��� ������� ���������
    //  - ���� ��� �� ������ ��� �� ������� ������ �� ������ - �������� ������ ��� ��������� ���������
    bool bRayOnBorder = true;
    if ( iRayLine < iLinesAboveBorder || iRayOffset < tEachBorderWidth )
    {
        // ��� �� �������. ���� ������ �� ������, ���� ����� �� ������.
        // ��� ��������� ��������� � ������� ���������.

        // ���������� ������������ �����������.
        if ( NewVideoMode == 6 )
        {
            // ������������ � ��������� �����.
            if ( (Offset & 32) //< �������� ������� "��� ������ A5=1"
                 && (iRayLine & 7) < 4 //< �������� ������� "� ������ 0..3"
               )
            {
//                printf("CASE-1: 0x%.4x Incremented (+64) on line %d\n", Offset, iRayLine);
                Offset += 64;
                AtmVideoCtrl.Scanlines[iRayLine].Offset = Offset;
            }

            AtmVideoCtrl.Scanlines[iRayLine].VideoMode = NewVideoMode;

            // ����� ������� ���� ����� ������ ������� ��������� � ��������� ������ ����� �������� A3..A5
            Offset &= (~0x38); // ����� A3..A5
//            printf("CASE-1a, reset A3..A5: 0x%.4x\n", Offset);
            
            // ������� ������������ ��� ���������� ���������
            while (++iRayLine < 256)
            {
                if ( 0 == (iRayLine & 7))
                {
                    Offset += 64;
                }
                AtmVideoCtrl.Scanlines[iRayLine].Offset = Offset;
                AtmVideoCtrl.Scanlines[iRayLine].VideoMode = NewVideoMode;
            }
        } else {
            // ������������ �� ���������� ������.
            if ( 0 == (Offset & 32) ) //< �������� ������� "��� ������ A5=0"
            {
//                printf("CASE-2: 0x%.4x Incremented (+64) on line %d\n", Offset, iRayLine);
                Offset += 64;
                AtmVideoCtrl.Scanlines[iRayLine].Offset = Offset;
            }
            AtmVideoCtrl.Scanlines[iRayLine].VideoMode = NewVideoMode;

            // ������� ������������ ��� ���������� ���������
            while (++iRayLine < 256)
            {
                AtmVideoCtrl.Scanlines[iRayLine].Offset = Offset;
                AtmVideoCtrl.Scanlines[iRayLine].VideoMode = NewVideoMode;
            }
        }
    } else {
        // ��� ������ �����, ���� ������ ������ �� ������.

        // ��������� ������� �������� �����������

        // ���������� � ����������� ��� +64 ����������, 
        // ��������� � ���� ��������� ������ ������ ���������
        if (iRayLine == AtmVideoCtrl.CurrentRayLine)
        {
            Offset += AtmVideoCtrl.IncCounter_InRaster;
        } else {
            // ������� ����������� ������� (�.�. �������� �� ��������� �������� �� �������)
            // �������������� ��� ��� ������� ���������.
            AtmVideoCtrl.CurrentRayLine = iRayLine;
            AtmVideoCtrl.IncCounter_InRaster = 0;
            AtmVideoCtrl.IncCounter_InBorder = 0;
        }
        
        // ���������� � ����������� ��� +8 ����������, ������������ ��� ��������� ������
        bool bRayInRaster = iRayOffset < (tScreenWidth + tEachBorderWidth);
        
        int iScanlineRemainder = 0; //< ������� +8 ����������� ��� ����� ������� �� ����� ��������� 
                                    //  (�.�. ��� ����� ������������ �����������)
        if ( bRayInRaster )
        {
            // ��� ������ �����. 
            // ���������� � �������� ����������� ������� +8, 
            // ������� ���� ��������� ������������ 64���������� �����.
            int iIncValue = 8 * ((iRayOffset-tEachBorderWidth)/32);
            iScanlineRemainder = 40 - iIncValue;
//            printf("CASE-4: 0x%.4x Incremented (+%d) on line %d\n", Offset, iIncValue, iRayLine);
            Offset += iIncValue;
        } else {
            // ��������� ������ ����� ���������.
            // �.�. ��� 5-��� 64-���������� ����� ���� ��������. ���������� � ������ +40.
//            printf("CASE-5: 0x%.4x Incremented (+40) on line %d\n", Offset, iRayLine);
            Offset += 40;

            // ���� ���������� ������� ��� ��������� �����,
            // �� ��� �������� � ������ �� ������ ������ ���� �������� A3..A5
            if (OldVideoMode == 6)
            {
                Offset &= (~0x38); // ����� A3..A5
//                printf("CASE-5a, reset A3..A5: 0x%.4x\n", Offset);
            }
        }

        // ���������� � ����������� ��� +64 ����������, 
        // ��������� � ���� ��������� ������� �� ������� ������ ���������
        Offset += AtmVideoCtrl.IncCounter_InBorder;

        // ������� �������� ����������� ���������. 
        // ������������ ������������ �����������.
        int OffsetInc = 0;
        if ( NewVideoMode == 6 )
        {
            // ������������ � ��������� �����.
            if ( (Offset & 32) //< �������� ������� "��� ������ A5=1"
                && (iRayLine & 7) < 4 //< �������� ������� "� ������ 0..3"
                )
            {
                OffsetInc = 64;
//                printf("CASE-6: 0x%.4x Incremented (+64) on line %d\n", Offset, iRayLine);
                Offset += OffsetInc;
            }
            // ������� ������������ ��� ���������� ���������
            Offset += iScanlineRemainder;
            while (++iRayLine < 256)
            {
                if ( 0 == (iRayLine & 7))
                    Offset += 64;
                AtmVideoCtrl.Scanlines[iRayLine].Offset = Offset;
                AtmVideoCtrl.Scanlines[iRayLine].VideoMode = NewVideoMode;
            }
        } else {
            // ������������ �� ���������� ������.
            if ( 0 == (Offset & 32) ) //< �������� ������� "��� ������ A5=0"
            {
                OffsetInc = 64;
//                printf("CASE-7: 0x%.4x Incremented (+64) on line %d\n", Offset, iRayLine);
                Offset += OffsetInc;
            }

            // ������� ������������ ��� ���������� ���������
            Offset += iScanlineRemainder;
            while (++iRayLine < 256)
            {
                AtmVideoCtrl.Scanlines[iRayLine].Offset = Offset;
                AtmVideoCtrl.Scanlines[iRayLine].VideoMode = NewVideoMode;
                Offset += 40;
            }
        }

        // ���������� ��������� ��������� �� ������, 
        // ���� �� ����� ��������� � ���� ��������� ������� ���������.
        if ( bRayInRaster )
        {
            AtmVideoCtrl.IncCounter_InRaster += OffsetInc;
        } else {
            AtmVideoCtrl.IncCounter_InBorder += OffsetInc;
        }
    }
}

void set_atm_FF77(unsigned port, unsigned char val)
{
   if ((comp.pFF77 ^ val) & 1)
       atm_memswap();
   

   if ((comp.pFF77 & 7) ^ (val & 7))
   {
        // ���������� ������������ �����������
        AtmApplySideEffectsWhenChangeVideomode(val);
   }

   comp.pFF77 = val;
   comp.aFF77 = port;
   cpu.int_gate = (comp.pFF77 & 0x20) != false;
   set_banks();
}

void set_atm_aFE(unsigned char addr)
{
   unsigned char old_aFE = comp.aFE;
   comp.aFE = addr;
   if ((addr ^ old_aFE) & 0x40) atm_memswap();
   if ((addr ^ old_aFE) & 0x80) set_banks();
}

static u8 atm_pal[0x10] = { 0 };

void atm_writepal(unsigned char val)
{
   assert(comp.border_attr < 0x10);
   atm_pal[comp.border_attr] = val;
   comp.comp_pal[comp.border_attr] = t.atm_pal_map[val];
   temp.comp_pal_changed = 1;
}

u8 atm_readpal()
{
   return atm_pal[comp.border_attr];
}

unsigned char atm450_z(unsigned t)
{
   // PAL hardware gives 3 zeros in secret short time intervals
   if (conf.frame < 80000) { // NORMAL SPEED mode
      if ((unsigned)(t-7200) < 40 || (unsigned)(t-7284) < 40 || (unsigned)(t-7326) < 40) return 0;
   } else { // TURBO mode
      if ((unsigned)(t-21514) < 40 || (unsigned)(t-21703) < 80 || (unsigned)(t-21808) < 40) return 0;
   }
   return 0x80;
}
